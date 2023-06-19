// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/help_app_ui/search/search_tag_registry.h"

#include "ash/webui/help_app_ui/search/search.mojom.h"
#include "ash/webui/help_app_ui/search/search_metadata.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/local_search_service/public/mojom/index.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::help_app {

namespace {

class FakeObserver : public SearchTagRegistry::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

 private:
  // SearchTagRegistry::Observer:
  void OnRegistryUpdated() override { ++num_calls_; }

  size_t num_calls_ = 0;
};

}  // namespace

class HelpAppSearchTagRegistryTest : public testing::Test {
 protected:
  HelpAppSearchTagRegistryTest()
      : search_tag_registry_(local_search_service_proxy_.get()) {}

  ~HelpAppSearchTagRegistryTest() override = default;

  // testing::Test:
  void SetUp() override {
    search_tag_registry_.AddObserver(&observer_);

    local_search_service_proxy_->GetIndex(
        local_search_service::IndexId::kHelpAppLauncher,
        local_search_service::Backend::kLinearMap,
        index_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override { search_tag_registry_.RemoveObserver(&observer_); }

  void IndexGetSizeAndCheckResults(uint32_t expected_num_items) {
    bool callback_done = false;
    uint32_t num_items = 0;
    index_remote_->GetSize(base::BindOnce(
        [](bool* callback_done, uint32_t* num_items, uint64_t size) {
          *callback_done = true;
          *num_items = size;
        },
        &callback_done, &num_items));
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(callback_done);
    EXPECT_EQ(num_items, expected_num_items);
  }

  // This line should be before search_tag_registry_ is declared.
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_ =
          std::make_unique<local_search_service::LocalSearchServiceProxy>(
              /*for_testing=*/true);
  SearchTagRegistry search_tag_registry_;
  FakeObserver observer_;
  mojo::Remote<local_search_service::mojom::Index> index_remote_;
};

TEST_F(HelpAppSearchTagRegistryTest, AddAndGet) {
  // Should be empty to start with.
  IndexGetSizeAndCheckResults(0u);
  EXPECT_EQ(0u, observer_.num_calls());

  // Add things to the registry.
  std::vector<mojom::SearchConceptPtr> to_add;
  mojom::SearchConceptPtr new_concept_1 = mojom::SearchConcept::New(
      /*id=*/"test-id-1",
      /*title=*/u"Title 1",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"Test tag", u"Tag 2"},
      /*tag_locale=*/"en",
      /*url_path_with_parameters=*/"help",
      /*locale=*/"");
  mojom::SearchConceptPtr new_concept_2 = mojom::SearchConcept::New(
      /*id=*/"test-id-2",
      /*title=*/u"Title 2",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"Another test tag"},
      /*tag_locale=*/"en",
      /*url_path_with_parameters=*/"help",
      /*locale=*/"");
  to_add.push_back(std::move(new_concept_1));
  to_add.push_back(std::move(new_concept_2));

  bool callback_done = false;
  search_tag_registry_.Update(
      to_add, base::BindOnce([](bool* callback_done) { *callback_done = true; },
                             &callback_done));
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(callback_done);
  IndexGetSizeAndCheckResults(2u);
  EXPECT_EQ(1u, observer_.num_calls());

  // Get tag metadata for something that exists.
  auto& result1 = search_tag_registry_.GetTagMetadata("test-id-1");
  EXPECT_EQ(result1.title, u"Title 1");

  // Get tag metadata for something that doesn't exist.
  auto& result2 = search_tag_registry_.GetTagMetadata("not-found");
  EXPECT_EQ(&result2, &SearchTagRegistry::not_found_);
}

TEST_F(HelpAppSearchTagRegistryTest, MultipleUpdate) {
  // Add things to the registry.
  std::vector<mojom::SearchConceptPtr> to_add;
  mojom::SearchConceptPtr new_concept_1 = mojom::SearchConcept::New(
      /*id=*/"test-id-1",
      /*title=*/u"Title 1",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"Test tag", u"Tag 2"},
      /*tag_locale=*/"en",
      /*url_path_with_parameters=*/"help",
      /*locale=*/"");
  mojom::SearchConceptPtr new_concept_2 = mojom::SearchConcept::New(
      /*id=*/"test-id-2",
      /*title=*/u"Title 2",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"Another test tag"},
      /*tag_locale=*/"en",
      /*url_path_with_parameters=*/"help",
      /*locale=*/"");
  to_add.push_back(std::move(new_concept_1));
  to_add.push_back(std::move(new_concept_2));

  search_tag_registry_.Update(to_add, base::BindOnce([]() {}));
  task_environment_.RunUntilIdle();

  IndexGetSizeAndCheckResults(2u);
  EXPECT_EQ(1u, observer_.num_calls());

  // The second update has a concept that matches an existing id.
  std::vector<mojom::SearchConceptPtr> to_add_2;
  mojom::SearchConceptPtr new_concept_3 = mojom::SearchConcept::New(
      /*id=*/"test-id-1",  // Matches concept 1.
      /*title=*/u"Title 3",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"Test tag"},
      /*tag_locale=*/"en",
      /*url_path_with_parameters=*/"help",
      /*locale=*/"");
  mojom::SearchConceptPtr new_concept_4 = mojom::SearchConcept::New(
      /*id=*/"test-id-4",
      /*title=*/u"Title 4",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"Another test tag"},
      /*tag_locale=*/"en",
      /*url_path_with_parameters=*/"help",
      /*locale=*/"");
  to_add_2.push_back(std::move(new_concept_3));
  to_add_2.push_back(std::move(new_concept_4));

  search_tag_registry_.Update(to_add_2, base::BindOnce([]() {}));
  task_environment_.RunUntilIdle();

  IndexGetSizeAndCheckResults(3u);
  EXPECT_EQ(2u, observer_.num_calls());

  // The later concept should replace the earlier concept.
  auto& result = search_tag_registry_.GetTagMetadata("test-id-1");
  EXPECT_EQ(result.title, u"Title 3");
}

}  // namespace ash::help_app

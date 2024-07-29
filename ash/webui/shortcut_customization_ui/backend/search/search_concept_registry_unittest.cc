// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/search/search_concept_registry.h"
#include <vector>

#include "ash/public/mojom/accelerator_info.mojom-shared.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ash/webui/shortcut_customization_ui/backend/search/fake_search_data.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search_concept.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/ash/components/local_search_service/public/mojom/index.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash::shortcut_ui {

namespace {
class FakeObserver : public SearchConceptRegistry::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

 private:
  // SearchConceptRegistry::Observer:
  void OnRegistryUpdated() override { ++num_calls_; }

  size_t num_calls_ = 0;
};

}  // namespace

class SearchConceptRegistryTest : public testing::Test {
 protected:
  SearchConceptRegistryTest()
      : search_concept_registry_(*local_search_service_proxy_.get()) {}

  ~SearchConceptRegistryTest() override = default;

  // testing::Test:
  void SetUp() override {
    search_concept_registry_.AddObserver(&observer_);

    local_search_service_proxy_->GetIndex(
        local_search_service::IndexId::kShortcutsApp,
        local_search_service::Backend::kLinearMap,
        index_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    search_concept_registry_.RemoveObserver(&observer_);
  }

  // Get the size of the LSS index and assert that the size is equal to
  // the expected value.
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

  // This line should be before search_concept_registry_ is declared.
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_ =
          std::make_unique<local_search_service::LocalSearchServiceProxy>(
              /*for_testing=*/true);
  SearchConceptRegistry search_concept_registry_;
  FakeObserver observer_;
  mojo::Remote<local_search_service::mojom::Index> index_remote_;
};

TEST_F(SearchConceptRegistryTest, AddAndRemove) {
  // The index should be empty.
  IndexGetSizeAndCheckResults(0u);
  // The observer should not have been called yet.
  EXPECT_EQ(0u, observer_.num_calls());

  // Create a list of fake SearchConcepts for this test.
  std::vector<SearchConcept> search_concept_list;
  search_concept_list.emplace_back(
      fake_search_data::CreateFakeAcceleratorLayoutInfo(
          /*description=*/u"Open launcher",
          /*source=*/ash::mojom::AcceleratorSource::kAsh,
          /*action=*/fake_search_data::FakeActionIds::kAction1,
          /*style=*/ash::mojom::AcceleratorLayoutStyle::kDefault),
      fake_search_data::CreateFakeAcceleratorInfoList());
  // Save the ID of this SearchConcept for later on.
  const std::string first_search_concept_id = search_concept_list.back().id;
  search_concept_list.emplace_back(
      fake_search_data::CreateFakeAcceleratorLayoutInfo(
          /*description=*/u"Open new tab",
          /*source=*/ash::mojom::AcceleratorSource::kBrowser,
          /*action=*/fake_search_data::FakeActionIds::kAction2,
          /*style=*/ash::mojom::AcceleratorLayoutStyle::kDefault),
      fake_search_data::CreateFakeAcceleratorInfoList());
  // Save this SearchConcept for later on.
  const std::string second_search_concept_id = search_concept_list.back().id;

  // Add SearchConcepts to the registry (and thus the index); the size of the
  // index should increase.
  const uint32_t list_size = search_concept_list.size();
  search_concept_registry_.SetSearchConcepts(std::move(search_concept_list));
  task_environment_.RunUntilIdle();
  IndexGetSizeAndCheckResults(list_size);
  // We expect the observer OnRegistryUpdated() method to have been called once.
  EXPECT_EQ(1u, observer_.num_calls());

  // SearchConcepts added should be available via GetSearchConceptById().
  const SearchConcept* first_search_concept_from_registry =
      search_concept_registry_.GetSearchConceptById(first_search_concept_id);
  ASSERT_TRUE(first_search_concept_from_registry);
  // Verify that the correct SearchConcept has been returned.
  EXPECT_EQ(
      fake_search_data::FakeActionIds::kAction1,
      first_search_concept_from_registry->accelerator_layout_info->action);
  EXPECT_EQ(
      ash::mojom::AcceleratorSource::kAsh,
      first_search_concept_from_registry->accelerator_layout_info->source);

  // Check the second SearchConcept, too.
  const SearchConcept* second_search_concept_from_registry =
      search_concept_registry_.GetSearchConceptById(second_search_concept_id);
  ASSERT_TRUE(second_search_concept_from_registry);
  // Verify that the correct SearchConcept has been returned.
  EXPECT_EQ(
      fake_search_data::FakeActionIds::kAction2,
      second_search_concept_from_registry->accelerator_layout_info->action);
  EXPECT_EQ(
      ash::mojom::AcceleratorSource::kBrowser,
      second_search_concept_from_registry->accelerator_layout_info->source);

  // Remove the first SearchConcept from the registry (and thus the index) by
  // registering only the second search concept. The size of the index should
  // decrease.
  std::vector<SearchConcept> next_search_concepts;
  next_search_concepts.emplace_back(
      fake_search_data::CreateFakeAcceleratorLayoutInfo(
          /*description=*/u"Open new tab",
          /*source=*/ash::mojom::AcceleratorSource::kBrowser,
          /*action=*/fake_search_data::FakeActionIds::kAction2,
          /*style=*/ash::mojom::AcceleratorLayoutStyle::kDefault),
      fake_search_data::CreateFakeAcceleratorInfoList());
  search_concept_registry_.SetSearchConcepts(std::move(next_search_concepts));
  task_environment_.RunUntilIdle();
  IndexGetSizeAndCheckResults(1u);
  // We expect the observer OnRegistryUpdated() method to have been called twice
  // now.

  // Verify that the first SearchConcept has been deleted.
  const SearchConcept* first_search_concept_after_deletion =
      search_concept_registry_.GetSearchConceptById(first_search_concept_id);
  ASSERT_FALSE(first_search_concept_after_deletion);

  // Verify that the second SearchConcept is still present.
  const SearchConcept* second_search_concept_after_deletion =
      search_concept_registry_.GetSearchConceptById(second_search_concept_id);
  ASSERT_TRUE(second_search_concept_after_deletion);
  EXPECT_EQ(
      fake_search_data::FakeActionIds::kAction2,
      second_search_concept_after_deletion->accelerator_layout_info->action);
  EXPECT_EQ(
      ash::mojom::AcceleratorSource::kBrowser,
      second_search_concept_after_deletion->accelerator_layout_info->source);
}

TEST_F(SearchConceptRegistryTest, SearchConceptToDataStandardAccelerator) {
  ash::mojom::AcceleratorInfoPtr first_standard_accelerator_info =
      ash::mojom::AcceleratorInfo::New(
          /*type=*/ash::mojom::AcceleratorType::kDefault,
          /*state=*/ash::mojom::AcceleratorState::kEnabled,
          /*locked=*/true,
          /*accelerator_locked=*/false,
          /*layout_properties=*/
          ash::mojom::LayoutStyleProperties::NewStandardAccelerator(
              ash::mojom::StandardAcceleratorProperties::New(
                  ui::Accelerator(
                      /*key_code=*/ui::KeyboardCode::VKEY_A,
                      /*modifiers=*/ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN),
                  u"A", std::nullopt)));
  ash::mojom::AcceleratorInfoPtr second_standard_accelerator_info =
      ash::mojom::AcceleratorInfo::New(
          /*type=*/ash::mojom::AcceleratorType::kDefault,
          /*state=*/ash::mojom::AcceleratorState::kEnabled,
          /*locked=*/true,
          /*accelerator_locked=*/false,
          /*layout_properties=*/
          ash::mojom::LayoutStyleProperties::NewStandardAccelerator(
              ash::mojom::StandardAcceleratorProperties::New(
                  ui::Accelerator(
                      /*key_code=*/ui::KeyboardCode::VKEY_BRIGHTNESS_DOWN,
                      /*modifiers=*/ui::EF_ALT_DOWN),
                  u"BrightnessDown", std::nullopt)));

  std::vector<ash::mojom::AcceleratorInfoPtr> accelerator_info_list;
  accelerator_info_list.push_back(std::move(first_standard_accelerator_info));
  accelerator_info_list.push_back(std::move(second_standard_accelerator_info));

  SearchConcept standard_search_concept = SearchConcept(
      fake_search_data::CreateFakeAcceleratorLayoutInfo(
          /*description=*/u"Open the Foobar",
          /*source=*/ash::mojom::AcceleratorSource::kAsh, /*action=*/1,
          /*style=*/ash::mojom::AcceleratorLayoutStyle::kDefault),
      std::move(accelerator_info_list));

  ash::local_search_service::Data data =
      search_concept_registry_.SearchConceptToData(standard_search_concept);

  // The overall data ID should be source + action.
  EXPECT_EQ(data.id, "0-1");
  // There should be only one contents entry for the description.
  EXPECT_EQ(data.contents.size(), 1u);
  // The first entry will always be the description of the SearchConcept.
  EXPECT_EQ(data.contents[0].id, "0-1-description");
  EXPECT_EQ(data.contents[0].content, u"Open the Foobar");
}

TEST_F(SearchConceptRegistryTest, SearchConceptToDataTextAccelerator) {
  // Construct a TextAccelerator by its parts.
  std::vector<ash::mojom::TextAcceleratorPartPtr> text_parts;
  text_parts.push_back(ash::mojom::TextAcceleratorPart::New(
      u"Press ", ash::mojom::TextAcceleratorPartType::kPlainText));
  text_parts.push_back(ash::mojom::TextAcceleratorPart::New(
      u"Ctrl", ash::mojom::TextAcceleratorPartType::kModifier));
  text_parts.push_back(ash::mojom::TextAcceleratorPart::New(
      u"+", ash::mojom::TextAcceleratorPartType::kDelimiter));
  text_parts.push_back(ash::mojom::TextAcceleratorPart::New(
      u"A", ash::mojom::TextAcceleratorPartType::kKey));

  ash::mojom::AcceleratorInfoPtr text_accelerator_info =
      ash::mojom::AcceleratorInfo::New(
          /*type=*/ash::mojom::AcceleratorType::kDefault,
          /*state=*/ash::mojom::AcceleratorState::kEnabled,
          /*locked=*/true,
          /*accelerator_locked=*/false,
          /*layout_properties=*/
          ash::mojom::LayoutStyleProperties::NewTextAccelerator(
              ash::mojom::TextAcceleratorProperties::New(
                  std::move(text_parts))));

  std::vector<ash::mojom::AcceleratorInfoPtr> accelerator_info_list;
  accelerator_info_list.push_back(std::move(text_accelerator_info));

  // Create a SearchConcept that contains that TextAccelerator.
  SearchConcept text_search_concept =
      SearchConcept(fake_search_data::CreateFakeAcceleratorLayoutInfo(
                        /*description=*/u"Select all",
                        /*source=*/ash::mojom::AcceleratorSource::kAsh,
                        /*action=*/fake_search_data::FakeActionIds::kAction1,
                        /*style=*/ash::mojom::AcceleratorLayoutStyle::kText),
                    std::move(accelerator_info_list));

  // Convert it to Data so that we can verify it has the correct properties.
  ash::local_search_service::Data data =
      search_concept_registry_.SearchConceptToData(text_search_concept);

  // The overall data ID should be source + action.
  EXPECT_EQ(data.id, "0-1");
  // There should be two entries: one for the description, and one for the
  // TextAccelerator.
  EXPECT_EQ(data.contents.size(), 2u);
  // The first entry will always be the description of the SearchConcept.
  EXPECT_EQ(data.contents[0].id, "0-1-description");
  EXPECT_EQ(data.contents[0].content, u"Select all");
  // The second entry in this case will be the accelerator info's accelerator.
  // For text accelerators, the id is always the literal "text-accelerator"
  // appended after the SearchConcept's ID.
  EXPECT_EQ(data.contents[1].id, "0-1-text-accelerator");
  EXPECT_EQ(data.contents[1].content, u"Press Ctrl+A");
}

}  // namespace ash::shortcut_ui
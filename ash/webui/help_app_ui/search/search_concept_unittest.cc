// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/help_app_ui/search/search_concept.h"

#include <cstddef>

#include "ash/webui/help_app_ui/search/search.mojom.h"
#include "ash/webui/help_app_ui/search/search_concept.pb.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::help_app::test {

namespace {

mojom::SearchConceptPtr GetFakeSearchConcept(std::string id) {
  mojom::SearchConceptPtr fake_search_concept = mojom::SearchConcept::New();
  fake_search_concept->id = id;
  fake_search_concept->title = u"title_" + base::UTF8ToUTF16(id);
  fake_search_concept->main_category =
      u"main_category_" + base::UTF8ToUTF16(id);

  return fake_search_concept;
}

}  // namespace

class SearchConceptTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath GetPath() { return temp_dir_.GetPath().Append("proto"); }

  void WriteToDisk(const SearchConceptProto& proto) {
    ASSERT_TRUE(base::WriteFile(GetPath(), proto.SerializeAsString()));
  }

  void OnRead(std::vector<mojom::SearchConceptPtr> search_concepts) {
    EXPECT_EQ(search_concepts.size(), expected_search_concepts_.size());
    for (size_t i = 0; i < search_concepts.size(); i++) {
      EXPECT_EQ(search_concepts[i]->id, expected_search_concepts_[i]->id);
    }
  }

  base::OnceCallback<void(std::vector<mojom::SearchConceptPtr>)>
  ReadCallback() {
    return base::BindOnce(&SearchConceptTest::OnRead, base::Unretained(this));
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  base::ScopedTempDir temp_dir_;

  std::vector<mojom::SearchConceptPtr> expected_search_concepts_;
};

// A dummy test to ensure that the search concept does not crash on
// initialization.
TEST_F(SearchConceptTest, Initialization) {
  SearchConcept search_concept(GetPath());
  SUCCEED();
}

// Test that search concepts can be properly save to disk and read from disk.
TEST_F(SearchConceptTest, WriteAndRead) {
  SearchConcept persistence(GetPath());

  std::vector<mojom::SearchConceptPtr> search_concepts;
  search_concepts.push_back(GetFakeSearchConcept("0"));
  search_concepts.push_back(GetFakeSearchConcept("1"));

  // save to disk.
  EXPECT_FALSE(base::PathExists(GetPath()));
  persistence.UpdateSearchConcepts(std::move(search_concepts));
  Wait();
  EXPECT_TRUE(base::PathExists(GetPath()));

  // read from disk.
  expected_search_concepts_.push_back(GetFakeSearchConcept("0"));
  expected_search_concepts_.push_back(GetFakeSearchConcept("1"));

  persistence.GetSearchConcepts(ReadCallback());
  Wait();
}

TEST_F(SearchConceptTest, ProtoDeletedOnVersionChange) {
  {
    SearchConceptProto proto;
    proto.set_version(0);
    WriteToDisk(proto);
  }

  {
    SearchConcept persistence(GetPath());
    EXPECT_TRUE(base::PathExists(GetPath()));
    persistence.GetSearchConcepts(ReadCallback());
    Wait();
    // The returned search concepts should be empty and the persistence should
    // have been wiped.
    EXPECT_FALSE(base::PathExists(GetPath()));
  }
}

}  // namespace ash::help_app::test

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/search/search_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/shortcut_customization_ui/backend/search/fake_search_data.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom-test-utils.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::shortcut_customization {
using shortcut_ui::fake_search_data::CreateFakeSearchResultList;

class SearchHandlerTest : public testing::Test {
 protected:
  SearchHandlerTest() = default;
  ~SearchHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kSearchInShortcutsApp},
        /*disabled_features=*/{});
    handler_.BindInterface(handler_remote_.BindNewPipeAndPassReceiver());
  }

  base::test::TaskEnvironment task_environment_;
  shortcut_ui::SearchHandler handler_;
  mojo::Remote<mojom::SearchHandler> handler_remote_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(cambickel): Remove this test when we use the real LocalSearchService
// index, since this test tests against the fake data that we're temporarily
// returning from SearchHandler.Search().
TEST_F(SearchHandlerTest, SearchWithFakeResults) {
  std::vector<mojom::SearchResultPtr> fake_search_results =
      shortcut_ui::fake_search_data::CreateFakeSearchResultList();
  std::vector<mojom::SearchResultPtr> search_results;
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"New tab",
              /*max_num_results=*/3u, &search_results);
  EXPECT_EQ(search_results.size(), fake_search_results.size());
  EXPECT_EQ(search_results[0]->accelerator_layout_info->description,
            fake_search_results[0]->accelerator_layout_info->description);
  EXPECT_EQ(search_results[1]->accelerator_layout_info->description,
            fake_search_results[1]->accelerator_layout_info->description);
}

}  // namespace ash::shortcut_customization
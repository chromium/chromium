// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/search/search_handler.h"
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/personalization_app/search/search.mojom-test-utils.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace personalization_app {

class PersonalizationAppSearchHandlerTest : public testing::Test {
 protected:
  PersonalizationAppSearchHandlerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ::ash::features::kPersonalizationHub);
  }

  ~PersonalizationAppSearchHandlerTest() override = default;

  void SetUp() override {
    search_handler_.BindInterface(
        search_handler_remote_.BindNewPipeAndPassReceiver());
  }

  SearchHandler* search_handler() { return &search_handler_; }

  mojo::Remote<mojom::SearchHandler>* search_handler_remote() {
    return &search_handler_remote_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  SearchHandler search_handler_;
  mojo::Remote<mojom::SearchHandler> search_handler_remote_;
};

TEST_F(PersonalizationAppSearchHandlerTest, AnswersPersonalizationQuery) {
  std::vector<mojom::SearchResultPtr> search_results;
  mojom::SearchHandlerAsyncWaiter(search_handler_remote()->get())
      .Search(u"testing", &search_results);
  EXPECT_TRUE(search_results.empty());

  std::u16string title = l10n_util::GetStringUTF16(
      IDS_PERSONALIZATION_APP_PERSONALIZATION_HUB_TITLE);
  mojom::SearchHandlerAsyncWaiter(search_handler_remote()->get())
      .Search(title, &search_results);
  EXPECT_EQ(search_results.size(), 1u);
  EXPECT_EQ(search_results.at(0)->text, title);
}

}  // namespace personalization_app
}  // namespace ash

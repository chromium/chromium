// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/tab_groups/tab_groups_page_handler.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/new_tab_page/modules/v2/tab_groups/tab_groups.mojom.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/search/ntp_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabGroupsPageHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  TabGroupsPageHandlerTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    handler_ = std::make_unique<TabGroupsPageHandler>(
        mojo::PendingReceiver<ntp::tab_groups::mojom::PageHandler>(),
        web_contents());
  }

  void TearDown() override {
    handler_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Synchronously fetches tab groups data from
  // `TabGroupsPageHandler::GetTabGroups()`. The actual mojo call is async, and
  // this helper blocks the current thread until the page handler responds to
  // achieve synchronization.
  std::vector<ntp::tab_groups::mojom::TabGroupPtr> RunGetTabGroups() {
    std::vector<ntp::tab_groups::mojom::TabGroupPtr> tab_groups_mojom;
    base::RunLoop wait_loop;
    handler_->GetTabGroups(base::BindOnce(
        [](base::OnceClosure stop_waiting,
           std::vector<ntp::tab_groups::mojom::TabGroupPtr>* tab_groups,
           std::vector<ntp::tab_groups::mojom::TabGroupPtr> tab_groups_arg) {
          *tab_groups = std::move(tab_groups_arg);
          std::move(stop_waiting).Run();
        },
        wait_loop.QuitClosure(), &tab_groups_mojom));
    wait_loop.Run();
    return tab_groups_mojom;
  }

  TabGroupsPageHandler* handler() { return handler_.get(); }

 private:
  mojo::PendingRemote<ntp::tab_groups::mojom::PageHandler> page_handler_remote_;
  std::unique_ptr<TabGroupsPageHandler> handler_;
};

TEST_F(TabGroupsPageHandlerTest, GetFakeTabGroups) {
  // Enable the feature and set the parameter to "Fake Data".
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpTabGroupsModule,
      {{ntp_features::kNtpTabGroupsModuleDataParam, "Fake Data"}});

  auto tab_groups_mojom = RunGetTabGroups();

  ASSERT_FALSE(tab_groups_mojom.empty());
  int num_tab_groups = static_cast<int>(tab_groups_mojom.size());
  for (int i = 0; i < num_tab_groups; ++i) {
    auto& tab_group = tab_groups_mojom[i];
    ASSERT_EQ("Tab Group " + base::NumberToString(i + 1), tab_group->title);
    ASSERT_EQ(GURL("https://www.google.com"), tab_group->url);
  }
}

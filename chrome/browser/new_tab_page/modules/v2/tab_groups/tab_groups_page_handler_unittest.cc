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

using TabGroupsOptional =
    std::optional<std::vector<ntp::tab_groups::mojom::TabGroupPtr>>;

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
  TabGroupsOptional RunGetTabGroups() {
    TabGroupsOptional tab_groups_mojom;
    base::RunLoop wait_loop;
    handler_->GetTabGroups(base::BindOnce(
        [](base::OnceClosure stop_waiting, TabGroupsOptional* tab_groups,
           TabGroupsOptional tab_groups_arg) {
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
  ASSERT_TRUE(tab_groups_mojom.has_value());

  const auto& tab_groups = tab_groups_mojom.value();
  ASSERT_FALSE(tab_groups.empty());

  const auto& group1 = tab_groups[0];
  EXPECT_EQ("Tab Group 1 (3 tabs total)", group1->title);
  EXPECT_EQ(3, group1->total_tab_count);
  EXPECT_EQ(3u, group1->favicon_urls.size());
  EXPECT_EQ(group1->total_tab_count,
            static_cast<int>(group1->favicon_urls.size()));
  EXPECT_EQ(GURL("https://www.google.com"), group1->favicon_urls[0]);
  EXPECT_EQ(GURL("https://www.youtube.com"), group1->favicon_urls[1]);
  EXPECT_EQ(GURL("https://www.wikipedia.org"), group1->favicon_urls[2]);

  const auto& group2 = tab_groups[1];
  EXPECT_EQ("Tab Group 2 (4 tabs total)", group2->title);
  EXPECT_EQ(4u, group2->favicon_urls.size());
  EXPECT_EQ(4, group2->total_tab_count);

  const auto& group3 = tab_groups[2];
  EXPECT_EQ("Tab Group 3 (8 tabs total)", group3->title);
  EXPECT_EQ(4u, group3->favicon_urls.size());
  EXPECT_EQ(8, group3->total_tab_count);

  const auto& group4 = tab_groups[3];
  EXPECT_EQ("Tab Group 4 (199 tabs total)", group4->title);
  EXPECT_EQ(4u, group4->favicon_urls.size());
  EXPECT_EQ(199, group4->total_tab_count);
}

TEST_F(TabGroupsPageHandlerTest, GetFakeZeroStateTabGroups) {
  // Enable the feature and set the parameter to "Fake Zero State".
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpTabGroupsModule,
      {{ntp_features::kNtpTabGroupsModuleDataParam, "Fake Zero State"}});

  auto tab_groups_mojom = RunGetTabGroups();
  ASSERT_TRUE(tab_groups_mojom.has_value());

  const auto& tab_groups = tab_groups_mojom.value();
  EXPECT_TRUE(tab_groups.empty());
}

TEST_F(TabGroupsPageHandlerTest, DismissAndRestoreModule) {
  // Enable the feature and set the parameter to "Fake Data".
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpTabGroupsModule,
      {{ntp_features::kNtpTabGroupsModuleDataParam, "Fake Data"}});

  // With no dismissal pref set we should get the fake data.
  auto initial_tab_groups = RunGetTabGroups();
  ASSERT_TRUE(initial_tab_groups.has_value());
  EXPECT_FALSE(initial_tab_groups.value().empty());

  // Call DismissModule() and subsequent GetTabGroups() must return nullopt.
  handler()->DismissModule();
  auto module_dismissed = RunGetTabGroups();
  EXPECT_FALSE(module_dismissed.has_value());

  // Call RestoreModule() and data should again be returned.
  handler()->RestoreModule();
  auto module_restored = RunGetTabGroups();
  ASSERT_TRUE(module_restored.has_value());
  EXPECT_FALSE(module_restored.value().empty());
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geic/geic_pwc_manager.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/geic/geic_browser_host_impl.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace geic {
namespace {

class GeicPwcManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        pwc::mojom::features::kPrivilegedWebContents);
    ChromeRenderViewHostTestHarness::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GeicPwcManagerTest, InitializesDistinctPwcPerTab) {
  const GURL test_url("https://localhost.corp.google.com:10443/side-panel");
  GeicPwcManager manager(profile(), test_url);

  tabs::MockTabInterface tab1;
  tabs::MockTabInterface tab2;

  content::WebContents* contents1 = manager.GetOrCreateWebContentsForTab(&tab1);
  ASSERT_TRUE(contents1);
  EXPECT_EQ(contents1->GetBrowserContext(), profile());

  content::WebContents* contents2 = manager.GetOrCreateWebContentsForTab(&tab2);
  ASSERT_TRUE(contents2);
  EXPECT_NE(contents1, contents2);

  EXPECT_EQ(manager.entry_count_for_testing(), 2u);

  pwc::PrivilegedWebContents* pwc1 = manager.GetPwcForTab(&tab1);
  pwc::PrivilegedWebContents* pwc2 = manager.GetPwcForTab(&tab2);
  ASSERT_TRUE(pwc1);
  ASSERT_TRUE(pwc2);
  EXPECT_NE(pwc1, pwc2);
  EXPECT_EQ(pwc1->web_contents(), contents1);
  EXPECT_EQ(pwc2->web_contents(), contents2);

  // Calling again on the same tab returns the existing instance:
  EXPECT_EQ(manager.GetOrCreateWebContentsForTab(&tab1), contents1);

  // Browser hosts are distinct and mapped to their respective PWCs:
  GeicBrowserHostImpl* host1 = manager.GetBrowserHostForPwc(pwc1);
  GeicBrowserHostImpl* host2 = manager.GetBrowserHostForPwc(pwc2);
  ASSERT_TRUE(host1);
  ASSERT_TRUE(host2);
  EXPECT_NE(host1, host2);

  // Cleanup tab1:
  manager.RemoveTab(tab1.GetHandle());
  EXPECT_EQ(manager.entry_count_for_testing(), 1u);
  EXPECT_FALSE(manager.GetPwcForTab(&tab1));
  EXPECT_TRUE(manager.GetPwcForTab(&tab2));
  EXPECT_FALSE(manager.GetBrowserHostForPwc(pwc1));
  EXPECT_EQ(manager.GetBrowserHostForPwc(pwc2), host2);
}

TEST_F(GeicPwcManagerTest, DoesNotInitializeWhenNoUrlConfigured) {
  GeicPwcManager manager(profile(), GURL());
  tabs::MockTabInterface tab;

  EXPECT_FALSE(manager.GetOrCreateWebContentsForTab(&tab));
  EXPECT_FALSE(manager.GetPwcForTab(&tab));
  EXPECT_EQ(manager.entry_count_for_testing(), 0u);
}

TEST_F(GeicPwcManagerTest, ConfiguresGuestURLFromCommandLine) {
  base::test::ScopedCommandLine scoped_command_line;
  const std::string custom_url =
      "https://custom.corp.google.com:10443/custom-panel";
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      kGeicGuestURLSwitch, custom_url);

  EXPECT_EQ(GeicPwcManager::GetConfiguredGuestURL(), GURL(custom_url));

  GeicPwcManager manager(profile());
  EXPECT_EQ(manager.guest_url(), GURL(custom_url));
}

TEST_F(GeicPwcManagerTest, ClearsAllEntriesOnProfileDestruction) {
  const GURL test_url("https://localhost.corp.google.com:10443/side-panel");
  GeicPwcManager manager(profile(), test_url);

  tabs::MockTabInterface tab1;
  tabs::MockTabInterface tab2;
  manager.GetOrCreateWebContentsForTab(&tab1);
  manager.GetOrCreateWebContentsForTab(&tab2);
  EXPECT_EQ(manager.entry_count_for_testing(), 2u);

  manager.OnProfileWillBeDestroyed(profile());
  EXPECT_EQ(manager.entry_count_for_testing(), 0u);
}

}  // namespace
}  // namespace geic

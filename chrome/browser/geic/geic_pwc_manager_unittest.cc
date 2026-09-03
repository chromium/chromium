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
#include "chrome/browser/geic/geic_enabling.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
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

    mock_browser_window_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
    tab_strip_model_delegate_.SetBrowserWindowInterface(
        mock_browser_window_interface_.get());
    tab_strip_model_ =
        std::make_unique<TabStripModel>(&tab_strip_model_delegate_, profile());
    TabStripModel* strip = tab_strip_model_.get();
    ON_CALL(*mock_browser_window_interface_, GetTabStripModel())
        .WillByDefault(testing::Return(strip));
    ON_CALL(*mock_browser_window_interface_, GetActiveTabInterface())
        .WillByDefault([strip]() -> tabs::TabInterface* {
          return strip ? strip->GetActiveTab() : nullptr;
        });
    ON_CALL(*mock_browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile()));
    ON_CALL(*mock_browser_window_interface_, GetSessionID())
        .WillByDefault(testing::ReturnRef(session_id_));
  }

  void TearDown() override {
    tab_strip_model_->CloseAllTabs();
    tab_strip_model_.reset();
    tab_strip_model_delegate_.SetBrowserWindowInterface(nullptr);
    mock_browser_window_interface_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  tabs::TabInterface* AddTab(const GURL& url) {
    std::unique_ptr<content::WebContents> contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    content::WebContents* raw_contents = contents.get();
    tab_strip_model_->AppendWebContents(std::move(contents),
                                        /*foreground=*/true);
    content::NavigationSimulator::NavigateAndCommitFromBrowser(raw_contents,
                                                               url);
    return tab_strip_model_->GetActiveTab();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  tabs::TabModel::PreventFeatureInitializationForTesting prevent_tab_features_;
  SessionID session_id_ = SessionID::FromSerializedValue(1);
  std::unique_ptr<testing::NiceMock<MockBrowserWindowInterface>>
      mock_browser_window_interface_;
  TestTabStripModelDelegate tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
};

TEST_F(GeicPwcManagerTest, InitializesDistinctPwcPerTab) {
  tabs::TabInterface* tab1 = AddTab(GURL("https://example.com/tab1"));
  tabs::TabInterface* tab2 = AddTab(GURL("https://example.com/tab2"));

  const GURL test_url("https://localhost.corp.google.com:10443/side-panel");
  GeicPwcManager manager(profile(), test_url);

  content::WebContents* contents1 = manager.GetOrCreateWebContentsForTab(tab1);
  ASSERT_TRUE(contents1);
  EXPECT_EQ(contents1->GetBrowserContext(), profile());

  content::WebContents* contents2 = manager.GetOrCreateWebContentsForTab(tab2);
  ASSERT_TRUE(contents2);
  EXPECT_NE(contents1, contents2);

  EXPECT_EQ(manager.entry_count_for_testing(), 2u);

  pwc::PrivilegedWebContents* pwc1 = manager.GetPwcForTab(tab1);
  pwc::PrivilegedWebContents* pwc2 = manager.GetPwcForTab(tab2);
  ASSERT_TRUE(pwc1);
  ASSERT_TRUE(pwc2);
  EXPECT_NE(pwc1, pwc2);
  EXPECT_EQ(pwc1->web_contents(), contents1);
  EXPECT_EQ(pwc2->web_contents(), contents2);

  // Calling again on the same tab returns the existing instance:
  EXPECT_EQ(manager.GetOrCreateWebContentsForTab(tab1), contents1);

  // Browser hosts are distinct and mapped to their respective PWCs:
  GeicBrowserHostImpl* host1 = manager.GetBrowserHostForPwc(pwc1);
  GeicBrowserHostImpl* host2 = manager.GetBrowserHostForPwc(pwc2);
  ASSERT_TRUE(host1);
  ASSERT_TRUE(host2);
  EXPECT_NE(host1, host2);

  // Cleanup tab1 and tab2:
  manager.RemoveTab(tab1->GetHandle());
  EXPECT_EQ(manager.entry_count_for_testing(), 1u);
  EXPECT_FALSE(manager.GetPwcForTab(tab1));
  EXPECT_TRUE(manager.GetPwcForTab(tab2));
  EXPECT_FALSE(manager.GetBrowserHostForPwc(pwc1));
  EXPECT_EQ(manager.GetBrowserHostForPwc(pwc2), host2);

  manager.RemoveTab(tab2->GetHandle());
  EXPECT_EQ(manager.entry_count_for_testing(), 0u);
  EXPECT_FALSE(manager.GetPwcForTab(tab2));
  EXPECT_FALSE(manager.GetBrowserHostForPwc(pwc2));
}

TEST_F(GeicPwcManagerTest, DoesNotInitializeWhenNoUrlConfigured) {
  tabs::TabInterface* tab = AddTab(GURL("https://example.com/tab"));
  GeicPwcManager manager(profile(), GURL());

  EXPECT_FALSE(manager.GetOrCreateWebContentsForTab(tab));
  EXPECT_FALSE(manager.GetPwcForTab(tab));
  EXPECT_EQ(manager.entry_count_for_testing(), 0u);
}

TEST_F(GeicPwcManagerTest, ConfiguresGuestURLFromCommandLine) {
  base::test::ScopedCommandLine scoped_command_line;
  const std::string custom_url =
      "https://custom.corp.google.com:10443/custom-panel";
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      kGeicGuestURLSwitch, custom_url);

  EXPECT_EQ(GeicPwcManager::GetConfiguredGuestURL(nullptr), GURL(custom_url));

  GeicPwcManager manager(profile());
  EXPECT_EQ(manager.guest_url(), GURL(custom_url));
}

TEST_F(GeicPwcManagerTest, ConfiguresGuestURLFromFeatureParam) {
  base::test::ScopedFeatureList feature_list;
  const std::string param_url =
      "https://param.corp.google.com:10443/param-panel";
  feature_list.InitAndEnableFeatureWithParameters(
      features::kGeic, {{features::kGeicGuestURL.name, param_url}});

  EXPECT_EQ(GeicPwcManager::GetConfiguredGuestURL(nullptr), GURL(param_url));

  GeicPwcManager manager(profile());
  EXPECT_EQ(manager.guest_url(), GURL(param_url));
}

TEST_F(GeicPwcManagerTest, CommandLineOverridesFeatureParam) {
  base::test::ScopedFeatureList feature_list;
  const std::string param_url =
      "https://param.corp.google.com:10443/param-panel";
  feature_list.InitAndEnableFeatureWithParameters(
      features::kGeic, {{features::kGeicGuestURL.name, param_url}});

  base::test::ScopedCommandLine scoped_command_line;
  const std::string custom_url =
      "https://custom.corp.google.com:10443/custom-panel";
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      kGeicGuestURLSwitch, custom_url);

  EXPECT_EQ(GeicPwcManager::GetConfiguredGuestURL(nullptr), GURL(custom_url));

  GeicPwcManager manager(profile());
  EXPECT_EQ(manager.guest_url(), GURL(custom_url));
}

TEST_F(GeicPwcManagerTest, ClearsAllEntriesOnProfileDestruction) {
  tabs::TabInterface* tab1 = AddTab(GURL("https://example.com/tab1"));
  tabs::TabInterface* tab2 = AddTab(GURL("https://example.com/tab2"));

  const GURL test_url("https://localhost.corp.google.com:10443/side-panel");
  GeicPwcManager manager(profile(), test_url);

  manager.GetOrCreateWebContentsForTab(tab1);
  manager.GetOrCreateWebContentsForTab(tab2);
  EXPECT_EQ(manager.entry_count_for_testing(), 2u);

  manager.OnProfileWillBeDestroyed(profile());
  EXPECT_EQ(manager.entry_count_for_testing(), 0u);
}

TEST_F(GeicPwcManagerTest, IsValidGuestUrlValidatesAllowedOriginsAndSchemes) {
  EXPECT_FALSE(IsValidGuestUrl(GURL()));
  EXPECT_FALSE(IsValidGuestUrl(GURL("")));
  EXPECT_FALSE(IsValidGuestUrl(GURL("about:blank")));
  EXPECT_FALSE(IsValidGuestUrl(GURL("javascript:alert(1)")));

  // Insecure HTTP is rejected (except localhost):
  EXPECT_FALSE(IsValidGuestUrl(GURL("http://business.gemini.google/")));
  EXPECT_FALSE(IsValidGuestUrl(
      GURL("http://vertexaisearch.cloud.google.com/home/cid/123")));
  EXPECT_FALSE(IsValidGuestUrl(
      GURL("http://localhost.corp.google.com:10443/side-panel")));

  // Arbitrary third-party domains are rejected:
  EXPECT_FALSE(IsValidGuestUrl(GURL("https://attacker.com/")));
  EXPECT_FALSE(IsValidGuestUrl(GURL("https://example.com/side-panel")));
  EXPECT_FALSE(IsValidGuestUrl(GURL("https://notgoogle.com/home/cid/123")));
  EXPECT_FALSE(IsValidGuestUrl(
      GURL("https://cloud.google.com.attacker.com/side-panel")));

  // Localhost is allowed for development:
  EXPECT_TRUE(IsValidGuestUrl(GURL("http://localhost:8080/side-panel")));
  EXPECT_TRUE(IsValidGuestUrl(GURL("http://127.0.0.1:8080/side-panel")));
  EXPECT_TRUE(IsValidGuestUrl(GURL("https://localhost:10443/side-panel")));
  EXPECT_TRUE(IsValidGuestUrl(
      GURL("https://localhost.corp.google.com:10443/side-panel")));

  // Valid HTTPS Gemini / Cloud / Corp domains are permitted:
  EXPECT_TRUE(IsValidGuestUrl(GURL("https://business.gemini.google/")));
  EXPECT_TRUE(IsValidGuestUrl(GURL("https://gemini.google.com/")));
  EXPECT_TRUE(IsValidGuestUrl(GURL(
      "https://vertexaisearch.cloud.google.com/home/cid/fdd1e98d-1f52-4407-"
      "98fd-80e27c61fbc9")));
  EXPECT_TRUE(IsValidGuestUrl(
      GURL("https://ucs-widget.corp.google.com/home/cid/fdd1e98d-1f52-4407-"
           "98fd-80e27c61fbc9?e=SparkDogfoodLaunch%3A%3ALaunch")));
  EXPECT_TRUE(
      IsValidGuestUrl(GURL("https://ucs-app-autopush.corp.google.com/side-panel"
                           "?configId=3abd7045-7845-4f83-b204-e39fcbca3494")));
}

TEST_F(GeicPwcManagerTest, CanonicalizeGuestUrlTransformsPantheonConsoleUrls) {
  // Empty / invalid:
  EXPECT_EQ(CanonicalizeGuestUrl(GURL()), GURL());

  // Direct side-panel embed URL remains unchanged:
  const GURL side_panel_url(
      "https://ucs-app-autopush.corp.google.com/side-panel?configId="
      "3abd7045-7845-4f83-b204-e39fcbca3494&location=global&mods="
      "widget_staging_api_mod");
  EXPECT_EQ(CanonicalizeGuestUrl(side_panel_url), side_panel_url);

  // Pantheon URL with /home/cid/<configId> is rewritten to
  // /side-panel?configId=<configId>:
  const GURL pantheon_url(
      "https://vertexaisearch.cloud.google.com/home/cid/fdd1e98d-1f52-4407-"
      "98fd-80e27c61fbc9");
  EXPECT_EQ(CanonicalizeGuestUrl(pantheon_url),
            GURL("https://vertexaisearch.cloud.google.com/side-panel?configId="
                 "fdd1e98d-1f52-4407-98fd-80e27c61fbc9"));

  // Pantheon URL with trailing slash:
  const GURL pantheon_slash_url(
      "https://vertexaisearch.cloud.google.com/home/cid/fdd1e98d-1f52-4407-"
      "98fd-80e27c61fbc9/");
  EXPECT_EQ(CanonicalizeGuestUrl(pantheon_slash_url),
            GURL("https://vertexaisearch.cloud.google.com/side-panel?configId="
                 "fdd1e98d-1f52-4407-98fd-80e27c61fbc9"));

  // Pantheon URL with query parameters preserves existing query params:
  const GURL pantheon_query_url(
      "https://vertexaisearch.cloud.google.com/home/cid/"
      "fdd1e98d-1f52-4407-98fd-"
      "80e27c61fbc9?e=SparkDogfoodLaunch%3A%3ALaunch&location=global");
  EXPECT_EQ(
      CanonicalizeGuestUrl(pantheon_query_url),
      GURL("https://vertexaisearch.cloud.google.com/side-panel?configId="
           "fdd1e98d-1f52-4407-98fd-"
           "80e27c61fbc9&e=SparkDogfoodLaunch%3A%3ALaunch&location=global"));
}

TEST_F(GeicPwcManagerTest,
       ConfiguresGuestURLFromCommandLineCanonicalizesPantheonUrl) {
  base::test::ScopedCommandLine scoped_command_line;
  const std::string pantheon_url =
      "https://vertexaisearch.cloud.google.com/home/cid/"
      "fdd1e98d-1f52-4407-98fd-"
      "80e27c61fbc9?e=SparkDogfoodLaunch%3A%3ALaunch";
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      kGeicGuestURLSwitch, pantheon_url);

  const GURL expected_canonical_url(
      "https://vertexaisearch.cloud.google.com/side-panel?configId="
      "fdd1e98d-1f52-4407-98fd-80e27c61fbc9&e=SparkDogfoodLaunch%3A%3ALaunch");

  EXPECT_EQ(GeicPwcManager::GetConfiguredGuestURL(nullptr),
            expected_canonical_url);

  GeicPwcManager manager(profile());
  EXPECT_EQ(manager.guest_url(), expected_canonical_url);
}

TEST_F(GeicPwcManagerTest, ConfiguresGuestURLFromEnterprisePolicy) {
  base::DictValue policy_dict;
  policy_dict.Set(
      "url",
      "https://vertexaisearch.cloud.google.com/home/cid/fdd1e98d-1f52-4407-"
      "98fd-80e27c61fbc9");
  profile()->GetPrefs()->SetDict("glic.gemini_enterprise_settings",
                                 std::move(policy_dict));

  const GURL expected_canonical_url(
      "https://vertexaisearch.cloud.google.com/side-panel?configId="
      "fdd1e98d-1f52-4407-98fd-80e27c61fbc9");

  EXPECT_EQ(GetPolicyGuestUrl(profile()), expected_canonical_url);
  EXPECT_EQ(GeicPwcManager::GetConfiguredGuestURL(profile()),
            expected_canonical_url);

  GeicPwcManager manager(profile());
  EXPECT_EQ(manager.guest_url(), expected_canonical_url);
}

TEST_F(GeicPwcManagerTest, CommandLineOverridesEnterprisePolicy) {
  // Set policy:
  base::DictValue policy_dict;
  policy_dict.Set("url", "https://business.gemini.google/");
  profile()->GetPrefs()->SetDict("glic.gemini_enterprise_settings",
                                 std::move(policy_dict));

  // Set switch:
  base::test::ScopedCommandLine scoped_command_line;
  const std::string custom_switch_url =
      "https://localhost.corp.google.com:10443/side-panel";
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      kGeicGuestURLSwitch, custom_switch_url);

  // Switch takes precedence over policy:
  EXPECT_EQ(GeicPwcManager::GetConfiguredGuestURL(profile()),
            GURL(custom_switch_url));

  GeicPwcManager manager(profile());
  EXPECT_EQ(manager.guest_url(), GURL(custom_switch_url));
}

}  // namespace
}  // namespace geic

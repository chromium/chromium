// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {
namespace {

class GlicSiteSettingsBrowserTest : public GlicBrowserTest,
                                    public testing::WithParamInterface<bool> {
 public:
  GlicSiteSettingsBrowserTest() {
    if (IsNoWebview()) {
      scoped_feature_list_.InitAndEnableFeature(features::kGlicNoWebview);
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kGlicNoWebview);
    }
  }

  bool IsNoWebview() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicSiteSettingsBrowserTest,
                       SiteSettingsCookiesDoesNotAffectGlicSidePanel) {
  if (!IsNoWebview()) {
    GTEST_SKIP() << "Only fixed in NoWebview mode";
  }

  const GURL guest_url = GetGuestURL();
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  settings_map->SetContentSettingDefaultScope(guest_url, guest_url,
                                              ContentSettingsType::COOKIES,
                                              CONTENT_SETTING_BLOCK);

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);

  EXPECT_EQ("k=v", content::EvalJs(guest_contents,
                                   "document.cookie = 'k=v'; document.cookie"));
  EXPECT_EQ("ok", content::EvalJs(guest_contents, R"js(
    (() => {
      try {
        window.localStorage.setItem('k', 'v');
        return 'ok';
      } catch (e) {
        return e.name;
      }
    })()
  )js"));
}

IN_PROC_BROWSER_TEST_P(GlicSiteSettingsBrowserTest,
                       SiteSettingsJitAndV8OptimizerDoNotAffectGlicSidePanel) {
  if (!IsNoWebview()) {
    GTEST_SKIP() << "Only fixed in NoWebview mode";
  }

  const GURL guest_url = GetGuestURL();
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  settings_map->SetContentSettingDefaultScope(
      guest_url, guest_url, ContentSettingsType::JAVASCRIPT_JIT,
      CONTENT_SETTING_BLOCK);
  settings_map->SetContentSettingDefaultScope(
      guest_url, guest_url, ContentSettingsType::JAVASCRIPT_OPTIMIZER,
      CONTENT_SETTING_BLOCK);

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);

  content::RenderProcessHost* rph =
      guest_contents->GetPrimaryMainFrame()->GetProcess();
  EXPECT_FALSE(rph->IsJitDisabled());
  EXPECT_FALSE(rph->AreV8OptimizationsDisabled());
}

IN_PROC_BROWSER_TEST_P(GlicSiteSettingsBrowserTest,
                       SiteSettingsJavaScriptDoesNotAffectGlicSidePanel) {
  const GURL guest_url = GetGuestURL();
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  settings_map->SetContentSettingDefaultScope(guest_url, guest_url,
                                              ContentSettingsType::JAVASCRIPT,
                                              CONTENT_SETTING_BLOCK);

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  // Even with JAVASCRIPT blocked for the guest origin, the Glic side panel
  // executes its client scripts and connects.
  ASSERT_OK(WaitForGlicClient(instance));
  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);
  EXPECT_EQ(2, content::EvalJs(guest_contents, "1 + 1"));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    GlicSiteSettingsBrowserTest,
    ::testing::Bool(),
    [](const testing::TestParamInfo<bool>& info) {
      return info.param ? "NoWebview" : "Webview";
    });

}  // namespace
}  // namespace glic

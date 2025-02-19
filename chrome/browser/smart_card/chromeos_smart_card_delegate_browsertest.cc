// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/chromeos_smart_card_delegate.h"

#include <optional>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings_types.mojom-shared.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/smart_card_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "third_party/blink/public/common/features_generated.h"

class ChromeOsSmartCardDelegateBrowserTest
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  void SetUpOnMainThread() override {
    IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();
    app_ = web_app::IsolatedWebAppBuilder(
               web_app::ManifestBuilder().AddPermissionsPolicyWildcard(
                   network::mojom::PermissionsPolicyFeature::kSmartCard))
               .BuildBundle();
    app_frame_ = OpenApp(app_->InstallChecked(profile()).app_id());
    ASSERT_TRUE(app_frame_);
  }

  void TearDownOnMainThread() override {
    app_.reset();
    app_frame_ = nullptr;
    IsolatedWebAppBrowserTestHarness::TearDownOnMainThread();
  }

  base::Time GetLastUsed(content::RenderFrameHost& rfh) {
    return content_settings::PageSpecificContentSettings::GetForFrame(&rfh)
        ->GetLastUsedTime(
            content_settings::mojom::ContentSettingsType::SMART_CARD_GUARD);
  }

  bool IsInUse(content::RenderFrameHost& rfh) {
    return content_settings::PageSpecificContentSettings::GetForFrame(&rfh)
        ->IsInUse(
            content_settings::mojom::ContentSettingsType::SMART_CARD_GUARD);
  }

  content::SmartCardDelegate* GetSmartCardDelegate() {
    return content::GetContentClientForTesting()
        ->browser()
        ->GetSmartCardDelegate();
  }

 protected:
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app_;
  raw_ptr<content::RenderFrameHost> app_frame_ = nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kSmartCard};
};

IN_PROC_BROWSER_TEST_F(ChromeOsSmartCardDelegateBrowserTest,
                       NotifyConnectionUsed) {
  auto before = base::Time::Now();
  ASSERT_GT(before, GetLastUsed(*app_frame_));

  GetSmartCardDelegate()->NotifyConnectionUsed(*app_frame_);

  ASSERT_LE(before, GetLastUsed(*app_frame_));
  ASSERT_TRUE(IsInUse(*app_frame_));
}

IN_PROC_BROWSER_TEST_F(ChromeOsSmartCardDelegateBrowserTest,
                       NotifyLastConnectionLost) {
  ASSERT_FALSE(IsInUse(*app_frame_));

  GetSmartCardDelegate()->NotifyConnectionUsed(*app_frame_);
  ASSERT_TRUE(IsInUse(*app_frame_));

  GetSmartCardDelegate()->NotifyLastConnectionLost(*app_frame_);
  ASSERT_FALSE(IsInUse(*app_frame_));
}

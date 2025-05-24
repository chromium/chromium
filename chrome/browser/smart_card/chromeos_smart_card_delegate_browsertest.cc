// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/chromeos_smart_card_delegate.h"

#include <optional>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/smart_card/smart_card_permission_context.h"
#include "chrome/browser/smart_card/smart_card_permission_context_factory.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_types.mojom-shared.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/smart_card_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "third_party/blink/public/common/features.h"

namespace {
constexpr char kDummyReader[] = "Dummy Reader";
}  // namespace

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

  void GrantReaderPermission() {
    SmartCardPermissionContextFactory::GetForProfile(*profile())
        .GrantPersistentReaderPermission(app_frame_->GetLastCommittedOrigin(),
                                         kDummyReader);
  }

  void GrantEphemeralReaderPermission() {
    SmartCardPermissionContextFactory::GetForProfile(*profile())
        .GrantEphemeralReaderPermission(app_frame_->GetLastCommittedOrigin(),
                                        kDummyReader);
  }

  bool HasReaderPermission(const url::Origin& origin,
                           const std::string& reader) {
    return SmartCardPermissionContextFactory::GetForProfile(*profile())
        .HasReaderPermission(origin, reader);
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

IN_PROC_BROWSER_TEST_F(ChromeOsSmartCardDelegateBrowserTest,
                       ReconnectionInTheBackgroundImpossible) {
  GrantReaderPermission();
  auto& pscs = CHECK_DEREF(
      content_settings::PageSpecificContentSettings::GetForFrame(app_frame_));
  // Set the last used time past the reconnection deadline.
  //
  // Depending on real time in browser tests might not be generally a good idea,
  // but this one will be stable as long as test execution doesn't travel back
  // in time. If it does, we might have bigger problems than
  // instability here.
  pscs.set_last_used_time_for_testing(ContentSettingsType::SMART_CARD_GUARD,
                                      base::Time::Now() -

                                          base::Seconds(16));

  // Hiding takes focus from the window.
  content::WebContents::FromRenderFrameHost(app_frame_)
      ->GetTopLevelNativeWindow()
      ->Hide();

  // Window does not have focus and the last connection was more than
  // `kSmartCardAllowedReconnectTime` ago. Hence, connection is not possible.
  EXPECT_FALSE(
      GetSmartCardDelegate()->HasReaderPermission(*app_frame_, kDummyReader));
}

IN_PROC_BROWSER_TEST_F(ChromeOsSmartCardDelegateBrowserTest,
                       ReconnectionInTheBackgroundPossible) {
  GrantReaderPermission();
  auto& pscs = CHECK_DEREF(
      content_settings::PageSpecificContentSettings::GetForFrame(app_frame_));
  // Set the last used time within the reconnection deadline (in the future).
  //
  // Depending on real time in browser tests might not be generally a good idea,
  // but this one will be stable as long as test execution from here takes less
  // than a minute. If it takes longer, we might have bigger problems than
  // instability here.
  pscs.set_last_used_time_for_testing(ContentSettingsType::SMART_CARD_GUARD,
                                      base::Time::Now() + base::Minutes(1));

  // Hiding takes focus from the window.
  content::WebContents::FromRenderFrameHost(app_frame_)
      ->GetTopLevelNativeWindow()
      ->Hide();

  // Window does not have focus but the last connection was less than
  // `kSmartCardAllowedReconnectTime` ago (probably still in the future). Hence,
  // connection is deemed a reconnection and is possible.
  EXPECT_TRUE(
      GetSmartCardDelegate()->HasReaderPermission(*app_frame_, kDummyReader));
}

IN_PROC_BROWSER_TEST_F(ChromeOsSmartCardDelegateBrowserTest,
                       ReconnectionInTheForegroundPossible) {
  GrantReaderPermission();
  auto& pscs = CHECK_DEREF(
      content_settings::PageSpecificContentSettings::GetForFrame(app_frame_));
  // Set the last used time past the reconnection deadline.
  //
  // Depending on real time in browser tests might not be generally a good idea,
  // but this one will be stable as long as test execution doesn't travel back
  // in time. If it does, we might have bigger problems than
  // instability here.
  pscs.set_last_used_time_for_testing(ContentSettingsType::SMART_CARD_GUARD,
                                      base::Time::Now() - base::Seconds(16));

  // Long time from last connection should not have impact on connection
  // possibility as long as the window has focus.
  EXPECT_TRUE(
      GetSmartCardDelegate()->HasReaderPermission(*app_frame_, kDummyReader));
}

IN_PROC_BROWSER_TEST_F(ChromeOsSmartCardDelegateBrowserTest,
                       PermissionRequestInTheBackgroundImpossible) {
  GrantReaderPermission();

  // Hiding takes focus from the window.
  content::WebContents::FromRenderFrameHost(app_frame_)
      ->GetTopLevelNativeWindow()
      ->Hide();

  base::test::TestFuture<bool> got_permission;
  // This should immediately return false, not even trying to display any
  // prompts.
  GetSmartCardDelegate()->RequestReaderPermission(*app_frame_, kDummyReader,
                                                  got_permission.GetCallback());
  EXPECT_FALSE(got_permission.Get());
}

IN_PROC_BROWSER_TEST_F(ChromeOsSmartCardDelegateBrowserTest,
                       ClosingWindowClearsEphemeralPermissions) {
  GrantEphemeralReaderPermission();
  const auto origin = app_frame_->GetLastCommittedOrigin();
  EXPECT_TRUE(HasReaderPermission(origin, kDummyReader));

  auto* tab_interface = tabs::TabInterface::GetFromContents(
      content::WebContents::FromRenderFrameHost(app_frame_));
  // Not resetting this causes dangling raw pointer error when closing tab.
  app_frame_ = nullptr;
  tab_interface->Close();

  EXPECT_FALSE(HasReaderPermission(origin, kDummyReader));
}

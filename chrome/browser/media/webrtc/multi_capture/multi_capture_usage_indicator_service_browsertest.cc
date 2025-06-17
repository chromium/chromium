// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/multi_capture/multi_capture_usage_indicator_service.h"

#include <optional>
#include <string>

#include "base/check_deref.h"
#include "base/scoped_observation.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/media/webrtc/multi_capture/multi_capture_usage_indicator_service_factory.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_server_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/web_package/test_support/signed_web_bundles/key_pair.h"
#include "content/public/test/browser_test.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {
static constexpr std::string_view kIndexHtml706 = R"(
  <head>
    <title>7.0.6</title>
  </head>
  <body>
    <h1>Hello from version 7.0.6</h1>
  </body>)";

}  // namespace

namespace multi_capture {

class MultiCaptureUsageIndicatorBrowserTest
    : public web_app::IsolatedWebAppBrowserTestHarness,
      public NotificationDisplayService::Observer {
 public:
  url::Origin GetAppOrigin() const {
    return web_app::IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
               GetWebBundleId())
        .origin();
  }
  webapps::AppId GetAppId() const {
    return web_app::IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
               GetWebBundleId())
        .app_id();
  }
  web_package::SignedWebBundleId GetWebBundleId() const {
    return web_package::test::GetDefaultEd25519WebBundleId();
  }
  const web_app::WebApp* GetIsolatedWebApp(const webapps::AppId& app_id) {
    return provider().registrar_unsafe().GetAppById(app_id);
  }

  std::optional<std::string> GetNotification() {
    std::optional<std::string> notification;
    bool notification_received = base::test::RunUntil([&]() -> bool {
      base::test::TestFuture<std::set<std::string>, bool> get_displayed_future;
      notificiation_display_service().GetDisplayed(
          get_displayed_future.GetCallback());
      const auto notifications =
          get_displayed_future.Get<std::set<std::string>>();
      if (notifications.size() == 1) {
        notification = *notifications.begin();
        return true;
      }
      return false;
    });
    EXPECT_TRUE(notification_received);
    return notification;
  }

  NotificationDisplayService& notificiation_display_service() {
    return CHECK_DEREF(
        NotificationDisplayServiceFactory::GetForProfile(profile()));
  }

  // NotificationDisplayService::Observer:
  void OnNotificationDisplayed(
      const message_center::Notification& notification,
      const NotificationCommon::Metadata* const metadata) override {
    last_received_notification_ = notification;
  }

  void OnNotificationClosed(const std::string& notification_id) override {}

  void OnNotificationDisplayServiceDestroyed(
      NotificationDisplayService* service) override {
    notification_observation_.Reset();
  }

 protected:
  void SetUpOnMainThread() override {
    web_app::IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();
    update_server_mixin_.AddBundle(
        web_app::IsolatedWebAppBuilder(
            web_app::ManifestBuilder().SetName("app-3.0.4").SetVersion("3.0.4"))
            .AddHtml("/", kIndexHtml706)
            .BuildBundle(GetWebBundleId(),
                         {web_package::test::GetDefaultEd25519KeyPair()}));
    notification_observation_.Observe(&notificiation_display_service());
  }

  std::optional<message_center::Notification> last_received_notification_;
  web_app::IsolatedWebAppUpdateServerMixin update_server_mixin_{&mixin_host_};
  base::test::ScopedFeatureList scoped_feature_list{
      chromeos::features::kMultiCaptureReworkedUsageIndicators};

 private:
  base::ScopedObservation<NotificationDisplayService,
                          MultiCaptureUsageIndicatorBrowserTest>
      notification_observation_{this};
};

IN_PROC_BROWSER_TEST_F(
    MultiCaptureUsageIndicatorBrowserTest,
    YouMayBeCapturedNotificationShowsIfAppInstalledAndAllowlisted) {
  PrefService& prefs = CHECK_DEREF(profile()->GetPrefs());
  prefs.SetList(prefs::kIsolatedWebAppInstallForceList,
                base::Value::List().Append(
                    update_server_mixin_.CreateForceInstallPolicyEntry(
                        GetWebBundleId())));
  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({GetAppId()});
  prefs.SetList(capture_policy::kManagedMultiScreenCaptureAllowedForUrls,
                base::Value::List().Append(
                    base::Value("isolated-app://" + GetWebBundleId().id())));

  multi_capture::MultiCaptureUsageIndicatorServiceFactory::GetForBrowserContext(
      profile())
      ->ShowUsageIndicatorsOnStart();

  const auto notification = GetNotification();
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(*notification, "multi-capture-login-privacy-indicators");

  ASSERT_TRUE(last_received_notification_.has_value());
  EXPECT_EQ(last_received_notification_->title(), u"");
  EXPECT_EQ(
      last_received_notification_->message(),
      u"Your administrator can record your screen anytime with app-3.0.4. "
      "You will be notified when the recording starts.");
}

}  // namespace multi_capture

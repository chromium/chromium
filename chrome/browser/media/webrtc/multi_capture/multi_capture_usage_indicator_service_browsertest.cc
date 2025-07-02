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
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"
#include "content/public/test/browser_test.h"
#include "ui/message_center/public/cpp/notification.h"

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Field;
using ::testing::Pair;
using ::testing::Property;

namespace {
static constexpr std::string_view kIndexHtml706 = R"(
  <head>
    <title>7.0.6</title>
  </head>
  <body>
    <h1>Hello from version 7.0.6</h1>
  </body>)";

static constexpr std::string_view kCurrentCaptureNotificationId =
    "multi-capture-active-privacy-indicators-";

}  // namespace

struct InstalledApp {
  web_package::test::EcdsaP256KeyPair key_pair;
  web_package::SignedWebBundleId bundle_id;
  std::string app_name;
};

struct MultiCaptureUsageIndicatorBrowserTestData {
  std::vector<InstalledApp> installed_apps;
  std::vector<InstalledApp> allowlisted_capture_apps;
  std::vector<InstalledApp> skip_notification_apps;
  std::vector<InstalledApp> capturing_apps;
  std::u16string expected_icon_notification_message_before_capture;
  std::u16string expected_icon_notification_message_after_capture;
  std::optional<std::u16string>
      expected_no_icon_notification_message_after_capture;
};

const web_package::test::EcdsaP256KeyPair app1_key_pair =
    web_package::test::EcdsaP256KeyPair::CreateRandom(
        /*produce_invalid_signature=*/false);
const web_package::SignedWebBundleId app1_bundle_id =
    web_package::SignedWebBundleId::CreateForPublicKey(
        app1_key_pair.public_key);
const InstalledApp app_1 = {.key_pair = app1_key_pair,
                            .bundle_id = app1_bundle_id,
                            .app_name = "app 1"};

const web_package::test::EcdsaP256KeyPair app2_key_pair =
    web_package::test::EcdsaP256KeyPair::CreateRandom(
        /*produce_invalid_signature=*/false);
const web_package::SignedWebBundleId app2_bundle_id =
    web_package::SignedWebBundleId::CreateForPublicKey(
        app2_key_pair.public_key);
const InstalledApp app_2 = {.key_pair = app2_key_pair,
                            .bundle_id = app2_bundle_id,
                            .app_name = "app 2"};

namespace multi_capture {

class MultiCaptureUsageIndicatorBrowserTest
    : public web_app::IsolatedWebAppBrowserTestHarness,
      public NotificationDisplayService::Observer,
      public testing::WithParamInterface<
          MultiCaptureUsageIndicatorBrowserTestData> {
 public:
  // NotificationDisplayService::Observer:
  void OnNotificationDisplayed(
      const message_center::Notification& notification,
      const NotificationCommon::Metadata* const metadata) override {
    visible_notifications_.insert({notification.id(), notification});
  }

  void OnNotificationClosed(const std::string& notification_id) override {
    visible_notifications_.erase(notification_id);
  }

  void OnNotificationDisplayServiceDestroyed(
      NotificationDisplayService* service) override {
    notification_observation_.Reset();
  }

 protected:
  void SetUpOnMainThread() override {
    web_app::IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();

    notification_observation_.Observe(&notificiation_display_service());

    InstallIwas();
    SetCaptureAllowList();
    SetSkipNotificationsAllowlist();
  }

  void InstallIwas() {
    base::Value::List install_iwa_force_list;
    std::set<webapps::AppId> app_ids_to_wait_for;
    for (const auto& installed_app : GetParam().installed_apps) {
      update_server_mixin_.AddBundle(
          web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder()
                                             .SetName(installed_app.app_name)
                                             .SetVersion("3.0.4"))
              .AddHtml("/", kIndexHtml706)
              .BuildBundle(installed_app.bundle_id, {installed_app.key_pair}));
      install_iwa_force_list.Append(
          update_server_mixin_.CreateForceInstallPolicyEntry(
              installed_app.bundle_id));
      app_ids_to_wait_for.insert(
          web_app::IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
              installed_app.bundle_id)
              .app_id());
    }

    CHECK_DEREF(profile()->GetPrefs())
        .SetList(prefs::kIsolatedWebAppInstallForceList,
                 std::move(install_iwa_force_list));
    web_app::WebAppTestInstallObserver(browser()->profile())
        .BeginListeningAndWait(app_ids_to_wait_for);
  }

  void SetCaptureAllowList() {
    base::Value::List capture_allow_list;
    for (const auto& allowed_app : GetParam().allowlisted_capture_apps) {
      capture_allow_list.Append(
          base::Value("isolated-app://" + allowed_app.bundle_id.id()));
    }

    CHECK_DEREF(profile()->GetPrefs())
        .SetList(capture_policy::kManagedMultiScreenCaptureAllowedForUrls,
                 std::move(capture_allow_list));
  }

  void SetSkipNotificationsAllowlist() {
    web_app::IwaKeyDistributionInfoProvider::SpecialAppPermissions
        special_app_permissions;
    for (const auto& skipping_app : GetParam().skip_notification_apps) {
      special_app_permissions[skipping_app.bundle_id.id()] = {
          .skip_capture_started_notification = true};
    }
    web_app::IwaKeyDistributionInfoProvider::GetInstance()
        .SetComponentDataForTesting(
            web_app::IwaKeyDistributionInfoProvider::ComponentData(
                /*version=*/base::Version("1.0.0"),
                /*key_rotations=*/{},
                /*special_app_permissions=*/
                special_app_permissions,
                /*managed_allowlist=*/{},
                /*special_app_permissions=*/true));
  }

  const web_app::WebApp* GetIsolatedWebApp(const webapps::AppId& app_id) {
    return provider().registrar_unsafe().GetAppById(app_id);
  }

  bool WaitForNotifications(const size_t wait_for_notifications_count) {
    bool notifications_received = base::test::RunUntil([&]() -> bool {
      base::test::TestFuture<std::set<std::string>, bool> get_displayed_future;
      notificiation_display_service().GetDisplayed(
          get_displayed_future.GetCallback());
      const auto displayed_notifications =
          get_displayed_future.Get<std::set<std::string>>();
      return displayed_notifications.size() == wait_for_notifications_count;
    });
    return notifications_received;
  }

  void ClearAllNotifications() {
    auto visible_notifications = visible_notifications_;
    for (const auto& [notification_id, _] : visible_notifications) {
      notificiation_display_service().Close(
          NotificationHandler::Type::ANNOUNCEMENT, notification_id);
    }
  }

  webapps::AppId GetAppIdForBundle(const std::string& bundle_id) {
    GURL url("isolated-app://" + bundle_id);
    const std::optional<webapps::AppId> app_id =
        provider().registrar_unsafe().FindBestAppWithUrlInScope(
            url, web_app::WebAppFilter::IsIsolatedApp());
    CHECK(app_id);
    return *app_id;
  }

  webapps::AppId GetCapturingNotificationWithAppId() const {
    for (const auto& [visible_notification_id, _] : visible_notifications_) {
      if (visible_notification_id.starts_with(kCurrentCaptureNotificationId)) {
        return visible_notification_id;
      }
    }
    NOTREACHED();
  }

  NotificationDisplayService& notificiation_display_service() {
    return CHECK_DEREF(
        NotificationDisplayServiceFactory::GetForProfile(profile()));
  }

  std::map<std::string, message_center::Notification> visible_notifications_;
  web_app::IsolatedWebAppUpdateServerMixin update_server_mixin_{&mixin_host_};
  base::test::ScopedFeatureList scoped_feature_list{
      chromeos::features::kMultiCaptureReworkedUsageIndicators};

 private:
  base::ScopedObservation<NotificationDisplayService,
                          MultiCaptureUsageIndicatorBrowserTest>
      notification_observation_{this};
};

IN_PROC_BROWSER_TEST_P(
    MultiCaptureUsageIndicatorBrowserTest,
    YouMayBeCapturedNotificationShowsIfAppInstalledAndAllowlisted) {
  multi_capture::MultiCaptureUsageIndicatorServiceFactory::GetForBrowserContext(
      profile())
      ->ShowUsageIndicatorsOnStart();

  ASSERT_TRUE(WaitForNotifications(/*wait_for_notifications_count=*/1u));
  ASSERT_TRUE(visible_notifications_.contains(
      "multi-capture-login-privacy-indicators"));
  const auto& notification =
      visible_notifications_.at("multi-capture-login-privacy-indicators");
  EXPECT_EQ(notification.title(), u"");
  EXPECT_EQ(notification.message(),
            GetParam().expected_icon_notification_message_before_capture);
}

IN_PROC_BROWSER_TEST_P(
    MultiCaptureUsageIndicatorBrowserTest,
    YouAreCapturedNotificationShowsIfAppInstalledAndAllowlisted) {
  multi_capture::MultiCaptureUsageIndicatorService& service =
      CHECK_DEREF(multi_capture::MultiCaptureUsageIndicatorServiceFactory::
                      GetForBrowserContext(profile()));

  service.ShowUsageIndicatorsOnStart();

  ASSERT_TRUE(WaitForNotifications(/*wait_for_notifications_count=*/1u));
  ASSERT_TRUE(visible_notifications_.contains(
      "multi-capture-login-privacy-indicators"));
  const auto& notification =
      visible_notifications_.at("multi-capture-login-privacy-indicators");
  EXPECT_EQ(notification.title(), u"");
  EXPECT_EQ(notification.message(),
            GetParam().expected_icon_notification_message_before_capture);

  if (GetParam().capturing_apps.empty()) {
    return;
  }

  for (const InstalledApp& app : GetParam().capturing_apps) {
    ClearAllNotifications();
    service.MultiCaptureStarted(
        /*label=*/"label1",
        /*app_id=*/GetAppIdForBundle(app.bundle_id.id()));
  }

  const size_t expected_notification_count =
      GetParam().expected_no_icon_notification_message_after_capture ? 2u : 1u;
  ASSERT_TRUE(WaitForNotifications(expected_notification_count));
  ASSERT_EQ(visible_notifications_.size(), expected_notification_count);
  EXPECT_THAT(
      visible_notifications_,
      Contains(Pair(
          "multi-capture-login-privacy-indicators",
          AllOf(
              Property(&message_center::Notification::title, u""),
              Property(
                  &message_center::Notification::message,
                  GetParam().expected_icon_notification_message_after_capture),
              Property(&message_center::Notification::notifier_id,
                       Field(&message_center::NotifierId::id,
                             "multi-capture-privacy-indicators"))))));

  if (expected_notification_count == 2) {
    EXPECT_THAT(
        visible_notifications_,
        Contains(Pair(
            GetCapturingNotificationWithAppId(),
            AllOf(Property(&message_center::Notification::title, u""),
                  Property(
                      &message_center::Notification::message,
                      GetParam()
                          .expected_no_icon_notification_message_after_capture
                          .value())))));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MultiCaptureUsageIndicatorBrowserTest,
    testing::ValuesIn({
        // New test case: One app installed and allowlist --> Standard
        // notification.
        MultiCaptureUsageIndicatorBrowserTestData{
            .installed_apps = {app_1},
            .allowlisted_capture_apps = {app_1},
            .skip_notification_apps = {},
            .capturing_apps = {app_1},
            .expected_icon_notification_message_before_capture =
                u"Your administrator can record your screen with app 1. "
                "You will be notified when the recording starts.",
            .expected_icon_notification_message_after_capture =
                u"Your administrator is recording your screen with app 1.",
            .expected_no_icon_notification_message_after_capture =
                std::nullopt},
        // New test case: One app installed and two allowlisted --> Still
        // only one app in the notification.
        MultiCaptureUsageIndicatorBrowserTestData{
            .installed_apps = {app_1},
            .allowlisted_capture_apps = {app_1, app_2},
            .skip_notification_apps = {},
            .capturing_apps = {app_1},
            .expected_icon_notification_message_before_capture =
                u"Your administrator can record your screen with app 1. "
                "You will be notified when the recording starts.",
            .expected_icon_notification_message_after_capture =
                u"Your administrator is recording your screen with app 1.",
            .expected_no_icon_notification_message_after_capture =
                std::nullopt},
        // New test case: Two apps installed and two allowlisted --> Standard
        // notification. One is capturing --> show a future notification and a
        // current notification.
        // (and remove the future capture notification).
        MultiCaptureUsageIndicatorBrowserTestData{
            .installed_apps = {app_1, app_2},
            .allowlisted_capture_apps = {app_1, app_2},
            .skip_notification_apps = {},
            .capturing_apps = {app_1},
            .expected_icon_notification_message_before_capture =
                u"Your administrator can record your screen with app 1 and "
                u"app 2. You will be notified when the recording starts.",
            .expected_icon_notification_message_after_capture =
                u"Your administrator can record your screen with app 2. You "
                u"will be notified when the recording starts.",
            .expected_no_icon_notification_message_after_capture =
                u"Your administrator is recording your screen with app 1."},
        // New test case: Two apps installed and two allowlisted --> Standard
        // notification. Both are capturing --> show a notification for both
        // (and remove the future capture notification).
        MultiCaptureUsageIndicatorBrowserTestData{
            .installed_apps = {app_1, app_2},
            .allowlisted_capture_apps = {app_1, app_2},
            .skip_notification_apps = {},
            .capturing_apps = {app_1, app_2},
            .expected_icon_notification_message_before_capture =
                u"Your administrator can record your screen with app 1 and "
                u"app 2. You will be notified when the recording starts.",
            .expected_icon_notification_message_after_capture =
                u"Your administrator is recording your screen with app 1.",
            .expected_no_icon_notification_message_after_capture =
                u"Your administrator is recording your screen with app 2."},
        // New test case: One app installed and one allowlisted --> Bypass
        // notification.
        MultiCaptureUsageIndicatorBrowserTestData{
            .installed_apps = {app_1},
            .allowlisted_capture_apps = {app_1},
            .skip_notification_apps = {app_1},
            .capturing_apps = {},
            .expected_icon_notification_message_before_capture =
                u"Your administrator can record your screen with app 1. You "
                u"will not be notified when the recording starts.",
            .expected_icon_notification_message_after_capture =
                u"Your administrator can record your screen with app 1. You "
                u"will not be notified when the recording starts.",
            .expected_no_icon_notification_message_after_capture =
                std::nullopt},
        // New test case: One app installed and two allowlisted --> Bypass
        // notification for one app.
        MultiCaptureUsageIndicatorBrowserTestData{
            .installed_apps = {app_1},
            .allowlisted_capture_apps = {app_1, app_2},
            .skip_notification_apps = {app_1},
            .capturing_apps = {app_1},
            .expected_icon_notification_message_before_capture =
                u"Your administrator can record your screen with app 1. You "
                u"will not be notified when the recording starts.",
            .expected_icon_notification_message_after_capture =
                u"Your administrator can record your screen with app 1. You "
                u"will not be notified when the recording starts.",
            .expected_no_icon_notification_message_after_capture =
                std::nullopt},
        // New test case: Two apps installed and two allowlisted --> Bypass
        // notification for one app. App that isn't allowed to bypass is
        // capturing.
        MultiCaptureUsageIndicatorBrowserTestData{
            .installed_apps = {app_1, app_2},
            .allowlisted_capture_apps = {app_1, app_2},
            .skip_notification_apps = {app_1},
            .capturing_apps = {app_2},
            .expected_icon_notification_message_before_capture =
                u"Your administrator can record your screen with app 1 and app "
                u"2.",
            .expected_icon_notification_message_after_capture =
                u"Your administrator can record your screen with app 1. You "
                u"will not be notified when the recording starts.",
            .expected_no_icon_notification_message_after_capture =
                u"Your administrator is recording your screen with app 2."},
        // New test case: Two apps installed and two allowlisted --> Bypass
        // notification for one app. App that is allowed to bypass is
        // capturing --> the notification does not change.
        MultiCaptureUsageIndicatorBrowserTestData{
            .installed_apps = {app_1, app_2},
            .allowlisted_capture_apps = {app_1, app_2},
            .skip_notification_apps = {app_1},
            .capturing_apps = {app_1},
            .expected_icon_notification_message_before_capture =
                u"Your administrator can record your screen with app 1 and app "
                u"2.",
            .expected_icon_notification_message_after_capture =
                u"Your administrator can record your screen with app 1 and app "
                u"2.",
            .expected_no_icon_notification_message_after_capture =
                std::nullopt},
        // New test case: Two apps installed and two allowlisted --> Bypass
        // notification for both apps. One app capturing, notification doesn't
        // change.
        MultiCaptureUsageIndicatorBrowserTestData{
            .installed_apps = {app_1, app_2},
            .allowlisted_capture_apps = {app_1, app_2},
            .skip_notification_apps = {app_1, app_2},
            .capturing_apps = {app_1, app_2},
            .expected_icon_notification_message_before_capture =
                u"Your administrator can record your screen with app 1 and app "
                u"2. You will not be notified when the recording starts.",
            .expected_icon_notification_message_after_capture =
                u"Your administrator can record your screen with app 1 and app "
                u"2. You will not be notified when the recording starts.",
            .expected_no_icon_notification_message_after_capture =
                std::nullopt},
    }));

}  // namespace multi_capture

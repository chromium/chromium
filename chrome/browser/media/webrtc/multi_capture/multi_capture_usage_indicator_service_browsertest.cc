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
#include "chrome/browser/media/webrtc/multi_capture/multi_capture_data_service.h"
#include "chrome/browser/media/webrtc/multi_capture/multi_capture_data_service_factory.h"
#include "chrome/browser/media/webrtc/multi_capture/multi_capture_usage_indicator_service_factory.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test_update_server.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
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
  std::u16string expected_icon_notification_title_before_capture;
  std::u16string expected_icon_notification_title_after_capture;
  std::optional<std::u16string>
      expected_no_icon_notification_message_after_capture;
};

const web_package::test::EcdsaP256KeyPair kApp1KeyPair =
    web_package::test::EcdsaP256KeyPair::CreateRandom(
        /*produce_invalid_signature=*/false);
const web_package::SignedWebBundleId kApp1BundleId =
    web_package::SignedWebBundleId::CreateForPublicKey(kApp1KeyPair.public_key);
const InstalledApp kApp1 = {.key_pair = kApp1KeyPair,
                            .bundle_id = kApp1BundleId,
                            .app_name = "app 1"};

const web_package::test::EcdsaP256KeyPair kApp2KeyPair =
    web_package::test::EcdsaP256KeyPair::CreateRandom(
        /*produce_invalid_signature=*/false);
const web_package::SignedWebBundleId kApp2BundleId =
    web_package::SignedWebBundleId::CreateForPublicKey(kApp2KeyPair.public_key);
const InstalledApp kApp2 = {.key_pair = kApp2KeyPair,
                            .bundle_id = kApp2BundleId,
                            .app_name = "app 2"};

namespace multi_capture {

class MultiCaptureUsageIndicatorBrowserTestBase
    : public web_app::IsolatedWebAppBrowserTestHarness,
      public NotificationDisplayService::Observer {
 public:
  MultiCaptureUsageIndicatorBrowserTestBase(
      const std::vector<InstalledApp>& installed_apps,
      const std::vector<InstalledApp> allowlisted_capture_apps,
      const std::vector<InstalledApp> skip_notification_apps,
      const std::vector<InstalledApp> capturing_apps)
      : installed_apps_(installed_apps),
        allowlisted_capture_apps_(allowlisted_capture_apps),
        skip_notification_apps_(skip_notification_apps),
        capturing_apps_(capturing_apps) {}

  // NotificationDisplayService::Observer:
  void OnNotificationDisplayed(
      const message_center::Notification& notification,
      const NotificationCommon::Metadata* const metadata) override {
    visible_notifications_.insert_or_assign(notification.id(), notification);
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

    web_app::IwaKeyDistributionInfoProvider::GetInstance()
        .SkipManagedAllowlistChecksForTesting(true);
    InstallIwas();
    SetCaptureAllowList();
    SetSkipNotificationsAllowlist();
  }

  void TearDownOnMainThread() override {
    web_app::IwaKeyDistributionInfoProvider::GetInstance()
        .SkipManagedAllowlistChecksForTesting(false);
    IsolatedWebAppBrowserTestHarness::TearDownOnMainThread();
  }

  void InstallIwa(const InstalledApp& app) {
    PrefService& prefs = CHECK_DEREF(profile()->GetPrefs());
    base::Value::List app_install_force_list =
        prefs.GetList(prefs::kIsolatedWebAppInstallForceList).Clone();
    iwa_test_update_server_.AddBundle(
        web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder()
                                           .SetName(app.app_name)
                                           .SetVersion("3.0.4"))
            .AddHtml("/", kIndexHtml706)
            .BuildBundle(app.bundle_id, {app.key_pair}));
    app_install_force_list.Append(
        iwa_test_update_server_.CreateForceInstallPolicyEntry(app.bundle_id));

    prefs.SetList(prefs::kIsolatedWebAppInstallForceList,
                  std::move(app_install_force_list));
    web_app::WebAppTestInstallObserver(browser()->profile())
        .BeginListeningAndWait(
            {web_app::IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
                 app.bundle_id)
                 .app_id()});
  }

  void InstallIwas() {
    base::Value::List install_iwa_force_list;
    std::set<webapps::AppId> app_ids_to_wait_for;
    for (const auto& installed_app : installed_apps_) {
      InstallIwa(installed_app);
    }
  }

  void SetCaptureAllowList() {
    base::Value::List capture_allow_list;
    for (const auto& allowed_app : allowlisted_capture_apps_) {
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
    auto component_builder =
        web_app::test::KeyDistributionComponentBuilder(base::Version("1.0.0"));
    for (const auto& skipping_app : skip_notification_apps_) {
      component_builder.AddToSpecialAppPermissions(
          skipping_app.bundle_id, {.skip_capture_started_notification = true});
    }
    std::move(component_builder).Build().InjectComponentDataDirectly();
  }

  const web_app::WebApp* GetIsolatedWebApp(const webapps::AppId& app_id) {
    return provider().registrar_unsafe().GetAppById(app_id);
  }

  webapps::AppId GetAppIdForBundle(const std::string& bundle_id) {
    GURL url("isolated-app://" + bundle_id);
    const std::optional<webapps::AppId> app_id =
        provider().registrar_unsafe().FindBestAppWithUrlInScope(
            url, web_app::WebAppFilter::IsIsolatedApp());
    CHECK(app_id);
    return *app_id;
  }

  webapps::AppId GetLastCapturingNotificationWithAppId() {
    return std::string(kCurrentCaptureNotificationId) +
           GetAppIdForBundle(
               capturing_apps_[capturing_apps_.size() - 1].bundle_id.id());
  }

  NotificationDisplayService& notificiation_display_service() {
    return CHECK_DEREF(
        NotificationDisplayServiceFactory::GetForProfile(profile()));
  }

  std::map<std::string, message_center::Notification> visible_notifications_;
  web_app::IsolatedWebAppTestUpdateServer iwa_test_update_server_;

 private:
  const std::vector<InstalledApp> installed_apps_;
  const std::vector<InstalledApp> allowlisted_capture_apps_;
  const std::vector<InstalledApp> skip_notification_apps_;
  const std::vector<InstalledApp> capturing_apps_;

  base::ScopedObservation<NotificationDisplayService,
                          MultiCaptureUsageIndicatorBrowserTestBase>
      notification_observation_{this};
};

class MultiCaptureUsageIndicatorBrowserTest
    : public MultiCaptureUsageIndicatorBrowserTestBase,
      public testing::WithParamInterface<
          MultiCaptureUsageIndicatorBrowserTestData> {
 public:
  MultiCaptureUsageIndicatorBrowserTest()
      : MultiCaptureUsageIndicatorBrowserTestBase(
            GetParam().installed_apps,
            GetParam().allowlisted_capture_apps,
            GetParam().skip_notification_apps,
            GetParam().capturing_apps) {}
};

IN_PROC_BROWSER_TEST_P(
    MultiCaptureUsageIndicatorBrowserTest,
    YouMayBeCapturedNotificationShowsIfAppInstalledAndAllowlisted) {
  CHECK_DEREF(
      multi_capture::MultiCaptureDataServiceFactory::GetForBrowserContext(
          profile()))
      .LoadData();

  ASSERT_TRUE(visible_notifications_.contains(
      "multi-capture-login-privacy-indicators"));
  const auto& notification =
      visible_notifications_.at("multi-capture-login-privacy-indicators");
  EXPECT_EQ(notification.title(),
            GetParam().expected_icon_notification_title_before_capture);
  EXPECT_EQ(notification.message(), u"");
}

IN_PROC_BROWSER_TEST_P(
    MultiCaptureUsageIndicatorBrowserTest,
    YouAreCapturedNotificationShowsIfAppInstalledAndAllowlisted) {
  multi_capture::MultiCaptureUsageIndicatorService&
      MultiCaptureUsageIndicatorService =
          CHECK_DEREF(multi_capture::MultiCaptureUsageIndicatorServiceFactory::
                          GetForBrowserContext(profile()));
  CHECK_DEREF(
      multi_capture::MultiCaptureDataServiceFactory::GetForBrowserContext(
          profile()))
      .LoadData();

  ASSERT_TRUE(visible_notifications_.contains(
      "multi-capture-login-privacy-indicators"));
  const auto& notification =
      visible_notifications_.at("multi-capture-login-privacy-indicators");
  EXPECT_EQ(notification.title(),
            GetParam().expected_icon_notification_title_before_capture);
  EXPECT_EQ(notification.message(), u"");

  if (GetParam().capturing_apps.empty()) {
    return;
  }

  for (const InstalledApp& app : GetParam().capturing_apps) {
    MultiCaptureUsageIndicatorService.MultiCaptureStarted(
        /*label=*/"label1",
        /*app_id=*/GetAppIdForBundle(app.bundle_id.id()));
  }

  EXPECT_THAT(
      visible_notifications_,
      Contains(Pair(
          "multi-capture-login-privacy-indicators",
          AllOf(Property(
                    &message_center::Notification::title,
                    GetParam().expected_icon_notification_title_after_capture),
                Property(&message_center::Notification::message, u""),
                Property(&message_center::Notification::notifier_id,
                         Field(&message_center::NotifierId::id,
                               "multi-capture-login-privacy-indicators"))))));

  if (GetParam()
          .expected_no_icon_notification_message_after_capture.has_value()) {
    EXPECT_THAT(
        visible_notifications_,
        Contains(Pair(
            GetLastCapturingNotificationWithAppId(),
            AllOf(Property(
                      &message_center::Notification::title,
                      GetParam()
                          .expected_no_icon_notification_message_after_capture
                          .value()),
                  Property(&message_center::Notification::message, u"")))));
  }
}

class MultiCaptureUsageIndicatorDynamicAppBrowserTest
    : public MultiCaptureUsageIndicatorBrowserTestBase {
 public:
  MultiCaptureUsageIndicatorDynamicAppBrowserTest()
      : MultiCaptureUsageIndicatorBrowserTestBase(
            /*installed_apps=*/{kApp1},
            /*allowlisted_capture_apps=*/{kApp1, kApp2},
            /*skip_notification_apps=*/{kApp1},
            /*capturing_apps=*/{}) {}
};

IN_PROC_BROWSER_TEST_F(MultiCaptureUsageIndicatorDynamicAppBrowserTest,
                       AllowlistedAppAddedAndDeleted) {
  CHECK_DEREF(
      multi_capture::MultiCaptureDataServiceFactory::GetForBrowserContext(
          profile()))
      .LoadData();

  ASSERT_EQ(visible_notifications_.size(), 1u);
  EXPECT_THAT(
      visible_notifications_,
      Contains(Pair(
          "multi-capture-login-privacy-indicators",
          AllOf(
              Property(&message_center::Notification::title,
                       u"Your administrator can record your screen with app 1. "
                       u"You will not be notified when the recording starts."),
              Property(&message_center::Notification::message, u""),
              Property(&message_center::Notification::notifier_id,
                       Field(&message_center::NotifierId::id,
                             "multi-capture-login-privacy-indicators"))))));

  InstallIwa(kApp2);
  ASSERT_EQ(visible_notifications_.size(), 1u);
  EXPECT_THAT(
      visible_notifications_,
      Contains(Pair(
          "multi-capture-login-privacy-indicators",
          AllOf(Property(&message_center::Notification::title,
                         u"Your administrator can record your screen with app "
                         u"1 and app 2."),
                Property(&message_center::Notification::message, u""),
                Property(&message_center::Notification::notifier_id,
                         Field(&message_center::NotifierId::id,
                               "multi-capture-login-privacy-indicators"))))));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MultiCaptureUsageIndicatorBrowserTest,
    testing::ValuesIn({
        // New test case: One app installed and allowlist --> Standard
        // notification.
        MultiCaptureUsageIndicatorBrowserTestData{
            .installed_apps = {kApp1},
            .allowlisted_capture_apps = {kApp1},
            .skip_notification_apps = {},
            .capturing_apps = {kApp1},
            .expected_icon_notification_title_before_capture =
                u"Your administrator can record your screen with app 1. "
                "You will be notified when the recording starts.",
            .expected_icon_notification_title_after_capture =
                u"Your administrator is recording your screen with app 1.",
            .expected_no_icon_notification_message_after_capture =
                std::nullopt},
        // New test case: One app installed and two allowlisted --> Still
        // only one app in the notification.
        MultiCaptureUsageIndicatorBrowserTestData{
            .installed_apps = {kApp1},
            .allowlisted_capture_apps = {kApp1, kApp2},
            .skip_notification_apps = {},
            .capturing_apps = {kApp1},
            .expected_icon_notification_title_before_capture =
                u"Your administrator can record your screen with app 1. "
                "You will be notified when the recording starts.",
            .expected_icon_notification_title_after_capture =
                u"Your administrator is recording your screen with app 1.",
            .expected_no_icon_notification_message_after_capture =
                std::nullopt},
        // New test case: Two apps installed and two allowlisted --> Standard
        // notification. One is capturing --> show a future notification and a
        // current notification.
        // (and remove the future capture notification).
        MultiCaptureUsageIndicatorBrowserTestData{
            .installed_apps = {kApp1, kApp2},
            .allowlisted_capture_apps = {kApp1, kApp2},
            .skip_notification_apps = {},
            .capturing_apps = {kApp1},
            .expected_icon_notification_title_before_capture =
                u"Your administrator can record your screen with app 1 and "
                u"app 2. You will be notified when the recording starts.",
            .expected_icon_notification_title_after_capture =
                u"Your administrator can record your screen with app 2. You "
                u"will be notified when the recording starts.",
            .expected_no_icon_notification_message_after_capture =
                u"Your administrator is recording your screen with app 1."},
        // New test case: Two apps installed and two allowlisted --> Standard
        // notification. Both are capturing --> show a notification for both
        // (and remove the future capture notification).
        MultiCaptureUsageIndicatorBrowserTestData{
            .installed_apps = {kApp1, kApp2},
            .allowlisted_capture_apps = {kApp1, kApp2},
            .skip_notification_apps = {},
            .capturing_apps = {kApp1, kApp2},
            .expected_icon_notification_title_before_capture =
                u"Your administrator can record your screen with app 1 and "
                u"app 2. You will be notified when the recording starts.",
            .expected_icon_notification_title_after_capture =
                u"Your administrator is recording your screen with app 1.",
            .expected_no_icon_notification_message_after_capture =
                u"Your administrator is recording your screen with app 2."},
        // New test case: One app installed and one allowlisted --> Bypass
        // notification.
        MultiCaptureUsageIndicatorBrowserTestData{
            .installed_apps = {kApp1},
            .allowlisted_capture_apps = {kApp1},
            .skip_notification_apps = {kApp1},
            .capturing_apps = {},
            .expected_icon_notification_title_before_capture =
                u"Your administrator can record your screen with app 1. You "
                u"will not be notified when the recording starts.",
            .expected_icon_notification_title_after_capture =
                u"Your administrator can record your screen with app 1. You "
                u"will not be notified when the recording starts.",
            .expected_no_icon_notification_message_after_capture =
                std::nullopt},
        // New test case: One app installed and two allowlisted --> Bypass
        // notification for one app.
        MultiCaptureUsageIndicatorBrowserTestData{
            .installed_apps = {kApp1},
            .allowlisted_capture_apps = {kApp1, kApp2},
            .skip_notification_apps = {kApp1},
            .capturing_apps = {kApp1},
            .expected_icon_notification_title_before_capture =
                u"Your administrator can record your screen with app 1. You "
                u"will not be notified when the recording starts.",
            .expected_icon_notification_title_after_capture =
                u"Your administrator can record your screen with app 1. You "
                u"will not be notified when the recording starts.",
            .expected_no_icon_notification_message_after_capture =
                std::nullopt},
        // New test case: Two apps installed and two allowlisted --> Bypass
        // notification for one app. App that isn't allowed to bypass is
        // capturing.
        MultiCaptureUsageIndicatorBrowserTestData{
            .installed_apps = {kApp1, kApp2},
            .allowlisted_capture_apps = {kApp1, kApp2},
            .skip_notification_apps = {kApp1},
            .capturing_apps = {kApp2},
            .expected_icon_notification_title_before_capture =
                u"Your administrator can record your screen with app 1 and app "
                u"2.",
            .expected_icon_notification_title_after_capture =
                u"Your administrator can record your screen with app 1. You "
                u"will not be notified when the recording starts.",
            .expected_no_icon_notification_message_after_capture =
                u"Your administrator is recording your screen with app 2."},
        // New test case: Two apps installed and two allowlisted --> Bypass
        // notification for one app. App that is allowed to bypass is
        // capturing --> the notification does not change.
        MultiCaptureUsageIndicatorBrowserTestData{
            .installed_apps = {kApp1, kApp2},
            .allowlisted_capture_apps = {kApp1, kApp2},
            .skip_notification_apps = {kApp1},
            .capturing_apps = {kApp1},
            .expected_icon_notification_title_before_capture =
                u"Your administrator can record your screen with app 1 and app "
                u"2.",
            .expected_icon_notification_title_after_capture =
                u"Your administrator can record your screen with app 1 and app "
                u"2.",
            .expected_no_icon_notification_message_after_capture =
                std::nullopt},
        // New test case: Two apps installed and two allowlisted --> Bypass
        // notification for both apps. One app capturing, notification doesn't
        // change.
        MultiCaptureUsageIndicatorBrowserTestData{
            .installed_apps = {kApp1, kApp2},
            .allowlisted_capture_apps = {kApp1, kApp2},
            .skip_notification_apps = {kApp1, kApp2},
            .capturing_apps = {kApp1, kApp2},
            .expected_icon_notification_title_before_capture =
                u"Your administrator can record your screen with app 1 and app "
                u"2. You will not be notified when the recording starts.",
            .expected_icon_notification_title_after_capture =
                u"Your administrator can record your screen with app 1 and app "
                u"2. You will not be notified when the recording starts.",
            .expected_no_icon_notification_message_after_capture =
                std::nullopt},
    }));

}  // namespace multi_capture

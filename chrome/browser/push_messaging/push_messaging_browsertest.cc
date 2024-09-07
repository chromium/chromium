// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/barrier_closure.h"
#include "base/base64url.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/permissions/crowd_deny_fake_safe_browsing_database_manager.h"
#include "chrome/browser/permissions/crowd_deny_preload_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"
#include "chrome/browser/push_messaging/push_messaging_constants.h"
#include "chrome/browser/push_messaging/push_messaging_features.h"
#include "chrome/browser/push_messaging/push_messaging_service_factory.h"
#include "chrome/browser/push_messaging/push_messaging_service_impl.h"
#include "chrome/browser/push_messaging/push_messaging_utils.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/instance_id/fake_gcm_driver_for_instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/permissions/permission_request_manager.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "ui/message_center/public/cpp/notification.h"

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/background/background_mode_manager.h"
#endif

namespace {

const char kManifestSenderId[] = "1234567890";
const int32_t kApplicationServerKeyLength = 65;

enum class PushSubscriptionKeyFormat { kOmitKey, kBinary, kBase64UrlEncoded };

// NIST P-256 public key made available to tests. Must be an uncompressed
// point in accordance with SEC1 2.3.3.
const uint8_t kApplicationServerKey[kApplicationServerKeyLength] = {
    0x04, 0x55, 0x52, 0x6A, 0xA5, 0x6E, 0x8E, 0xAA, 0x47, 0x97, 0x36,
    0x10, 0xC1, 0x66, 0x3C, 0x1E, 0x65, 0xBF, 0xA1, 0x7B, 0xEE, 0x48,
    0xC9, 0xC6, 0xBB, 0xBF, 0x02, 0x18, 0x53, 0x72, 0x1D, 0x0C, 0x7B,
    0xA9, 0xE3, 0x11, 0xB7, 0x03, 0x52, 0x21, 0xD3, 0x71, 0x90, 0x13,
    0xA8, 0xC1, 0xCF, 0xED, 0x20, 0xF7, 0x1F, 0xD1, 0x7F, 0xF2, 0x76,
    0xB6, 0x01, 0x20, 0xD8, 0x35, 0xA5, 0xD9, 0x3C, 0x43, 0xFD};

// URL-safe base64 encoded version of the |kApplicationServerKey|.
const char kEncodedApplicationServerKey[] =
    "BFVSaqVujqpHlzYQwWY8HmW_oXvuSMnGu78CGFNyHQx7qeMRtwNSIdNxkBOowc_tIPcf0X_ydr"
    "YBINg1pdk8Q_0";

// From chrome/browser/push_messaging/push_messaging_manager.cc
const char* kIncognitoWarningPattern =
    "Chrome currently does not support the Push API in incognito mode "
    "(https://crbug.com/401439). There is deliberately no way to "
    "feature-detect this, since incognito mode needs to be undetectable by "
    "websites.";

std::string GetTestApplicationServerKey(bool base64_url_encoded = false) {
  std::string application_server_key;

  if (base64_url_encoded) {
    base::Base64UrlEncode(reinterpret_cast<const char*>(kApplicationServerKey),
                          base::Base64UrlEncodePolicy::OMIT_PADDING,
                          &application_server_key);
  } else {
    application_server_key =
        std::string(kApplicationServerKey,
                    kApplicationServerKey + std::size(kApplicationServerKey));
  }

  return application_server_key;
}

void LegacyRegisterCallback(base::OnceClosure done_callback,
                            std::string* out_registration_id,
                            gcm::GCMClient::Result* out_result,
                            const std::string& registration_id,
                            gcm::GCMClient::Result result) {
  if (out_registration_id)
    *out_registration_id = registration_id;
  if (out_result)
    *out_result = result;
  std::move(done_callback).Run();
}

void DidRegister(base::OnceClosure done_callback,
                 const std::string& registration_id,
                 const GURL& endpoint,
                 const std::optional<base::Time>& expiration_time,
                 const std::vector<uint8_t>& p256dh,
                 const std::vector<uint8_t>& auth,
                 blink::mojom::PushRegistrationStatus status) {
  EXPECT_EQ(blink::mojom::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE,
            status);
  std::move(done_callback).Run();
}

void InstanceIDResultCallback(base::OnceClosure done_callback,
                              instance_id::InstanceID::Result* out_result,
                              instance_id::InstanceID::Result result) {
  DCHECK(out_result);
  *out_result = result;
  std::move(done_callback).Run();
}

}  // namespace

class PushMessagingBrowserTestBase : public InProcessBrowserTest {
 public:
  PushMessagingBrowserTestBase()
      : scoped_testing_factory_installer_(
            base::BindRepeating(&gcm::FakeGCMProfileService::Build)),
        gcm_service_(nullptr),
        gcm_driver_(nullptr) {}

  ~PushMessagingBrowserTestBase() override = default;

  PushMessagingBrowserTestBase(const PushMessagingBrowserTestBase&) = delete;
  PushMessagingBrowserTestBase& operator=(const PushMessagingBrowserTestBase&) =
      delete;

  // InProcessBrowserTest:
  void SetUp() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    content::SetupCrossSiteRedirector(https_server_.get());

    site_engagement::SiteEngagementScore::SetParamValuesForTesting();
    InProcessBrowserTest::SetUp();
  }
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable experimental features for subscription restrictions.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);

    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load webby domains like "embedded.com" without an interstitial.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(https_server_->Start());

    KeyedService* keyed_service =
        gcm::GCMProfileServiceFactory::GetForProfile(GetBrowser()->profile());
    if (keyed_service) {
      gcm_service_ = static_cast<gcm::FakeGCMProfileService*>(keyed_service);
      gcm_driver_ = static_cast<instance_id::FakeGCMDriverForInstanceID*>(
          gcm_service_->driver());
    }

    notification_tester_ = std::make_unique<NotificationDisplayServiceTester>(
        GetBrowser()->profile());

    push_service_ =
        PushMessagingServiceFactory::GetForProfile(GetBrowser()->profile());

    LoadTestPage();
  }

  void TearDownOnMainThread() override {
    notification_tester_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  // Calls should be wrapped in the ASSERT_NO_FATAL_FAILURE() macro.
  void RestartPushService() {
    Profile* profile = GetBrowser()->profile();
    PushMessagingServiceFactory::GetInstance()->SetTestingFactory(
        profile, BrowserContextKeyedServiceFactory::TestingFactory());
    ASSERT_EQ(nullptr, PushMessagingServiceFactory::GetForProfile(profile));
    PushMessagingServiceFactory::GetInstance()->RestoreFactoryForTests(profile);
    PushMessagingServiceImpl::InitializeForProfile(profile);
    push_service_ = PushMessagingServiceFactory::GetForProfile(profile);
  }

  // Helper function to test if a Keep Alive is registered while avoiding the
  // platform checks. Returns a boolean so that assertion failures are reported
  // at the right line.
  // Returns true when KeepAlives are not supported by the platform, or when
  // the registration state is equal to the expectation.
  bool IsRegisteredKeepAliveEqualTo(bool expectation) {
#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
    return expectation ==
           KeepAliveRegistry::GetInstance()->IsOriginRegistered(
               KeepAliveOrigin::IN_FLIGHT_PUSH_MESSAGE);
#else
    return true;
#endif
  }

  void LoadTestPage(const std::string& path) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowser(),
                                             https_server_->GetURL(path)));
  }

  void LoadTestPage() { LoadTestPage(GetTestURL()); }

  void LoadTestPageWithoutManifest() { LoadTestPage(GetNoManifestTestURL()); }

  content::EvalJsResult RunScript(const std::string& script) {
    return RunScript(script, nullptr);
  }

  content::EvalJsResult RunScript(const std::string& script,
                                  content::WebContents* web_contents) {
    if (!web_contents) {
      web_contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
    }
    return content::EvalJs(web_contents->GetPrimaryMainFrame(), script);
  }

  gcm::GCMAppHandler* GetAppHandler() {
    return gcm_driver_->GetAppHandler(kPushMessagingAppIdentifierPrefix);
  }

  permissions::PermissionRequestManager* GetPermissionRequestManager() {
    return permissions::PermissionRequestManager::FromWebContents(
        GetBrowser()->tab_strip_model()->GetActiveWebContents());
  }

  // Calls should be wrapped in the ASSERT_NO_FATAL_FAILURE() macro.
  void RequestAndAcceptPermission();
  // Calls should be wrapped in the ASSERT_NO_FATAL_FAILURE() macro.
  void RequestAndDenyPermission();

  // Sets out_token to the subscription token (not including server URL).
  // Calls should be wrapped in the ASSERT_NO_FATAL_FAILURE() macro.
  void SubscribeSuccessfully(
      PushSubscriptionKeyFormat key_format = PushSubscriptionKeyFormat::kBinary,
      std::string* out_token = nullptr);

  // Sets up the state corresponding to a dangling push subscription whose
  // service worker registration no longer exists. Some users may be left with
  // such orphaned subscriptions due to service worker unregistrations not
  // clearing push subscriptions in the past. This allows us to emulate that.
  // Calls should be wrapped in the ASSERT_NO_FATAL_FAILURE() macro.
  void SetupOrphanedPushSubscription(std::string* out_app_id);

  // Legacy subscribe path using GCMDriver rather than Instance IDs. Only
  // for testing that we maintain support for existing stored registrations.
  // Calls should be wrapped in the ASSERT_NO_FATAL_FAILURE() macro.
  void LegacySubscribeSuccessfully(std::string* out_subscription_id = nullptr);

  // Strips server URL from a registration endpoint to get subscription token.
  // Calls should be wrapped in the ASSERT_NO_FATAL_FAILURE() macro.
  void EndpointToToken(const std::string& endpoint,
                       bool standard_protocol = true,
                       std::string* out_token = nullptr);

  blink::mojom::PushSubscriptionPtr GetSubscriptionForAppIdentifier(
      const PushMessagingAppIdentifier& app_identifier) {
    blink::mojom::PushSubscriptionPtr result;
    base::RunLoop run_loop;
    push_service_->GetPushSubscriptionFromAppIdentifier(
        app_identifier,
        base::BindLambdaForTesting(
            [&](blink::mojom::PushSubscriptionPtr subscription) {
              result = std::move(subscription);
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  // Deletes an Instance ID from the GCM Store but keeps the push subscription
  // stored in the PushMessagingAppIdentifier map and Service Worker DB.
  // Calls should be wrapped in the ASSERT_NO_FATAL_FAILURE() macro.
  void DeleteInstanceIDAsIfGCMStoreReset(const std::string& app_id);

  PushMessagingAppIdentifier GetAppIdentifierForServiceWorkerRegistration(
      int64_t service_worker_registration_id);

  void SendMessageAndWaitUntilHandled(
      const PushMessagingAppIdentifier& app_identifier,
      const gcm::IncomingMessage& message);

  net::EmbeddedTestServer* https_server() const { return https_server_.get(); }

  // Returns a vector of the currently displayed Notification objects.
  std::vector<message_center::Notification> GetDisplayedNotifications() {
    return notification_tester_->GetDisplayedNotificationsForType(
        NotificationHandler::Type::WEB_PERSISTENT);
  }

  // Returns the number of notifications that are currently being shown.
  size_t GetNotificationCount() { return GetDisplayedNotifications().size(); }

  // Removes all shown notifications.
  void RemoveAllNotifications() {
    notification_tester_->RemoveAllNotifications(
        NotificationHandler::Type::WEB_PERSISTENT, true /* by_user */);
  }

  // To be called when delivery of a push message has finished. The |run_loop|
  // will be told to quit after |messages_required| messages were received.
  void OnDeliveryFinished(std::vector<size_t>* number_of_notifications_shown,
                          base::OnceClosure done_closure) {
    DCHECK(number_of_notifications_shown);
    number_of_notifications_shown->push_back(GetNotificationCount());

    std::move(done_closure).Run();
  }

  PushMessagingServiceImpl* push_service() const { return push_service_; }

  void SetSiteEngagementScore(const GURL& url, double score) {
    site_engagement::SiteEngagementService* service =
        site_engagement::SiteEngagementService::Get(GetBrowser()->profile());
    service->ResetBaseScoreForURL(url, score);
    EXPECT_EQ(score, service->GetScore(url));
  }

  // Matches |tag| against the notification's ID to see if the notification's
  // js-provided tag could have been |tag|. This is not perfect as it might
  // return true for a |tag| that is a substring of the original tag.
  static bool TagEquals(const message_center::Notification& notification,
                        const std::string& tag) {
    return std::string::npos != notification.id().find(tag);
  }

 protected:
  virtual std::string GetTestURL() { return "/push_messaging/test.html"; }

  virtual std::string GetNoManifestTestURL() {
    return "/push_messaging/test_no_manifest.html";
  }

  virtual Browser* GetBrowser() const { return browser(); }

  gcm::GCMProfileServiceFactory::ScopedTestingFactoryInstaller
      scoped_testing_factory_installer_;

  raw_ptr<gcm::FakeGCMProfileService, DanglingUntriaged> gcm_service_;
  raw_ptr<instance_id::FakeGCMDriverForInstanceID, DanglingUntriaged>
      gcm_driver_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  raw_ptr<PushMessagingServiceImpl, DanglingUntriaged> push_service_;
};

void PushMessagingBrowserTestBase::RequestAndAcceptPermission() {
  GetPermissionRequestManager()->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  ASSERT_EQ("permission status - granted",
            RunScript("requestNotificationPermission();"));
}

void PushMessagingBrowserTestBase::RequestAndDenyPermission() {
  GetPermissionRequestManager()->set_auto_response_for_test(
      permissions::PermissionRequestManager::DENY_ALL);
  ASSERT_EQ("permission status - denied",
            RunScript("requestNotificationPermission();"));
}

void PushMessagingBrowserTestBase::SubscribeSuccessfully(
    PushSubscriptionKeyFormat key_format,
    std::string* out_token) {
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_NO_FATAL_FAILURE(RequestAndAcceptPermission());

  switch (key_format) {
    case PushSubscriptionKeyFormat::kBinary:
      ASSERT_EQ("manifest removed", RunScript("removeManifest()"));

      ASSERT_NO_FATAL_FAILURE(
          EndpointToToken(RunScript("documentSubscribePush()").ExtractString(),
                          true, out_token));
      break;
    case PushSubscriptionKeyFormat::kBase64UrlEncoded:
      ASSERT_EQ("manifest removed", RunScript("removeManifest()"));

      ASSERT_NO_FATAL_FAILURE(EndpointToToken(
          RunScript("documentSubscribePushWithBase64URLEncodedString()")
              .ExtractString(),
          true, out_token));
      break;
    case PushSubscriptionKeyFormat::kOmitKey:
      // Test backwards compatibility with old ID based subscriptions.
      ASSERT_NO_FATAL_FAILURE(EndpointToToken(
          RunScript("documentSubscribePushWithoutKey()").ExtractString(), false,
          out_token));
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void PushMessagingBrowserTestBase::SetupOrphanedPushSubscription(
    std::string* out_app_id) {
  ASSERT_NO_FATAL_FAILURE(RequestAndAcceptPermission());
  GURL requesting_origin =
      https_server()->GetURL("/").DeprecatedGetOriginAsURL();
  // Use 1234LL as it's unlikely to collide with an active service worker
  // registration id (they increment from 0).
  const int64_t service_worker_registration_id = 1234LL;

  auto options = blink::mojom::PushSubscriptionOptions::New();
  options->user_visible_only = true;

  std::string test_application_server_key = GetTestApplicationServerKey();
  options->application_server_key = std::vector<uint8_t>(
      test_application_server_key.begin(), test_application_server_key.end());

  base::RunLoop run_loop;
  push_service()->SubscribeFromWorker(
      requesting_origin, service_worker_registration_id,
      /*render_process_id=*/-1, std::move(options),
      base::BindOnce(&DidRegister, run_loop.QuitClosure()));
  run_loop.Run();

  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(
          GetBrowser()->profile(), requesting_origin,
          service_worker_registration_id);
  ASSERT_FALSE(app_identifier.is_null());
  *out_app_id = app_identifier.app_id();
}

void PushMessagingBrowserTestBase::LegacySubscribeSuccessfully(
    std::string* out_subscription_id) {
  // Create a non-InstanceID GCM registration. Have to directly access
  // GCMDriver, since this codepath has been deleted from Push.

  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_NO_FATAL_FAILURE(RequestAndAcceptPermission());

  GURL requesting_origin =
      https_server()->GetURL("/").DeprecatedGetOriginAsURL();
  int64_t service_worker_registration_id = 0LL;
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::LegacyGenerateForTesting(
          requesting_origin, service_worker_registration_id);
  push_service_->IncreasePushSubscriptionCount(1, true /* is_pending */);

  std::string subscription_id;
  {
    base::RunLoop run_loop;
    gcm::GCMClient::Result register_result = gcm::GCMClient::UNKNOWN_ERROR;
    gcm_driver_->Register(
        app_identifier.app_id(), {kManifestSenderId},
        base::BindOnce(&LegacyRegisterCallback, run_loop.QuitClosure(),
                       &subscription_id, &register_result));
    run_loop.Run();
    ASSERT_EQ(gcm::GCMClient::SUCCESS, register_result);
  }

  app_identifier.PersistToPrefs(GetBrowser()->profile());
  push_service_->IncreasePushSubscriptionCount(1, false /* is_pending */);
  push_service_->DecreasePushSubscriptionCount(1, true /* was_pending */);

  {
    base::RunLoop run_loop;
    push_service_->StorePushSubscriptionForTesting(
        GetBrowser()->profile(), requesting_origin,
        service_worker_registration_id, subscription_id, kManifestSenderId,
        run_loop.QuitClosure());
    run_loop.Run();
  }

  if (out_subscription_id)
    *out_subscription_id = subscription_id;
}

void PushMessagingBrowserTestBase::EndpointToToken(const std::string& endpoint,
                                                   bool standard_protocol,
                                                   std::string* out_token) {
  size_t last_slash = endpoint.rfind('/');
  ASSERT_NE(last_slash, std::string::npos);

  ASSERT_EQ(base::FeatureList::IsEnabled(
                features::kPushMessagingGcmEndpointEnvironment)
                ? push_messaging::GetGcmEndpointForChannel(chrome::GetChannel())
                : kPushMessagingGcmEndpoint,
            endpoint.substr(0, last_slash + 1));

  ASSERT_LT(last_slash + 1, endpoint.length());  // Token must not be empty.

  if (out_token)
    *out_token = endpoint.substr(last_slash + 1);
}

PushMessagingAppIdentifier
PushMessagingBrowserTestBase::GetAppIdentifierForServiceWorkerRegistration(
    int64_t service_worker_registration_id) {
  GURL origin = https_server()->GetURL("/").DeprecatedGetOriginAsURL();
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(
          GetBrowser()->profile(), origin, service_worker_registration_id);
  EXPECT_FALSE(app_identifier.is_null());
  return app_identifier;
}

void PushMessagingBrowserTestBase::DeleteInstanceIDAsIfGCMStoreReset(
    const std::string& app_id) {
  // Delete the Instance ID directly, keeping the push subscription stored in
  // the PushMessagingAppIdentifier map and the Service Worker database. This
  // simulates the GCM Store getting reset but failing to clear push
  // subscriptions, either because the store got reset before
  // 93ec793ac69a542b2213297737178a55d069fd0d (Chrome 56), or because a race
  // condition (e.g. shutdown) prevents PushMessagingServiceImpl::OnStoreReset
  // from clearing all subscriptions.
  instance_id::InstanceIDProfileService* instance_id_profile_service =
      instance_id::InstanceIDProfileServiceFactory::GetForProfile(
          GetBrowser()->profile());
  DCHECK(instance_id_profile_service);
  instance_id::InstanceIDDriver* instance_id_driver =
      instance_id_profile_service->driver();
  DCHECK(instance_id_driver);
  instance_id::InstanceID::Result delete_result =
      instance_id::InstanceID::UNKNOWN_ERROR;
  base::RunLoop run_loop;
  instance_id_driver->GetInstanceID(app_id)->DeleteID(base::BindOnce(
      &InstanceIDResultCallback, run_loop.QuitClosure(), &delete_result));
  run_loop.Run();
  ASSERT_EQ(instance_id::InstanceID::SUCCESS, delete_result);
}

void PushMessagingBrowserTestBase::SendMessageAndWaitUntilHandled(
    const PushMessagingAppIdentifier& app_identifier,
    const gcm::IncomingMessage& message) {
  base::RunLoop run_loop;
  push_service()->SetMessageCallbackForTesting(run_loop.QuitClosure());
  push_service()->OnMessage(app_identifier.app_id(), message);
  run_loop.Run();
}

class PushMessagingBrowserTest : public PushMessagingBrowserTestBase {
 public:
  PushMessagingBrowserTest() {
    disabled_features_.push_back(features::kPushMessagingDisallowSenderIDs);
  }

  void SetUp() override {
    feature_list_.InitWithFeatures(enabled_features_, disabled_features_);
    PushMessagingBrowserTestBase::SetUp();
  }

 protected:
  std::vector<base::test::FeatureRef> enabled_features_{};
  std::vector<base::test::FeatureRef> disabled_features_{};

 private:
  base::test::ScopedFeatureList feature_list_;
};

// This class is used to execute PushMessagingBrowserTest tests with
// third-party storage partitioning both enabled/disabled.
class PushMessagingPartitionedBrowserTest
    : public PushMessagingBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  PushMessagingPartitionedBrowserTest() {
    if (GetParam()) {
      enabled_features_.push_back(
          net::features::kThirdPartyStoragePartitioning);
    } else {
      disabled_features_.push_back(
          net::features::kThirdPartyStoragePartitioning);
    }
  }
};

INSTANTIATE_TEST_SUITE_P(PushMessagingPartitionedBrowserTest,
                         PushMessagingPartitionedBrowserTest,
                         testing::Values(true, false));

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       SubscribeWithoutKeySuccessNotificationsGranted) {
  ASSERT_NO_FATAL_FAILURE(
      SubscribeSuccessfully(PushSubscriptionKeyFormat::kOmitKey));
  EXPECT_EQ(kManifestSenderId, gcm_driver_->last_gettoken_authorized_entity());
  EXPECT_EQ(GetAppIdentifierForServiceWorkerRegistration(0LL).app_id(),
            gcm_driver_->last_gettoken_app_id());
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       SubscribeSuccessNotificationsGranted) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());
  EXPECT_EQ(kEncodedApplicationServerKey,
            gcm_driver_->last_gettoken_authorized_entity());
  EXPECT_EQ(GetAppIdentifierForServiceWorkerRegistration(0LL).app_id(),
            gcm_driver_->last_gettoken_app_id());
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       SubscribeSuccessNotificationsGrantedWithBase64URLKey) {
  ASSERT_NO_FATAL_FAILURE(
      SubscribeSuccessfully(PushSubscriptionKeyFormat::kBase64UrlEncoded));
  EXPECT_EQ(kEncodedApplicationServerKey,
            gcm_driver_->last_gettoken_authorized_entity());
  EXPECT_EQ(GetAppIdentifierForServiceWorkerRegistration(0LL).app_id(),
            gcm_driver_->last_gettoken_app_id());
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       SubscribeSuccessNotificationsPrompt) {
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  GetPermissionRequestManager()->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  // Both of these methods EXPECT that they succeed.
  ASSERT_NO_FATAL_FAILURE(
      EndpointToToken(RunScript("documentSubscribePush()").ExtractString()));
  GetAppIdentifierForServiceWorkerRegistration(0LL);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       SubscribeFailureNotificationsBlocked) {
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_NO_FATAL_FAILURE(RequestAndDenyPermission());

  EXPECT_EQ("NotAllowedError - Registration failed - permission denied",
            RunScript("documentSubscribePush()"));
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, SubscribeFailureNoManifest) {
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_NO_FATAL_FAILURE(RequestAndAcceptPermission());

  ASSERT_EQ("manifest removed", RunScript("removeManifest()"));

  EXPECT_EQ(
      "AbortError - Registration failed - missing applicationServerKey, and "
      "manifest empty or missing",
      RunScript("documentSubscribePushWithoutKey()"));
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, SubscribeFailureNoSenderId) {
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_NO_FATAL_FAILURE(RequestAndAcceptPermission());

  ASSERT_EQ("sender id removed from manifest",
            RunScript("swapManifestNoSenderId()"));

  EXPECT_EQ(
      "AbortError - Registration failed - missing applicationServerKey, and "
      "gcm_sender_id not found in manifest",
      RunScript("documentSubscribePushWithoutKey()"));
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       RegisterFailureEmptyPushSubscriptionOptions) {
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_NO_FATAL_FAILURE(RequestAndAcceptPermission());

  EXPECT_EQ("NotAllowedError - Registration failed - permission denied",
            RunScript("documentSubscribePushWithEmptyOptions()"));
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, SubscribeWithInvalidation) {
  std::string token1, token2, token3;

  ASSERT_NO_FATAL_FAILURE(
      SubscribeSuccessfully(PushSubscriptionKeyFormat::kBinary, &token1));
  ASSERT_FALSE(token1.empty());

  // Repeated calls to |subscribe()| should yield the same token.
  ASSERT_NO_FATAL_FAILURE(
      SubscribeSuccessfully(PushSubscriptionKeyFormat::kBinary, &token2));
  ASSERT_EQ(token1, token2);

  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(
          GetBrowser()->profile(),
          https_server()->GetURL("/").DeprecatedGetOriginAsURL(),
          0LL /* service_worker_registration_id */);

  ASSERT_FALSE(app_identifier.is_null());
  EXPECT_EQ(app_identifier.app_id(), gcm_driver_->last_gettoken_app_id());

  // Delete the InstanceID. This captures two scenarios: either the database was
  // corrupted, or the subscription was invalidated by the server.
  ASSERT_NO_FATAL_FAILURE(
      DeleteInstanceIDAsIfGCMStoreReset(app_identifier.app_id()));

  EXPECT_EQ(app_identifier.app_id(), gcm_driver_->last_deletetoken_app_id());

  // Repeated calls to |subscribe()| will now (silently) result in a new token.
  ASSERT_NO_FATAL_FAILURE(
      SubscribeSuccessfully(PushSubscriptionKeyFormat::kBinary, &token3));
  ASSERT_FALSE(token3.empty());
  EXPECT_NE(token1, token3);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, SubscribeWorker) {
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_NO_FATAL_FAILURE(RequestAndAcceptPermission());

  LoadTestPage();  // Reload to become controlled.

  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  // Try to subscribe from a worker without a key. This should fail.
  EXPECT_EQ(
      "AbortError - Registration failed - missing applicationServerKey, and "
      "gcm_sender_id not found in manifest",
      RunScript("workerSubscribePushNoKey()"));

  // Now run the subscribe with a key. This should succeed.
  ASSERT_NO_FATAL_FAILURE(
      EndpointToToken(RunScript("workerSubscribePush()").ExtractString(),
                      true /* standard_protocol */));

  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       SubscribeWorkerWithBase64URLEncodedString) {
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_NO_FATAL_FAILURE(RequestAndAcceptPermission());

  LoadTestPage();  // Reload to become controlled.

  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  // Try to subscribe from a worker without a key. This should fail.
  EXPECT_EQ(
      "AbortError - Registration failed - missing applicationServerKey, and "
      "gcm_sender_id not found in manifest",
      RunScript("workerSubscribePushNoKey()"));

  // Now run the subscribe with a key. This should succeed.
  ASSERT_NO_FATAL_FAILURE(EndpointToToken(
      RunScript("workerSubscribePushWithBase64URLEncodedString()")
          .ExtractString(),
      true /* standard_protocol */));

  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       ResubscribeWithoutKeyAfterSubscribingWithKeyInManifest) {
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_NO_FATAL_FAILURE(RequestAndAcceptPermission());

  LoadTestPage();  // Reload to become controlled.

  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  // Run the subscription from the document without a key, this will trigger
  // the code to read sender id from the manifest and will write it to the
  // datastore.
  std::string token1;
  ASSERT_NO_FATAL_FAILURE(EndpointToToken(
      RunScript("documentSubscribePushWithoutKey()").ExtractString(),
      false /* standard_protocol */, &token1));

  ASSERT_EQ("manifest removed", RunScript("removeManifest()"));

  // Try to resubscribe from the document without a key or manifest.
  // This should fail.
  EXPECT_EQ(
      "AbortError - Registration failed - missing applicationServerKey, "
      "and manifest empty or missing",
      RunScript("documentSubscribePushWithoutKey()"));

  // Now run the subscribe from the service worker without a key.
  // In this case, the sender id should be read from the datastore.
  std::string token2;
  ASSERT_NO_FATAL_FAILURE(
      EndpointToToken(RunScript("workerSubscribePushNoKey()").ExtractString(),
                      false /* standard_protocol */, &token2));
  EXPECT_EQ(token1, token2);

  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));

  // After unsubscribing, subscribe again from the worker with no key.
  // The sender id should again be read from the datastore, so the
  // subcribe should succeed, and we should get a new subscription token.
  std::string token3;
  ASSERT_NO_FATAL_FAILURE(
      EndpointToToken(RunScript("workerSubscribePushNoKey()").ExtractString(),
                      false /* standard_protocol */, &token3));
  EXPECT_NE(token1, token3);

  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));
}

IN_PROC_BROWSER_TEST_F(
    PushMessagingBrowserTest,
    ResubscribeWithoutKeyAfterSubscribingFromDocumentWithP256Key) {
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_NO_FATAL_FAILURE(RequestAndAcceptPermission());

  LoadTestPageWithoutManifest();  // Reload to become controlled.

  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  // Run the subscription from the document with a key.
  ASSERT_NO_FATAL_FAILURE(
      EndpointToToken(RunScript("documentSubscribePush()").ExtractString()));

  // Try to resubscribe from the document without a key - should fail.
  EXPECT_EQ(
      "AbortError - Registration failed - missing applicationServerKey, "
      "and manifest empty or missing",
      RunScript("documentSubscribePushWithoutKey()"));

  // Now try to resubscribe from the service worker without a key.
  // This should also fail as the original key was not numeric.
  EXPECT_EQ(
      "AbortError - Registration failed - missing applicationServerKey, "
      "and gcm_sender_id not found in manifest",
      RunScript("workerSubscribePushNoKey()"));

  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));

  // After unsubscribing, try to resubscribe again without a key.
  // This should again fail.
  EXPECT_EQ(
      "AbortError - Registration failed - missing applicationServerKey, "
      "and gcm_sender_id not found in manifest",
      RunScript("workerSubscribePushNoKey()"));
}

IN_PROC_BROWSER_TEST_F(
    PushMessagingBrowserTest,
    ResubscribeWithoutKeyAfterSubscribingFromWorkerWithP256Key) {
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_NO_FATAL_FAILURE(RequestAndAcceptPermission());

  LoadTestPageWithoutManifest();  // Reload to become controlled.

  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  // Run the subscribe from the service worker with a key.
  // This should succeed.
  ASSERT_NO_FATAL_FAILURE(
      EndpointToToken(RunScript("workerSubscribePush()").ExtractString(),
                      true /* standard_protocol */));

  // Try to resubscribe from the document without a key - should fail.
  EXPECT_EQ(
      "AbortError - Registration failed - missing applicationServerKey, "
      "and manifest empty or missing",
      RunScript("documentSubscribePushWithoutKey()"));

  // Now try to resubscribe from the service worker without a key.
  // This should also fail as the original key was not numeric.
  EXPECT_EQ(
      "AbortError - Registration failed - missing applicationServerKey, and "
      "gcm_sender_id not found in manifest",
      RunScript("workerSubscribePushNoKey()"));

  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));

  // After unsubscribing, try to resubscribe again without a key.
  // This should again fail.
  EXPECT_EQ(
      "AbortError - Registration failed - missing applicationServerKey, "
      "and gcm_sender_id not found in manifest",
      RunScript("workerSubscribePushNoKey()"));
}

IN_PROC_BROWSER_TEST_F(
    PushMessagingBrowserTest,
    ResubscribeWithoutKeyAfterSubscribingFromDocumentWithNumber) {
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_NO_FATAL_FAILURE(RequestAndAcceptPermission());

  LoadTestPageWithoutManifest();  // Reload to become controlled.

  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  // Run the subscribe from the document with a numeric key.
  // This should succeed.

  std::string token1;
  ASSERT_NO_FATAL_FAILURE(EndpointToToken(
      RunScript("documentSubscribePushWithNumericKey()").ExtractString(),
      false /* standard_protocol */, &token1));

  // Try to resubscribe from the document without a key - should fail.
  EXPECT_EQ(
      "AbortError - Registration failed - missing applicationServerKey, "
      "and manifest empty or missing",
      RunScript("documentSubscribePushWithoutKey()"));

  // Now run the subscribe from the service worker without a key.
  // In this case, the sender id should be read from the datastore.
  // Note, we would rather this failed as we only really want to support
  // no-key subscribes after subscribing with a numeric gcm sender id in the
  // manifest, not a numeric applicationServerKey, but for code simplicity
  // this case is allowed.
  std::string token2;
  ASSERT_NO_FATAL_FAILURE(
      EndpointToToken(RunScript("workerSubscribePushNoKey()").ExtractString(),
                      false /* standard_protocol */, &token2));
  EXPECT_EQ(token1, token2);

  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));

  // After unsubscribing, subscribe again from the worker with no key.
  // The sender id should again be read from the datastore, so the
  // subcribe should succeed, and we should get a new subscription token.
  std::string token3;
  ASSERT_NO_FATAL_FAILURE(
      EndpointToToken(RunScript("workerSubscribePushNoKey()").ExtractString(),
                      false /* standard_protocol */, &token3));
  EXPECT_NE(token1, token3);

  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));
}

IN_PROC_BROWSER_TEST_F(
    PushMessagingBrowserTest,
    ResubscribeWithoutKeyAfterSubscribingFromWorkerWithNumber) {
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_NO_FATAL_FAILURE(RequestAndAcceptPermission());

  LoadTestPageWithoutManifest();  // Reload to become controlled.

  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  // Run the subscribe from the service worker with a numeric key.
  // This should succeed.
  std::string token1;
  ASSERT_NO_FATAL_FAILURE(EndpointToToken(
      RunScript("workerSubscribePushWithNumericKey()").ExtractString(),
      false /* standard_protocol */, &token1));

  // Try to resubscribe from the document without a key - should fail.
  EXPECT_EQ(
      "AbortError - Registration failed - missing applicationServerKey, "
      "and manifest empty or missing",
      RunScript("documentSubscribePushWithoutKey()"));

  // Now run the subscribe from the service worker without a key.
  // In this case, the sender id should be read from the datastore.
  // Note, we would rather this failed as we only really want to support
  // no-key subscribes after subscribing with a numeric gcm sender id in the
  // manifest, not a numeric applicationServerKey, but for code simplicity
  // this case is allowed.
  std::string token2;
  ASSERT_NO_FATAL_FAILURE(
      EndpointToToken(RunScript("workerSubscribePushNoKey()").ExtractString(),
                      false /* standard_protocol */, &token2));
  EXPECT_EQ(token1, token2);

  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));

  // After unsubscribing, subscribe again from the worker with no key.
  // The sender id should again be read from the datastore, so the
  // subcribe should succeed, and we should get a new subscription token.
  std::string token3;
  ASSERT_NO_FATAL_FAILURE(
      EndpointToToken(RunScript("workerSubscribePushNoKey()").ExtractString(),
                      false /* standard_protocol */, &token3));
  EXPECT_NE(token1, token3);

  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, ResubscribeWithMismatchedKey) {
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_NO_FATAL_FAILURE(RequestAndAcceptPermission());

  LoadTestPage();  // Reload to become controlled.

  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  // Run the subscribe from the service worker with a key.
  // This should succeed.
  std::string token1;
  ASSERT_NO_FATAL_FAILURE(EndpointToToken(
      RunScript("workerSubscribePushWithNumericKey('11111')").ExtractString(),
      false /* standard_protocol */, &token1));

  // Try to resubscribe with a different key - should fail.
  EXPECT_EQ(
      "InvalidStateError - Registration failed - A subscription with a "
      "different applicationServerKey (or gcm_sender_id) already exists; to "
      "change the applicationServerKey, unsubscribe then resubscribe.",
      RunScript("workerSubscribePushWithNumericKey('22222')"));

  // Try to resubscribe with the original key - should succeed.
  std::string token2;
  ASSERT_NO_FATAL_FAILURE(EndpointToToken(
      RunScript("workerSubscribePushWithNumericKey('11111')").ExtractString(),
      false /* standard_protocol */, &token2));
  EXPECT_EQ(token1, token2);

  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));

  // Resubscribe with a different key after unsubscribing.
  // Should succeed, and we should get a new subscription token.
  std::string token3;
  ASSERT_NO_FATAL_FAILURE(EndpointToToken(
      RunScript("workerSubscribePushWithNumericKey('22222')").ExtractString(),
      false /* standard_protocol */, &token3));
  EXPECT_NE(token1, token3);

  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, SubscribePersisted) {
  // First, test that Service Worker registration IDs are assigned in order of
  // registering the Service Workers, and the (fake) push subscription ids are
  // assigned in order of push subscription (even when these orders are
  // different).

  std::string token1;
  ASSERT_NO_FATAL_FAILURE(
      SubscribeSuccessfully(PushSubscriptionKeyFormat::kBinary, &token1));
  PushMessagingAppIdentifier sw0_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);
  EXPECT_EQ(sw0_identifier.app_id(), gcm_driver_->last_gettoken_app_id());

  LoadTestPage("/push_messaging/subscope1/test.html");
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  LoadTestPage("/push_messaging/subscope2/test.html");
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  // Note that we need to reload the page after registering, otherwise
  // navigator.serviceWorker.ready is going to be resolved with the parent
  // Service Worker which still controls the page.
  LoadTestPage("/push_messaging/subscope2/test.html");
  std::string token2;
  ASSERT_NO_FATAL_FAILURE(
      SubscribeSuccessfully(PushSubscriptionKeyFormat::kBinary, &token2));
  EXPECT_NE(token1, token2);
  PushMessagingAppIdentifier sw2_identifier =
      GetAppIdentifierForServiceWorkerRegistration(2LL);
  EXPECT_EQ(sw2_identifier.app_id(), gcm_driver_->last_gettoken_app_id());

  LoadTestPage("/push_messaging/subscope1/test.html");
  std::string token3;
  ASSERT_NO_FATAL_FAILURE(
      SubscribeSuccessfully(PushSubscriptionKeyFormat::kBinary, &token3));
  EXPECT_NE(token1, token3);
  EXPECT_NE(token2, token3);
  PushMessagingAppIdentifier sw1_identifier =
      GetAppIdentifierForServiceWorkerRegistration(1LL);
  EXPECT_EQ(sw1_identifier.app_id(), gcm_driver_->last_gettoken_app_id());

  // Now test that the Service Worker registration IDs and push subscription IDs
  // generated above were persisted to SW storage, by checking that they are
  // unchanged despite requesting them in a different order.

  LoadTestPage("/push_messaging/subscope1/test.html");
  std::string token4;
  ASSERT_NO_FATAL_FAILURE(
      SubscribeSuccessfully(PushSubscriptionKeyFormat::kBinary, &token4));
  EXPECT_EQ(token3, token4);
  EXPECT_EQ(sw1_identifier.app_id(), gcm_driver_->last_gettoken_app_id());

  LoadTestPage("/push_messaging/subscope2/test.html");
  std::string token5;
  ASSERT_NO_FATAL_FAILURE(
      SubscribeSuccessfully(PushSubscriptionKeyFormat::kBinary, &token5));
  EXPECT_EQ(token2, token5);
  EXPECT_EQ(sw2_identifier.app_id(), gcm_driver_->last_gettoken_app_id());

  LoadTestPage();
  std::string token6;
  ASSERT_NO_FATAL_FAILURE(
      SubscribeSuccessfully(PushSubscriptionKeyFormat::kBinary, &token6));
  EXPECT_EQ(token1, token6);
  EXPECT_EQ(sw0_identifier.app_id(), gcm_driver_->last_gettoken_app_id());
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, AppHandlerOnlyIfSubscribed) {
  // This test restarts the push service to simulate restarting the browser.

  EXPECT_NE(push_service(), GetAppHandler());
  ASSERT_NO_FATAL_FAILURE(RestartPushService());
  EXPECT_NE(push_service(), GetAppHandler());

  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());

  EXPECT_EQ(push_service(), GetAppHandler());
  ASSERT_NO_FATAL_FAILURE(RestartPushService());
  EXPECT_EQ(push_service(), GetAppHandler());

  // Unsubscribe.
  base::RunLoop run_loop;
  push_service()->SetUnsubscribeCallbackForTesting(run_loop.QuitClosure());
  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));
  // The app handler is only guaranteed to be unregistered once the unsubscribe
  // callback for testing has been run (PushSubscription.unsubscribe() usually
  // resolves before that, in order to avoid blocking on network retries etc).
  run_loop.Run();

  EXPECT_NE(push_service(), GetAppHandler());
  ASSERT_NO_FATAL_FAILURE(RestartPushService());
  EXPECT_NE(push_service(), GetAppHandler());
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, PushEventSuccess) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());
  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);

  ASSERT_EQ("false - is not controlled", RunScript("isControlled()"));
  LoadTestPage();  // Reload to become controlled.
  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  EXPECT_TRUE(IsRegisteredKeepAliveEqualTo(false));
  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.raw_data = "testdata";
  message.decrypted = true;
  push_service()->OnMessage(app_identifier.app_id(), message);
  EXPECT_TRUE(IsRegisteredKeepAliveEqualTo(true));
  EXPECT_EQ("testdata", RunScript("resultQueue.pop()"));

  // Check that we record this case in UMA.
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.DeliveryStatus",
      static_cast<int>(blink::mojom::PushEventStatus::SUCCESS), 1);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, PushEventOnShutdown) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());
  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);

  ASSERT_EQ("false - is not controlled", RunScript("isControlled()"));
  LoadTestPage();  // Reload to become controlled.
  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  EXPECT_TRUE(IsRegisteredKeepAliveEqualTo(false));
  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.raw_data = "testdata";
  message.decrypted = true;
  push_service()->OnAppTerminating();
  push_service()->OnMessage(app_identifier.app_id(), message);
  EXPECT_TRUE(IsRegisteredKeepAliveEqualTo(false));
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, PushEventWithoutPayload) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());
  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);

  LoadTestPage();  // Reload to become controlled.
  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.decrypted = false;

  push_service()->OnMessage(app_identifier.app_id(), message);
  EXPECT_EQ("[NULL]", RunScript("resultQueue.pop()"));
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, LegacyPushEvent) {
  ASSERT_NO_FATAL_FAILURE(LegacySubscribeSuccessfully());
  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);

  LoadTestPage();  // Reload to become controlled.
  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  gcm::IncomingMessage message;
  message.sender_id = kManifestSenderId;
  message.decrypted = false;

  push_service()->OnMessage(app_identifier.app_id(), message);
  EXPECT_EQ("[NULL]", RunScript("resultQueue.pop()"));
}

// Some users may have gotten into a state in the past where they still have
// a subscription even though the service worker was unregistered.
// Emulate this and test a push message triggers unsubscription.
IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, PushEventNoServiceWorker) {
  std::string app_id;
  ASSERT_NO_FATAL_FAILURE(SetupOrphanedPushSubscription(&app_id));

  // Try to send a push message.
  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.raw_data = "testdata";
  message.decrypted = true;

  base::RunLoop run_loop;
  push_service()->SetMessageCallbackForTesting(run_loop.QuitClosure());
  EXPECT_TRUE(IsRegisteredKeepAliveEqualTo(false));
  push_service()->OnMessage(app_id, message);
  EXPECT_TRUE(IsRegisteredKeepAliveEqualTo(true));
  run_loop.Run();
  EXPECT_TRUE(IsRegisteredKeepAliveEqualTo(false));

  // No push data should have been received.
  EXPECT_EQ("null", RunScript("String(resultQueue.popImmediately())"));

  // Check that we record this case in UMA.
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.DeliveryStatus",
      static_cast<int>(blink::mojom::PushEventStatus::NO_SERVICE_WORKER), 1);

  // Missing Service Workers should trigger an automatic unsubscription attempt.
  EXPECT_EQ(app_id, gcm_driver_->last_deletetoken_app_id());
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(
          blink::mojom::PushUnregistrationReason::DELIVERY_NO_SERVICE_WORKER),
      1);

  // |app_identifier| should no longer be stored in prefs.
  PushMessagingAppIdentifier stored_app_identifier =
      PushMessagingAppIdentifier::FindByAppId(GetBrowser()->profile(), app_id);
  EXPECT_TRUE(stored_app_identifier.is_null());
}

// Tests receiving messages for a subscription that no longer exists.
IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, NoSubscription) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());
  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);

  LoadTestPage();  // Reload to become controlled.
  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(blink::mojom::PushUnregistrationReason::JAVASCRIPT_API),
      1);

  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.raw_data = "testdata";
  message.decrypted = true;
  SendMessageAndWaitUntilHandled(app_identifier, message);

  // No push data should have been received.
  EXPECT_EQ("null", RunScript("String(resultQueue.popImmediately())"));

  // Check that we record this case in UMA.
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.DeliveryStatus",
      static_cast<int>(blink::mojom::PushEventStatus::UNKNOWN_APP_ID), 1);

  // Missing subscriptions should trigger an automatic unsubscription attempt.
  EXPECT_EQ(app_identifier.app_id(), gcm_driver_->last_deletetoken_app_id());
  histogram_tester_.ExpectBucketCount(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(
          blink::mojom::PushUnregistrationReason::DELIVERY_UNKNOWN_APP_ID),
      1);
}

// Tests receiving messages for an origin that does not have permission, but
// somehow still has a subscription (as happened in https://crbug.com/633310).
IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, PushEventWithoutPermission) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());
  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);

  LoadTestPage();  // Reload to become controlled.
  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  // Revoke notifications permission, but first disable the
  // PushMessagingServiceImpl's OnContentSettingChanged handler so that it
  // doesn't automatically unsubscribe, since we want to test the case where
  // there is still a subscription.
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->RemoveObserver(push_service());
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::NOTIFICATIONS);
  base::RunLoop().RunUntilIdle();

  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.raw_data = "testdata";
  message.decrypted = true;
  SendMessageAndWaitUntilHandled(app_identifier, message);

  // No push data should have been received.
  EXPECT_EQ("null", RunScript("String(resultQueue.popImmediately())"));

  // Check that we record this case in UMA.
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.DeliveryStatus",
      static_cast<int>(blink::mojom::PushEventStatus::PERMISSION_DENIED), 1);

  // Missing permission should trigger an automatic unsubscription attempt.
  EXPECT_EQ(app_identifier.app_id(), gcm_driver_->last_deletetoken_app_id());
  EXPECT_EQ("false - not subscribed", RunScript("hasSubscription()"));
  GURL origin = https_server()->GetURL("/").DeprecatedGetOriginAsURL();
  PushMessagingAppIdentifier app_identifier_afterwards =
      PushMessagingAppIdentifier::FindByServiceWorker(GetBrowser()->profile(),
                                                      origin, 0LL);
  EXPECT_TRUE(app_identifier_afterwards.is_null());
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(
          blink::mojom::PushUnregistrationReason::DELIVERY_PERMISSION_DENIED),
      1);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       PushEventEnforcesUserVisibleNotification) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());
  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);

  ASSERT_EQ("false - is not controlled", RunScript("isControlled()"));

  LoadTestPage();  // Reload to become controlled.

  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  RemoveAllNotifications();
  ASSERT_EQ(0u, GetNotificationCount());

  // We'll need to specify the web_contents in which to eval script, since we're
  // going to run script in a background tab.
  content::WebContents* web_contents =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();

  // Set the site engagement score for the site. Setting it to 10 means it
  // should have a budget of 4, enough for two non-shown notification, which
  // cost 2 each.
  SetSiteEngagementScore(web_contents->GetLastCommittedURL(), 10.0);

  // If the site is visible in an active tab, we should not force a notification
  // to be shown. Try it twice, since we allow one mistake per 10 push events.
  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.decrypted = true;
  for (int n = 0; n < 2; n++) {
    message.raw_data = "testdata";
    SendMessageAndWaitUntilHandled(app_identifier, message);
    EXPECT_EQ("testdata", RunScript("resultQueue.pop()"));
    EXPECT_EQ(0u, GetNotificationCount());
  }

  // Open a blank foreground tab so site is no longer visible.
  ui_test_utils::NavigateToURLWithDisposition(
      GetBrowser(), GURL("about:blank"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // If the Service Worker push event handler shows a notification, we
  // should not show a forced one.
  message.raw_data = "shownotification";
  SendMessageAndWaitUntilHandled(app_identifier, message);
  EXPECT_EQ("shownotification", RunScript("resultQueue.pop()", web_contents));
  EXPECT_EQ(1u, GetNotificationCount());
  EXPECT_TRUE(TagEquals(GetDisplayedNotifications()[0], "push_test_tag"));
  RemoveAllNotifications();

  // If the Service Worker push event handler does not show a notification, we
  // should show a forced one, but only once the origin is out of budget.
  message.raw_data = "testdata";
  for (int n = 0; n < 2; n++) {
    // First two missed notifications shouldn't force a default one.
    SendMessageAndWaitUntilHandled(app_identifier, message);
    EXPECT_EQ("testdata", RunScript("resultQueue.pop()", web_contents));
    EXPECT_EQ(0u, GetNotificationCount());
  }

  // Third missed notification should trigger a default notification, since the
  // origin will be out of budget.
  message.raw_data = "testdata";
  SendMessageAndWaitUntilHandled(app_identifier, message);
  EXPECT_EQ("testdata", RunScript("resultQueue.pop()", web_contents));

  {
    std::vector<message_center::Notification> notifications =
        GetDisplayedNotifications();
    ASSERT_EQ(notifications.size(), 1u);

    EXPECT_TRUE(
        TagEquals(notifications[0], kPushMessagingForcedNotificationTag));
    EXPECT_TRUE(notifications[0].silent());
  }

  // The notification will be automatically dismissed when the developer shows
  // a new notification themselves at a later point in time.
  base::RunLoop notification_closed_run_loop;
  notification_tester_->SetNotificationClosedClosure(
      notification_closed_run_loop.QuitClosure());

  message.raw_data = "shownotification";
  SendMessageAndWaitUntilHandled(app_identifier, message);
  EXPECT_EQ("shownotification", RunScript("resultQueue.pop()", web_contents));

  // Wait for the default notification to dismiss.
  notification_closed_run_loop.Run();

  {
    std::vector<message_center::Notification> notifications =
        GetDisplayedNotifications();
    ASSERT_EQ(notifications.size(), 1u);

    EXPECT_FALSE(
        TagEquals(notifications[0], kPushMessagingForcedNotificationTag));
  }
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       PushEventAllowSilentPushCommandLineFlag) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());
  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);
  EXPECT_EQ(app_identifier.app_id(), gcm_driver_->last_gettoken_app_id());
  EXPECT_EQ(kEncodedApplicationServerKey,
            gcm_driver_->last_gettoken_authorized_entity());

  ASSERT_EQ("false - is not controlled", RunScript("isControlled()"));

  LoadTestPage();  // Reload to become controlled.

  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  RemoveAllNotifications();
  ASSERT_EQ(0u, GetNotificationCount());

  // We'll need to specify the web_contents in which to eval script, since we're
  // going to run script in a background tab.
  content::WebContents* web_contents =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();

  SetSiteEngagementScore(web_contents->GetLastCommittedURL(), 5.0);

  ui_test_utils::NavigateToURLWithDisposition(
      GetBrowser(), GURL("about:blank"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // Send a missed notification to use up the budget.
  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.raw_data = "testdata";
  message.decrypted = true;

  SendMessageAndWaitUntilHandled(app_identifier, message);
  EXPECT_EQ("testdata", RunScript("resultQueue.pop()", web_contents));
  EXPECT_EQ(0u, GetNotificationCount());

  // If the Service Worker push event handler does not show a notification, we
  // should show a forced one providing there is no foreground tab and the
  // origin ran out of budget.
  SendMessageAndWaitUntilHandled(app_identifier, message);
  EXPECT_EQ("testdata", RunScript("resultQueue.pop()", web_contents));

  // Because the --allow-silent-push command line flag has not been passed,
  // this should have shown a default notification.
  {
    std::vector<message_center::Notification> notifications =
        GetDisplayedNotifications();
    ASSERT_EQ(notifications.size(), 1u);

    EXPECT_TRUE(
        TagEquals(notifications[0], kPushMessagingForcedNotificationTag));
    EXPECT_TRUE(notifications[0].silent());
  }

  RemoveAllNotifications();

  // Send the message again, but this time with the -allow-silent-push command
  // line flag set. The default notification should *not* be shown.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowSilentPush);

  SendMessageAndWaitUntilHandled(app_identifier, message);
  EXPECT_EQ("testdata", RunScript("resultQueue.pop()", web_contents));

  ASSERT_EQ(0u, GetNotificationCount());
}

class PushMessagingBrowserTestWithAbusiveOriginPermissionRevocation
    : public PushMessagingBrowserTestBase {
 public:
  PushMessagingBrowserTestWithAbusiveOriginPermissionRevocation() = default;

  using SiteReputation = CrowdDenyPreloadData::SiteReputation;

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    PushMessagingBrowserTestBase::CreatedBrowserMainParts(browser_main_parts);

    testing_preload_data_.emplace();
    fake_database_manager_ =
        base::MakeRefCounted<CrowdDenyFakeSafeBrowsingDatabaseManager>();
    test_safe_browsing_factory_ =
        std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>();
    test_safe_browsing_factory_->SetTestDatabaseManager(
        fake_database_manager_.get());
    safe_browsing::SafeBrowsingServiceInterface::RegisterFactory(
        test_safe_browsing_factory_.get());
  }

  void AddToPreloadDataBlocklist(
      const GURL& origin,
      chrome_browser_crowd_deny::
          SiteReputation_NotificationUserExperienceQuality reputation_type) {
    SiteReputation reputation;
    reputation.set_notification_ux_quality(reputation_type);
    testing_preload_data_->SetOriginReputation(url::Origin::Create(origin),
                                               std::move(reputation));
  }

  void AddToSafeBrowsingBlocklist(const GURL& url) {
    safe_browsing::ThreatMetadata test_metadata;
    test_metadata.api_permissions.emplace("NOTIFICATIONS");
    fake_database_manager_->SetSimulatedMetadataForUrl(url, test_metadata);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::optional<testing::ScopedCrowdDenyPreloadDataOverride>
      testing_preload_data_;
  scoped_refptr<CrowdDenyFakeSafeBrowsingDatabaseManager>
      fake_database_manager_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      test_safe_browsing_factory_;
};

IN_PROC_BROWSER_TEST_F(
    PushMessagingBrowserTestWithAbusiveOriginPermissionRevocation,
    PushEventPermissionRevoked) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());
  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);

  LoadTestPage();  // Reload to become controlled.
  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  // Add an origin to blocking lists after service worker is registered.
  AddToPreloadDataBlocklist(
      https_server()->GetURL("/").DeprecatedGetOriginAsURL(),
      SiteReputation::ABUSIVE_CONTENT);
  AddToSafeBrowsingBlocklist(
      https_server()->GetURL("/").DeprecatedGetOriginAsURL());

  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.raw_data = "testdata";
  message.decrypted = true;
  SendMessageAndWaitUntilHandled(app_identifier, message);

  // No push data should have been received.
  EXPECT_EQ("null", RunScript("String(resultQueue.popImmediately())"));

  // Check that we record this case in UMA.
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.DeliveryStatus",
      static_cast<int>(
          blink::mojom::PushEventStatus::PERMISSION_REVOKED_ABUSIVE),
      1);

  //   Missing permission should trigger an automatic unsubscription attempt.
  EXPECT_EQ(app_identifier.app_id(), gcm_driver_->last_deletetoken_app_id());
  EXPECT_EQ("false - not subscribed", RunScript("hasSubscription()"));
  GURL origin = https_server()->GetURL("/").DeprecatedGetOriginAsURL();
  PushMessagingAppIdentifier app_identifier_afterwards =
      PushMessagingAppIdentifier::FindByServiceWorker(GetBrowser()->profile(),
                                                      origin, 0LL);
  EXPECT_TRUE(app_identifier_afterwards.is_null());

  // 1st event - blink::mojom::PushUnregistrationReason::PERMISSION_REVOKED.
  // 2nd event -
  // blink::mojom::PushUnregistrationReason::PERMISSION_REVOKED_ABUSIVE.
  histogram_tester_.ExpectTotalCount("PushMessaging.UnregistrationReason", 2);

  histogram_tester_.ExpectBucketCount(
      "PushMessaging.UnregistrationReason",
      blink::mojom::PushUnregistrationReason::PERMISSION_REVOKED_ABUSIVE, 1);
  histogram_tester_.ExpectBucketCount(
      "PushMessaging.UnregistrationReason",
      blink::mojom::PushUnregistrationReason::PERMISSION_REVOKED, 1);
}

// That test verifies that an origin is not revoked because it is not on
// SafeBrowsing blocking list.
IN_PROC_BROWSER_TEST_F(
    PushMessagingBrowserTestWithAbusiveOriginPermissionRevocation,
    OriginIsNotOnSafeBrowsingBlockingList) {
  // The origin should be marked as |ABUSIVE_CONTENT| on |CrowdDenyPreloadData|
  // otherwise the permission revocation logic will not be triggered.
  AddToPreloadDataBlocklist(
      https_server()->GetURL("/").DeprecatedGetOriginAsURL(),
      SiteReputation::ABUSIVE_CONTENT);

  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());
  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);

  ASSERT_EQ("false - is not controlled", RunScript("isControlled()"));
  LoadTestPage();  // Reload to become controlled.
  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  EXPECT_TRUE(IsRegisteredKeepAliveEqualTo(false));
  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.raw_data = "testdata";
  message.decrypted = true;
  push_service()->OnMessage(app_identifier.app_id(), message);
  EXPECT_TRUE(IsRegisteredKeepAliveEqualTo(true));
  EXPECT_EQ("testdata", RunScript("resultQueue.pop()"));

  // Check that we record this case in UMA.
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.DeliveryStatus",
      static_cast<int>(blink::mojom::PushEventStatus::SUCCESS), 1);
}

class PushMessagingBrowserTestWithNotificationTriggersEnabled
    : public PushMessagingBrowserTestBase {
 public:
  PushMessagingBrowserTestWithNotificationTriggersEnabled() {
    feature_list_.InitAndEnableFeature(features::kNotificationTriggers);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTestWithNotificationTriggersEnabled,
                       PushEventIgnoresScheduledNotificationsForEnforcement) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());
  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);

  LoadTestPage();  // Reload to become controlled.

  RemoveAllNotifications();

  // We'll need to specify the web_contents in which to eval script, since we're
  // going to run script in a background tab.
  content::WebContents* web_contents =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();

  // Initialize site engagement score to have no budget for silent pushes.
  SetSiteEngagementScore(web_contents->GetLastCommittedURL(), 0);

  ui_test_utils::NavigateToURLWithDisposition(
      GetBrowser(), GURL("about:blank"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.raw_data = "shownotification-with-showtrigger";
  message.decrypted = true;

  // If the Service Worker push event handler only schedules a notification, we
  // should show a forced one providing there is no foreground tab and the
  // origin ran out of budget.
  SendMessageAndWaitUntilHandled(app_identifier, message);
  EXPECT_EQ("shownotification-with-showtrigger",
            RunScript("resultQueue.pop()", web_contents));

  // Because scheduled notifications do not count as displayed notifications,
  // this should have shown a default notification.
  std::vector<message_center::Notification> notifications =
      GetDisplayedNotifications();
  ASSERT_EQ(notifications.size(), 1u);

  EXPECT_TRUE(TagEquals(notifications[0], kPushMessagingForcedNotificationTag));
  EXPECT_TRUE(notifications[0].silent());
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       PushEventEnforcesUserVisibleNotificationAfterQueue) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());
  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);

  ASSERT_EQ("false - is not controlled", RunScript("isControlled()"));

  LoadTestPage();  // Reload to become controlled.

  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  // Fire off two push messages in sequence, only the second one of which will
  // display a notification. The additional round-trip and I/O required by the
  // second message, which shows a notification, should give us a reasonable
  // confidence that the ordering will be maintained.

  std::vector<size_t> number_of_notifications_shown;

  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.decrypted = true;

  {
    base::RunLoop run_loop;
    push_service()->SetMessageCallbackForTesting(base::BindRepeating(
        &PushMessagingBrowserTestBase::OnDeliveryFinished,
        base::Unretained(this), &number_of_notifications_shown,
        base::BarrierClosure(2 /* num_closures */, run_loop.QuitClosure())));

    message.raw_data = "testdata";
    push_service()->OnMessage(app_identifier.app_id(), message);

    message.raw_data = "shownotification";
    push_service()->OnMessage(app_identifier.app_id(), message);

    run_loop.Run();
  }

  ASSERT_EQ(2u, number_of_notifications_shown.size());
  EXPECT_EQ(0u, number_of_notifications_shown[0]);
  EXPECT_EQ(1u, number_of_notifications_shown[1]);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       PushEventNotificationWithoutEventWaitUntil) {
  content::WebContents* web_contents =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());
  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);

  ASSERT_EQ("false - is not controlled", RunScript("isControlled()"));

  LoadTestPage();  // Reload to become controlled.

  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  base::RunLoop run_loop;
  base::RepeatingClosure quit_barrier =
      base::BarrierClosure(2 /* num_closures */, run_loop.QuitClosure());
  push_service()->SetMessageCallbackForTesting(quit_barrier);
  notification_tester_->SetNotificationAddedClosure(quit_barrier);

  gcm::IncomingMessage message;
  message.sender_id = GetTestApplicationServerKey();
  message.raw_data = "shownotification-without-waituntil";
  message.decrypted = true;
  EXPECT_TRUE(IsRegisteredKeepAliveEqualTo(false));
  push_service()->OnMessage(app_identifier.app_id(), message);
  EXPECT_TRUE(IsRegisteredKeepAliveEqualTo(true));
  EXPECT_EQ("immediate:shownotification-without-waituntil",
            RunScript("resultQueue.pop()", web_contents));

  run_loop.Run();

  EXPECT_TRUE(IsRegisteredKeepAliveEqualTo(false));
  ASSERT_EQ(1u, GetNotificationCount());
  EXPECT_TRUE(TagEquals(GetDisplayedNotifications()[0], "push_test_tag"));

  // Verify that the renderer process hasn't crashed.
  EXPECT_EQ("permission status - granted",
            RunScript("pushManagerPermissionState()"));
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, PermissionStateSaysPrompt) {
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_EQ("permission status - prompt",
            RunScript("pushManagerPermissionState()"));
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, PermissionStateSaysGranted) {
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_NO_FATAL_FAILURE(RequestAndAcceptPermission());

  ASSERT_NO_FATAL_FAILURE(
      EndpointToToken(RunScript("documentSubscribePush()").ExtractString()));

  EXPECT_EQ("permission status - granted",
            RunScript("pushManagerPermissionState()"));
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, PermissionStateSaysDenied) {
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_NO_FATAL_FAILURE(RequestAndDenyPermission());

  EXPECT_EQ("NotAllowedError - Registration failed - permission denied",
            RunScript("documentSubscribePush()"));

  EXPECT_EQ("permission status - denied",
            RunScript("pushManagerPermissionState()"));
}

IN_PROC_BROWSER_TEST_P(PushMessagingPartitionedBrowserTest, CrossOriginFrame) {
  const GURL kEmbedderURL = https_server()->GetURL(
      "embedder.com", "/push_messaging/framed_test.html");
  const GURL kRequesterURL = https_server()->GetURL("requester.com", "/");
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetCookieSetting(kRequesterURL, CONTENT_SETTING_ALLOW);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowser(), kEmbedderURL));

  auto* web_contents = GetBrowser()->tab_strip_model()->GetActiveWebContents();
  LOG(ERROR) << web_contents->GetLastCommittedURL();
  auto* subframe =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0u);
  ASSERT_TRUE(subframe);

  // A cross-origin subframe that had not been granted the NOTIFICATIONS
  // permission previously should see it as "denied", not be able to request it,
  // and not be able to use the Push and Web Notification API. It is verified
  // that no prompts are shown by auto-accepting and still expecting the
  // permission to be denied.

  GetPermissionRequestManager()->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_EQ("permission status - denied",
            content::EvalJs(subframe, "requestNotificationPermission();"));

  ASSERT_EQ("ok - service worker registered",
            content::EvalJs(subframe, "registerServiceWorker()"));

  EXPECT_EQ("permission status - denied",
            content::EvalJs(subframe, "pushManagerPermissionState()"));

  EXPECT_EQ("permission status - denied",
            content::EvalJs(subframe, "notificationPermissionState()"));

  EXPECT_EQ("permission status - denied",
            content::EvalJs(subframe, "notificationPermissionAPIState()"));

  EXPECT_EQ("NotAllowedError - Registration failed - permission denied",
            content::EvalJs(subframe, "documentSubscribePush()"));

  // A cross-origin subframe that had been granted the NOTIFICATIONS permission
  // previously (in a first-party context) should see it as "granted", and be
  // able to use the Push and Web Notifications APIs.

  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->SetContentSettingDefaultScope(kRequesterURL, kRequesterURL,
                                      ContentSettingsType::NOTIFICATIONS,
                                      CONTENT_SETTING_ALLOW);

  GetPermissionRequestManager()->set_auto_response_for_test(
      permissions::PermissionRequestManager::DENY_ALL);

  EXPECT_EQ("permission status - granted",
            content::EvalJs(subframe, "requestNotificationPermission();"));

  EXPECT_EQ("permission status - granted",
            content::EvalJs(subframe, "pushManagerPermissionState()"));

  EXPECT_EQ("permission status - granted",
            content::EvalJs(subframe, "notificationPermissionState()"));

  EXPECT_EQ("permission status - granted",
            content::EvalJs(subframe, "notificationPermissionAPIState()"));

  ASSERT_NO_FATAL_FAILURE(EndpointToToken(
      content::EvalJs(subframe, "documentSubscribePush()").ExtractString()));
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, UnsubscribeSuccess) {
  std::string token1;
  ASSERT_NO_FATAL_FAILURE(
      SubscribeSuccessfully(PushSubscriptionKeyFormat::kOmitKey, &token1));
  EXPECT_EQ("ok - stored", RunScript("storePushSubscription()"));

  // Resolves true if there was a subscription.
  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(blink::mojom::PushUnregistrationReason::JAVASCRIPT_API),
      1);

  // Resolves false if there was no longer a subscription.
  EXPECT_EQ("unsubscribe result: false",
            RunScript("unsubscribeStoredPushSubscription()"));
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(blink::mojom::PushUnregistrationReason::JAVASCRIPT_API),
      2);

  // TODO(johnme): Test that doesn't reject if there was a network error (should
  // deactivate subscription locally anyway).
  // TODO(johnme): Test that doesn't reject if there were other push service
  // errors (should deactivate subscription locally anyway).

  // Unsubscribing (with an existing reference to a PushSubscription), after
  // replacing the Service Worker, actually still works, as the Service Worker
  // registration is unchanged.
  std::string token2;
  ASSERT_NO_FATAL_FAILURE(
      SubscribeSuccessfully(PushSubscriptionKeyFormat::kOmitKey, &token2));
  EXPECT_NE(token1, token2);
  EXPECT_EQ("ok - stored", RunScript("storePushSubscription()"));
  EXPECT_EQ("ok - service worker replaced",
            RunScript("replaceServiceWorker()"));
  EXPECT_EQ("unsubscribe result: true",
            RunScript("unsubscribeStoredPushSubscription()"));
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(blink::mojom::PushUnregistrationReason::JAVASCRIPT_API),
      3);

  // Unsubscribing (with an existing reference to a PushSubscription), after
  // unregistering the Service Worker, should fail.
  std::string token3;
  ASSERT_NO_FATAL_FAILURE(
      SubscribeSuccessfully(PushSubscriptionKeyFormat::kOmitKey, &token3));
  EXPECT_NE(token1, token3);
  EXPECT_NE(token2, token3);
  EXPECT_EQ("ok - stored", RunScript("storePushSubscription()"));

  // Unregister service worker and wait for callback.
  base::RunLoop run_loop;
  push_service()->SetServiceWorkerUnregisteredCallbackForTesting(
      run_loop.QuitClosure());
  EXPECT_EQ("service worker unregistration status: true",
            RunScript("unregisterServiceWorker()"));
  run_loop.Run();

  // Unregistering should have triggered an automatic unsubscribe.
  histogram_tester_.ExpectBucketCount(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(
          blink::mojom::PushUnregistrationReason::SERVICE_WORKER_UNREGISTERED),
      1);
  histogram_tester_.ExpectTotalCount("PushMessaging.UnregistrationReason", 4);

  // Now manual unsubscribe should return false.
  EXPECT_EQ("unsubscribe result: false",
            RunScript("unsubscribeStoredPushSubscription()"));
}

// Push subscriptions used to be non-InstanceID GCM registrations. Still need
// to be able to unsubscribe these, even though new ones are no longer created.
// Flaky on some Win and Linux buildbots.  See crbug.com/835382.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_LegacyUnsubscribeSuccess DISABLED_LegacyUnsubscribeSuccess
#else
#define MAYBE_LegacyUnsubscribeSuccess LegacyUnsubscribeSuccess
#endif
IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       MAYBE_LegacyUnsubscribeSuccess) {
  std::string subscription_id1;
  ASSERT_NO_FATAL_FAILURE(LegacySubscribeSuccessfully(&subscription_id1));
  EXPECT_EQ("ok - stored", RunScript("storePushSubscription()"));

  // Resolves true if there was a subscription.
  gcm_service_->AddExpectedUnregisterResponse(gcm::GCMClient::SUCCESS);
  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(blink::mojom::PushUnregistrationReason::JAVASCRIPT_API),
      1);

  // Resolves false if there was no longer a subscription.
  EXPECT_EQ("unsubscribe result: false",
            RunScript("unsubscribeStoredPushSubscription()"));
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(blink::mojom::PushUnregistrationReason::JAVASCRIPT_API),
      2);

  // Doesn't reject if there was a network error (deactivates subscription
  // locally anyway).
  std::string subscription_id2;
  ASSERT_NO_FATAL_FAILURE(LegacySubscribeSuccessfully(&subscription_id2));
  EXPECT_NE(subscription_id1, subscription_id2);
  gcm_service_->AddExpectedUnregisterResponse(gcm::GCMClient::NETWORK_ERROR);
  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(blink::mojom::PushUnregistrationReason::JAVASCRIPT_API),
      3);
  EXPECT_EQ("false - not subscribed", RunScript("hasSubscription()"));

  // Doesn't reject if there were other push service errors (deactivates
  // subscription locally anyway).
  std::string subscription_id3;
  ASSERT_NO_FATAL_FAILURE(LegacySubscribeSuccessfully(&subscription_id3));
  EXPECT_NE(subscription_id1, subscription_id3);
  EXPECT_NE(subscription_id2, subscription_id3);
  gcm_service_->AddExpectedUnregisterResponse(
      gcm::GCMClient::INVALID_PARAMETER);
  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(blink::mojom::PushUnregistrationReason::JAVASCRIPT_API),
      4);

  // Unsubscribing (with an existing reference to a PushSubscription), after
  // replacing the Service Worker, actually still works, as the Service Worker
  // registration is unchanged.
  std::string subscription_id4;
  ASSERT_NO_FATAL_FAILURE(LegacySubscribeSuccessfully(&subscription_id4));
  EXPECT_NE(subscription_id1, subscription_id4);
  EXPECT_NE(subscription_id2, subscription_id4);
  EXPECT_NE(subscription_id3, subscription_id4);
  EXPECT_EQ("ok - stored", RunScript("storePushSubscription()"));
  EXPECT_EQ("ok - service worker replaced",
            RunScript("replaceServiceWorker()"));
  EXPECT_EQ("unsubscribe result: true",
            RunScript("unsubscribeStoredPushSubscription()"));
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(blink::mojom::PushUnregistrationReason::JAVASCRIPT_API),
      5);

  // Unsubscribing (with an existing reference to a PushSubscription), after
  // unregistering the Service Worker, should fail.
  std::string subscription_id5;
  ASSERT_NO_FATAL_FAILURE(LegacySubscribeSuccessfully(&subscription_id5));
  EXPECT_NE(subscription_id1, subscription_id5);
  EXPECT_NE(subscription_id2, subscription_id5);
  EXPECT_NE(subscription_id3, subscription_id5);
  EXPECT_NE(subscription_id4, subscription_id5);
  EXPECT_EQ("ok - stored", RunScript("storePushSubscription()"));

  // Unregister service worker and wait for callback.
  base::RunLoop run_loop;
  push_service()->SetServiceWorkerUnregisteredCallbackForTesting(
      run_loop.QuitClosure());
  EXPECT_EQ("service worker unregistration status: true",
            RunScript("unregisterServiceWorker()"));
  run_loop.Run();

  // Unregistering should have triggered an automatic unsubscribe.
  histogram_tester_.ExpectBucketCount(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(
          blink::mojom::PushUnregistrationReason::SERVICE_WORKER_UNREGISTERED),
      1);
  histogram_tester_.ExpectTotalCount("PushMessaging.UnregistrationReason", 6);

  // Now manual unsubscribe should return false.
  EXPECT_EQ("unsubscribe result: false",
            RunScript("unsubscribeStoredPushSubscription()"));
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, UnsubscribeOffline) {
  EXPECT_NE(push_service(), GetAppHandler());

  std::string token;
  ASSERT_NO_FATAL_FAILURE(
      SubscribeSuccessfully(PushSubscriptionKeyFormat::kBinary, &token));

  gcm_service_->set_offline(true);

  // Should quickly resolve true after deleting local state (rather than waiting
  // until unsubscribing over the network exceeds the maximum backoff duration).
  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(blink::mojom::PushUnregistrationReason::JAVASCRIPT_API),
      1);

  // Since the service is offline, the network request to GCM is still being
  // retried, so the app handler shouldn't have been unregistered yet.
  EXPECT_EQ(push_service(), GetAppHandler());
  // But restarting the push service will unregister the app handler, since the
  // subscription is no longer stored in the PushMessagingAppIdentifier map.
  ASSERT_NO_FATAL_FAILURE(RestartPushService());
  EXPECT_NE(push_service(), GetAppHandler());
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       UnregisteringServiceWorkerUnsubscribes) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());

  LoadTestPage();  // Reload to become controlled.
  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  // Unregister the worker, and wait for callback to complete.
  base::RunLoop run_loop;
  push_service()->SetServiceWorkerUnregisteredCallbackForTesting(
      run_loop.QuitClosure());
  ASSERT_EQ("service worker unregistration status: true",
            RunScript("unregisterServiceWorker()"));
  run_loop.Run();

  // This should have unregistered the push subscription.
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(
          blink::mojom::PushUnregistrationReason::SERVICE_WORKER_UNREGISTERED),
      1);

  // We should not be able to look up the app id.
  GURL origin = https_server()->GetURL("/").DeprecatedGetOriginAsURL();
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(
          GetBrowser()->profile(), origin,
          0LL /* service_worker_registration_id */);
  EXPECT_TRUE(app_identifier.is_null());
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       ServiceWorkerDatabaseDeletionUnsubscribes) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());

  LoadTestPage();  // Reload to become controlled.
  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  // Pretend as if the Service Worker database went away, and wait for callback
  // to complete.
  base::RunLoop run_loop;
  push_service()->SetServiceWorkerDatabaseWipedCallbackForTesting(
      run_loop.QuitClosure());
  push_service()->DidDeleteServiceWorkerDatabase();
  run_loop.Run();

  // This should have unregistered the push subscription.
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(blink::mojom::PushUnregistrationReason::
                           SERVICE_WORKER_DATABASE_WIPED),
      1);

  // There should not be any subscriptions left.
  EXPECT_EQ(PushMessagingAppIdentifier::GetCount(GetBrowser()->profile()), 0u);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       InvalidGetSubscriptionUnsubscribes) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());

  GURL origin = https_server()->GetURL("/").DeprecatedGetOriginAsURL();
  PushMessagingAppIdentifier app_identifier1 =
      PushMessagingAppIdentifier::FindByServiceWorker(
          GetBrowser()->profile(), origin,
          0LL /* service_worker_registration_id */);
  ASSERT_FALSE(app_identifier1.is_null());

  ASSERT_NO_FATAL_FAILURE(
      DeleteInstanceIDAsIfGCMStoreReset(app_identifier1.app_id()));

  // Push messaging should not yet be aware of the InstanceID being deleted.
  histogram_tester_.ExpectTotalCount("PushMessaging.UnregistrationReason", 0);
  // We should still be able to look up the app id.
  PushMessagingAppIdentifier app_identifier2 =
      PushMessagingAppIdentifier::FindByServiceWorker(
          GetBrowser()->profile(), origin,
          0LL /* service_worker_registration_id */);
  EXPECT_FALSE(app_identifier2.is_null());
  EXPECT_EQ(app_identifier1.app_id(), app_identifier2.app_id());

  // Now call PushManager.getSubscription(). It should return null.
  EXPECT_EQ("false - not subscribed", RunScript("hasSubscription()"));

  // This should have unsubscribed the push subscription.
  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(blink::mojom::PushUnregistrationReason::
                           GET_SUBSCRIPTION_STORAGE_CORRUPT),
      1);
  // We should no longer be able to look up the app id.
  PushMessagingAppIdentifier app_identifier3 =
      PushMessagingAppIdentifier::FindByServiceWorker(
          GetBrowser()->profile(), origin,
          0LL /* service_worker_registration_id */);
  EXPECT_TRUE(app_identifier3.is_null());
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       GlobalResetPushPermissionUnsubscribes) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());

  EXPECT_EQ("true - subscribed", RunScript("hasSubscription()"));

  EXPECT_EQ("permission status - granted",
            RunScript("pushManagerPermissionState()"));

  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  push_service()->SetContentSettingChangedCallbackForTesting(
      base::BarrierClosure(1, message_loop_runner->QuitClosure()));

  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::NOTIFICATIONS);

  message_loop_runner->Run();

  EXPECT_EQ("permission status - prompt",
            RunScript("pushManagerPermissionState()"));

  EXPECT_EQ("false - not subscribed", RunScript("hasSubscription()"));

  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(
          blink::mojom::PushUnregistrationReason::PERMISSION_REVOKED),
      1);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       LocalResetPushPermissionUnsubscribes) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());

  EXPECT_EQ("true - subscribed", RunScript("hasSubscription()"));

  EXPECT_EQ("permission status - granted",
            RunScript("pushManagerPermissionState()"));

  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  push_service()->SetContentSettingChangedCallbackForTesting(
      base::BarrierClosure(1, message_loop_runner->QuitClosure()));

  GURL origin = https_server()->GetURL("/").DeprecatedGetOriginAsURL();
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->SetContentSettingDefaultScope(origin, origin,
                                      ContentSettingsType::NOTIFICATIONS,
                                      CONTENT_SETTING_DEFAULT);

  message_loop_runner->Run();

  EXPECT_EQ("permission status - prompt",
            RunScript("pushManagerPermissionState()"));

  EXPECT_EQ("false - not subscribed", RunScript("hasSubscription()"));

  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(
          blink::mojom::PushUnregistrationReason::PERMISSION_REVOKED),
      1);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       DenyPushPermissionUnsubscribes) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());

  EXPECT_EQ("true - subscribed", RunScript("hasSubscription()"));

  EXPECT_EQ("permission status - granted",
            RunScript("pushManagerPermissionState()"));

  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  push_service()->SetContentSettingChangedCallbackForTesting(
      base::BarrierClosure(1, message_loop_runner->QuitClosure()));

  GURL origin = https_server()->GetURL("/").DeprecatedGetOriginAsURL();
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->SetContentSettingDefaultScope(origin, origin,
                                      ContentSettingsType::NOTIFICATIONS,
                                      CONTENT_SETTING_BLOCK);

  message_loop_runner->Run();

  EXPECT_EQ("permission status - denied",
            RunScript("pushManagerPermissionState()"));

  EXPECT_EQ("false - not subscribed", RunScript("hasSubscription()"));

  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(
          blink::mojom::PushUnregistrationReason::PERMISSION_REVOKED),
      1);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       GlobalResetNotificationsPermissionUnsubscribes) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());

  EXPECT_EQ("true - subscribed", RunScript("hasSubscription()"));

  EXPECT_EQ("permission status - granted",
            RunScript("pushManagerPermissionState()"));

  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  push_service()->SetContentSettingChangedCallbackForTesting(
      base::BarrierClosure(1, message_loop_runner->QuitClosure()));

  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::NOTIFICATIONS);

  message_loop_runner->Run();

  EXPECT_EQ("permission status - prompt",
            RunScript("pushManagerPermissionState()"));

  EXPECT_EQ("false - not subscribed", RunScript("hasSubscription()"));

  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(
          blink::mojom::PushUnregistrationReason::PERMISSION_REVOKED),
      1);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       LocalResetNotificationsPermissionUnsubscribes) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());

  EXPECT_EQ("true - subscribed", RunScript("hasSubscription()"));

  EXPECT_EQ("permission status - granted",
            RunScript("pushManagerPermissionState()"));

  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  push_service()->SetContentSettingChangedCallbackForTesting(
      base::BarrierClosure(1, message_loop_runner->QuitClosure()));

  GURL origin = https_server()->GetURL("/").DeprecatedGetOriginAsURL();
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->SetContentSettingDefaultScope(origin, GURL(),
                                      ContentSettingsType::NOTIFICATIONS,
                                      CONTENT_SETTING_DEFAULT);

  message_loop_runner->Run();

  EXPECT_EQ("permission status - prompt",
            RunScript("pushManagerPermissionState()"));

  EXPECT_EQ("false - not subscribed", RunScript("hasSubscription()"));

  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(
          blink::mojom::PushUnregistrationReason::PERMISSION_REVOKED),
      1);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       DenyNotificationsPermissionUnsubscribes) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());

  EXPECT_EQ("true - subscribed", RunScript("hasSubscription()"));

  EXPECT_EQ("permission status - granted",
            RunScript("pushManagerPermissionState()"));

  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  push_service()->SetContentSettingChangedCallbackForTesting(
      base::BarrierClosure(1, message_loop_runner->QuitClosure()));

  GURL origin = https_server()->GetURL("/").DeprecatedGetOriginAsURL();
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->SetContentSettingDefaultScope(origin, GURL(),
                                      ContentSettingsType::NOTIFICATIONS,
                                      CONTENT_SETTING_BLOCK);

  message_loop_runner->Run();

  EXPECT_EQ("permission status - denied",
            RunScript("pushManagerPermissionState()"));

  EXPECT_EQ("false - not subscribed", RunScript("hasSubscription()"));

  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(
          blink::mojom::PushUnregistrationReason::PERMISSION_REVOKED),
      1);
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       GrantAlreadyGrantedPermissionDoesNotUnsubscribe) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());

  EXPECT_EQ("true - subscribed", RunScript("hasSubscription()"));

  EXPECT_EQ("permission status - granted",
            RunScript("pushManagerPermissionState()"));

  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  push_service()->SetContentSettingChangedCallbackForTesting(
      base::BarrierClosure(1, message_loop_runner->QuitClosure()));

  GURL origin = https_server()->GetURL("/").DeprecatedGetOriginAsURL();
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->SetContentSettingDefaultScope(origin, GURL(),
                                      ContentSettingsType::NOTIFICATIONS,
                                      CONTENT_SETTING_ALLOW);

  message_loop_runner->Run();

  EXPECT_EQ("permission status - granted",
            RunScript("pushManagerPermissionState()"));

  EXPECT_EQ("true - subscribed", RunScript("hasSubscription()"));

  histogram_tester_.ExpectTotalCount("PushMessaging.UnregistrationReason", 0);
}

// This test is testing some non-trivial content settings rules and make sure
// that they are respected with regards to automatic unsubscription. In other
// words, it checks that the push service does not end up unsubscribing origins
// that have push permission with some non-common rules.
IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest,
                       AutomaticUnsubscriptionFollowsContentSettingRules) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());

  EXPECT_EQ("true - subscribed", RunScript("hasSubscription()"));

  EXPECT_EQ("permission status - granted",
            RunScript("pushManagerPermissionState()"));

  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  push_service()->SetContentSettingChangedCallbackForTesting(
      base::BarrierClosure(2, message_loop_runner->QuitClosure()));

  GURL origin = https_server()->GetURL("/").DeprecatedGetOriginAsURL();
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS,
                                 CONTENT_SETTING_ALLOW);
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->SetContentSettingDefaultScope(origin, GURL(),
                                      ContentSettingsType::NOTIFICATIONS,
                                      CONTENT_SETTING_DEFAULT);

  message_loop_runner->Run();

  // The two first rules should give |origin| the permission to use Push even
  // if the rules it used to have have been reset.
  // The Push service should not unsubscribe |origin| because at no point it was
  // left without permission to use Push.

  EXPECT_EQ("permission status - granted",
            RunScript("pushManagerPermissionState()"));

  EXPECT_EQ("true - subscribed", RunScript("hasSubscription()"));

  histogram_tester_.ExpectTotalCount("PushMessaging.UnregistrationReason", 0);
}

// Checks automatically unsubscribing due to a revoked permission after
// previously clearing site data, under legacy conditions (ie. when
// unregistering a worker did not unsubscribe from push.)
IN_PROC_BROWSER_TEST_F(
    PushMessagingBrowserTest,
    ResetPushPermissionAfterClearingSiteDataUnderLegacyConditions) {
  std::string app_id;
  ASSERT_NO_FATAL_FAILURE(SetupOrphanedPushSubscription(&app_id));

  // Simulate a user clearing site data (including Service Workers, crucially).
  content::BrowsingDataRemover* remover =
      GetBrowser()->profile()->GetBrowsingDataRemover();
  content::BrowsingDataRemoverCompletionObserver observer(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(),
      chrome_browsing_data_remover::DATA_TYPE_SITE_DATA,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, &observer);
  observer.BlockUntilCompletion();

  base::RunLoop run_loop;
  push_service()->SetContentSettingChangedCallbackForTesting(
      run_loop.QuitClosure());
  // This shouldn't (asynchronously) cause a DCHECK.
  // TODO(johnme): Get this test running on Android with legacy GCM
  // registrations, which have a different codepath due to sender_id being
  // required for unsubscribing there.
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::NOTIFICATIONS);

  run_loop.Run();

  // |app_identifier| should no longer be stored in prefs.
  PushMessagingAppIdentifier stored_app_identifier =
      PushMessagingAppIdentifier::FindByAppId(GetBrowser()->profile(), app_id);
  EXPECT_TRUE(stored_app_identifier.is_null());

  histogram_tester_.ExpectUniqueSample(
      "PushMessaging.UnregistrationReason",
      static_cast<int>(
          blink::mojom::PushUnregistrationReason::PERMISSION_REVOKED),
      1);

  base::RunLoop().RunUntilIdle();

  // Revoked permission should trigger an automatic unsubscription attempt.
  EXPECT_EQ(app_id, gcm_driver_->last_deletetoken_app_id());
}

IN_PROC_BROWSER_TEST_F(PushMessagingBrowserTest, EncryptionKeyUniqueness) {
  std::string token1;
  ASSERT_NO_FATAL_FAILURE(
      SubscribeSuccessfully(PushSubscriptionKeyFormat::kOmitKey, &token1));

  std::string first_public_key = RunScript("GetP256dh()").ExtractString();
  EXPECT_GE(first_public_key.size(), 32u);

  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));

  std::string token2;
  ASSERT_NO_FATAL_FAILURE(
      SubscribeSuccessfully(PushSubscriptionKeyFormat::kBinary, &token2));
  EXPECT_NE(token1, token2);

  std::string second_public_key = RunScript("GetP256dh()").ExtractString();
  EXPECT_GE(second_public_key.size(), 32u);

  EXPECT_NE(first_public_key, second_public_key);
}

class PushMessagingIncognitoBrowserTest : public PushMessagingBrowserTestBase {
 public:
  PushMessagingIncognitoBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &PushMessagingIncognitoBrowserTest::web_contents,
            base::Unretained(this))) {}
  ~PushMessagingIncognitoBrowserTest() override = default;

  // PushMessagingBrowserTest:
  void SetUpOnMainThread() override {
    incognito_browser_ = CreateIncognitoBrowser();
    // We SetUp here rather than in SetUp since the https_server isn't yet
    // created at that time.
    prerender_helper_.RegisterServerRequestMonitor(https_server());
    PushMessagingBrowserTestBase::SetUpOnMainThread();
  }
  Browser* GetBrowser() const override { return incognito_browser_; }

  content::WebContents* web_contents() {
    return GetBrowser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> incognito_browser_ = nullptr;
};

// Regression test for https://crbug.com/476474
IN_PROC_BROWSER_TEST_F(PushMessagingIncognitoBrowserTest,
                       IncognitoGetSubscriptionDoesNotHang) {
  ASSERT_TRUE(GetBrowser()->profile()->IsOffTheRecord());

  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  // In Incognito mode the promise returned by getSubscription should not hang,
  // it should just fulfill with null.
  ASSERT_EQ("false - not subscribed", RunScript("hasSubscription()"));
}

IN_PROC_BROWSER_TEST_F(PushMessagingIncognitoBrowserTest, WarningToCorrectRFH) {
  ASSERT_TRUE(GetBrowser()->profile()->IsOffTheRecord());

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kIncognitoWarningPattern);

  // Filter out the main frame host of the currently active page.
  console_observer.SetFilter(base::BindLambdaForTesting(
      [&](const content::WebContentsConsoleObserver::Message& message) {
        return message.source_frame->IsInPrimaryMainFrame();
      }));

  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_EQ("AbortError - Registration failed - permission denied",
            RunScript("documentSubscribePush()"));

  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());
}

// TODO(crbug.com/40204670): This test hits the issue. Re-enable after it
// is fixed.
IN_PROC_BROWSER_TEST_F(PushMessagingIncognitoBrowserTest,
                       DISABLED_WarningToCorrectRFH_Prerender) {
  ASSERT_TRUE(GetBrowser()->profile()->IsOffTheRecord());

  // Load an initial page.
  const GURL initial_url(https_server()->GetURL(GetTestURL()));
  prerender_helper_.NavigatePrimaryPage(initial_url);

  // Register a service worker. This must be done in the primary page as the
  // service worker registration in a prerendered page is deferred until
  // prerender page activation.
  ASSERT_EQ("ok - service worker registered",
            content::EvalJs(web_contents()->GetPrimaryMainFrame(),
                            "registerServiceWorker()"));

  // Start a prerender with the push messaging test URL.
  const GURL prerendering_url(
      https_server()->GetURL(GetTestURL() + "?prerendering"));
  content::FrameTreeNodeId host_id =
      prerender_helper_.AddPrerender(prerendering_url);
  content::test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                          host_id);
  ASSERT_TRUE(prerender_helper_.GetHostForUrl(prerendering_url));

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(kIncognitoWarningPattern);

  // Filter out the main frame host of the prerendered page.
  content::RenderFrameHost* prerender_rfh =
      prerender_helper_.GetPrerenderedMainFrameHost(host_id);
  console_observer.SetFilter(base::BindLambdaForTesting(
      [&](const content::WebContentsConsoleObserver::Message& message) {
        return message.source_frame == prerender_rfh;
      }));

  // Use ExecuteScriptAsync because binding of blink::mojom::PushMessaging
  // is deferred for the prerendered page. Script execution will finish after
  // the activation.
  ExecuteScriptAsync(prerender_rfh, "documentSubscribePush()");

  // Activate the prerendered page and wait for a response of script execution.
  content::DOMMessageQueue message_queue(web_contents());
  prerender_helper_.NavigatePrimaryPage(prerendering_url);
  // Make sure that the prerender was activated.
  ASSERT_TRUE(prerender_observer.was_activated());
  std::string script_result;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  } while (script_result !=
           "\"AbortError - Registration failed - permission denied\"");

  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());
}

class PushMessagingDisallowSenderIdsBrowserTest
    : public PushMessagingBrowserTestBase {
 public:
  PushMessagingDisallowSenderIdsBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kPushMessagingDisallowSenderIDs);
  }

  ~PushMessagingDisallowSenderIdsBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PushMessagingDisallowSenderIdsBrowserTest,
                       SubscriptionWithSenderIdFails) {
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_NO_FATAL_FAILURE(RequestAndAcceptPermission());

  LoadTestPage();  // Reload to become controlled.

  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  // Attempt to create a subscription with a GCM Sender ID ("numeric key"),
  // which should fail because the kPushMessagingDisallowSenderIDs feature has
  // been enabled for this test.
  EXPECT_EQ(
      "AbortError - Registration failed - GCM Sender IDs are no longer "
      "supported, please upgrade to VAPID authentication instead",
      RunScript("documentSubscribePushWithNumericKey()"));
}

class PushSubscriptionWithExpirationTimeTest
    : public PushMessagingBrowserTestBase {
 public:
  PushSubscriptionWithExpirationTimeTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kPushSubscriptionWithExpirationTime);
  }

  ~PushSubscriptionWithExpirationTimeTest() override = default;

  // Checks whether |expiration_time| lies in the future and is in the
  // valid format (seconds elapsed since Unix time)
  bool IsExpirationTimeValid(const std::string& expiration_time);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

bool PushSubscriptionWithExpirationTimeTest::IsExpirationTimeValid(
    const std::string& expiration_time) {
  int64_t output;
  if (!base::StringToInt64(expiration_time, &output))
    return false;
  return base::Time::Now().InMillisecondsFSinceUnixEpochIgnoringNull() < output;
}

IN_PROC_BROWSER_TEST_F(PushSubscriptionWithExpirationTimeTest,
                       SubscribeGetSubscriptionWithExpirationTime) {
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_NO_FATAL_FAILURE(RequestAndAcceptPermission());

  LoadTestPage();  // Reload to become controlled.

  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  // Subscribe with expiration time enabled, should get a subscription with
  // expiration time in the future back
  std::string subscription_expiration_time =
      RunScript("documentSubscribePushGetExpirationTime()").ExtractString();
  EXPECT_TRUE(IsExpirationTimeValid(subscription_expiration_time));

  // Get subscription should also yield a subscription with expiration time
  std::string get_subscription_expiration_time =
      RunScript("GetSubscriptionExpirationTime()").ExtractString();
  EXPECT_TRUE(IsExpirationTimeValid(get_subscription_expiration_time));
  // Both methods should return the same expiration time
  ASSERT_EQ(subscription_expiration_time, get_subscription_expiration_time);
}

IN_PROC_BROWSER_TEST_F(PushSubscriptionWithExpirationTimeTest,
                       GetSubscriptionWithExpirationTime) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());

  EXPECT_EQ("true - subscribed", RunScript("hasSubscription()"));

  // Get subscription should also yield a subscription with expiration time
  EXPECT_TRUE(IsExpirationTimeValid(
      RunScript("GetSubscriptionExpirationTime()").ExtractString()));
}

class PushSubscriptionWithoutExpirationTimeTest
    : public PushMessagingBrowserTestBase {
 public:
  PushSubscriptionWithoutExpirationTimeTest() {
    // Override current feature list to ensure having
    // |kPushSubscriptionWithExpirationTime| disabled
    scoped_feature_list_.InitAndDisableFeature(
        features::kPushSubscriptionWithExpirationTime);
  }

  ~PushSubscriptionWithoutExpirationTimeTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PushSubscriptionWithoutExpirationTimeTest,
                       SubscribeDocumentExpirationTimeNull) {
  ASSERT_EQ("ok - service worker registered",
            RunScript("registerServiceWorker()"));

  ASSERT_NO_FATAL_FAILURE(RequestAndAcceptPermission());

  LoadTestPage();  // Reload to become controlled.

  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  // When |features::kPushSubscriptionWithExpirationTime| is disabled,
  // expiration time should be null
  EXPECT_EQ("null", RunScript("documentSubscribePushGetExpirationTime()"));
}

class PushSubscriptionChangeEventTest : public PushMessagingBrowserTestBase {
 public:
  PushSubscriptionChangeEventTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kPushSubscriptionChangeEvent,
         features::kPushSubscriptionWithExpirationTime},
        {});
  }

  ~PushSubscriptionChangeEventTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PushSubscriptionChangeEventTest,
                       PushSubscriptionChangeEventSuccess) {
  // Create the |old_subscription| by subscribing and unsubscribing again
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());
  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);

  blink::mojom::PushSubscriptionPtr old_subscription =
      GetSubscriptionForAppIdentifier(app_identifier);

  EXPECT_EQ("unsubscribe result: true", RunScript("unsubscribePush()"));

  // There should be no subscription since we unsubscribed
  EXPECT_EQ(PushMessagingAppIdentifier::GetCount(GetBrowser()->profile()), 0u);

  // Create a |new_subscription| by resubscribing
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());
  app_identifier = GetAppIdentifierForServiceWorkerRegistration(0LL);

  blink::mojom::PushSubscriptionPtr new_subscription =
      GetSubscriptionForAppIdentifier(app_identifier);

  // Save the endpoints to compare with the JS result
  GURL old_endpoint = old_subscription->endpoint;
  GURL new_endpoint = new_subscription->endpoint;

  ASSERT_EQ("false - is not controlled", RunScript("isControlled()"));
  LoadTestPage();  // Reload to become controlled.
  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  base::RunLoop run_loop;
  push_service()->FirePushSubscriptionChange(
      app_identifier, run_loop.QuitClosure(), std::move(new_subscription),
      std::move(old_subscription));
  run_loop.Run();

  // Compare old subscription
  EXPECT_EQ(old_endpoint.spec(), RunScript("resultQueue.pop()"));
  // Compare new subscription
  EXPECT_EQ(new_endpoint.spec(), RunScript("resultQueue.pop()"));
}

IN_PROC_BROWSER_TEST_F(PushSubscriptionChangeEventTest,
                       FiredAfterPermissionRevoked) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());

  EXPECT_EQ("true - subscribed", RunScript("hasSubscription()"));

  EXPECT_EQ("permission status - granted",
            RunScript("pushManagerPermissionState()"));

  ASSERT_EQ("false - is not controlled", RunScript("isControlled()"));
  LoadTestPage();  // Reload to become controlled.
  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);
  auto old_subscription = GetSubscriptionForAppIdentifier(app_identifier);

  base::RunLoop run_loop;
  push_service()->SetContentSettingChangedCallbackForTesting(
      run_loop.QuitClosure());
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->SetContentSettingDefaultScope(app_identifier.origin(), GURL(),
                                      ContentSettingsType::NOTIFICATIONS,
                                      CONTENT_SETTING_BLOCK);
  run_loop.Run();

  EXPECT_EQ("permission status - denied",
            RunScript("pushManagerPermissionState()"));

  // Check if the pushsubscriptionchangeevent arrived in the document and
  // whether the |old_subscription| has the expected endpoint and
  // |new_subscription| is null
  EXPECT_EQ(old_subscription->endpoint.spec(), RunScript("resultQueue.pop()"));
  EXPECT_EQ("null", RunScript("resultQueue.pop()"));
}

IN_PROC_BROWSER_TEST_F(PushSubscriptionChangeEventTest, OnInvalidation) {
  ASSERT_NO_FATAL_FAILURE(SubscribeSuccessfully());

  EXPECT_EQ("true - subscribed", RunScript("hasSubscription()"));

  ASSERT_EQ("false - is not controlled", RunScript("isControlled()"));
  LoadTestPage();  // Reload to become controlled.
  ASSERT_EQ("true - is controlled", RunScript("isControlled()"));

  PushMessagingAppIdentifier app_identifier =
      GetAppIdentifierForServiceWorkerRegistration(0LL);
  ASSERT_FALSE(app_identifier.is_null());

  base::RunLoop run_loop;
  push_service()->SetInvalidationCallbackForTesting(run_loop.QuitClosure());
  push_service()->OnSubscriptionInvalidation(app_identifier.app_id());
  run_loop.Run();

  // Old subscription should be gone
  PushMessagingAppIdentifier deleted_identifier =
      PushMessagingAppIdentifier::FindByAppId(GetBrowser()->profile(),
                                              app_identifier.app_id());
  EXPECT_TRUE(deleted_identifier.is_null());

  // New subscription with a different app id should exist
  PushMessagingAppIdentifier new_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(
          GetBrowser()->profile(), app_identifier.origin(),
          app_identifier.service_worker_registration_id());
  EXPECT_FALSE(new_identifier.is_null());

  base::RunLoop().RunUntilIdle();

  // Expect `pushsubscriptionchange` event that is not null
  EXPECT_NE("null", RunScript("resultQueue.pop()"));
  EXPECT_NE("null", RunScript("resultQueue.pop()"));
}

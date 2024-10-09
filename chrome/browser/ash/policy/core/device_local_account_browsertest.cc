// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/policy/core/device_local_account.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_paths.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/display/display_prefs.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/shell.h"
#include "ash/system/session/logout_confirmation_controller.h"
#include "ash/system/session/logout_confirmation_dialog.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/test/gtest_tags.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/extensions/external_cache.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/session/chrome_session_manager.h"
#include "chrome/browser/ash/login/session/session_length_limiter.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/signin_specifics.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_or_lock_screen_visible_waiter.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/profile_prepared_waiter.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/test/webview_content_extractor.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_test_util.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_broker.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/external_data/cloud_external_data_manager_base_test_util.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/extensions/external_loader/device_local_account_external_policy_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/updater/chromeos_extension_cache_delegate.h"
#include "chrome/browser/extensions/updater/extension_cache_impl.h"
#include "chrome/browser/extensions/updater/local_extension_cache.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_test_utils.h"
#include "chrome/browser/policy/networking/device_network_configuration_updater_ash.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/login/terms_of_service_screen_handler.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/network/policy_certificate_provider.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/crx_file/crx_verifier.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/ukm/ukm_test_helper.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/browser/updater/extension_downloader_test_helper.h"
#include "extensions/common/extension.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/metrics_proto/ukm/entry.pb.h"
#include "third_party/metrics_proto/ukm/report.pb.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/input_method_descriptor.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

using base::test::TestFuture;
using extensions::CrxInstallError;

namespace policy {

namespace {

namespace em = ::enterprise_management;

using ::ash::test::GetOobeElementPath;

const char16_t kDomain[] = u"example.com";
const char kAccountId1[] = "dla1@example.com";
const char kAccountId2[] = "dla2@example.com";
const char kDisplayName1[] = "display name 1";
const char kDisplayName2[] = "display name 2";
const char* const kStartupURLs[] = {
    "chrome://policy",
    "chrome://about",
};
const char kExistentTermsOfServicePath[] = "chromeos/enterprise/tos.txt";
const char kNonexistentTermsOfServicePath[] = "chromeos/enterprise/tos404.txt";
const char kRelativeUpdateURL[] = "/service/update2/crx";
const char kHostedAppID[] = "kbmnembihfiondgfjekmnmcbddelicoi";
const char kHostedAppCRXPath[] = "extensions/hosted_app.crx";
const char kHostedAppVersion[] = "1.0.0.0";
const char kGoodExtensionID[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char kGoodExtensionCRXPath[] = "extensions/good.crx";
const char kGoodExtensionVersion[] = "1.0";
const char kPackagedAppCRXPath[] = "extensions/platform_apps/app_window_2.crx";
const char kShowManagedStorageID[] = "ongnjlefhnoajpbodoldndkbkdgfomlp";
const char kShowManagedStorageCRXPath[] = "extensions/show_managed_storage.crx";
const char kShowManagedStorageVersion[] = "1.0";

const char kExternalData[] = "External data";
const char kExternalDataPath[] = "/external";

const char* const kSingleRecommendedLocale[] = {
    "el",
};
const char* const kRecommendedLocales1[] = {
    "pl",
    "et",
    "en-US",
};
const char* const kRecommendedLocales2[] = {
    "fr",
    "nl",
};
const char* const kInvalidRecommendedLocale[] = {
    "xx",
};
const char kPublicSessionLocale[] = "de";
const char kPublicSessionInputMethodIDTemplate[] = "_comp_ime_%sxkb:de:neo:ger";

const char kFakeOncWithCertificate[] =
    "{\"Certificates\":["
    "{\"Type\":\"Authority\","
    "\"TrustBits\":[\"Web\"],"
    "\"X509\":\"-----BEGIN CERTIFICATE-----\n"
    "MIICVTCCAb6gAwIBAgIJAK8kOY/OQDsKMA0GCSqGSIb3DQEBCwUAMEIxCzAJBgNV\n"
    "BAYTAkRFMRAwDgYDVQQIDAdCYXZhcmlhMSEwHwYDVQQKDBhJbnRlcm5ldCBXaWRn\n"
    "aXRzIFB0eSBMdGQwHhcNMTgxMjI3MTIyNjI0WhcNMTkxMjI3MTIyNjI0WjBCMQsw\n"
    "CQYDVQQGEwJERTEQMA4GA1UECAwHQmF2YXJpYTEhMB8GA1UECgwYSW50ZXJuZXQg\n"
    "V2lkZ2l0cyBQdHkgTHRkMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDbFncT\n"
    "Q8slhRgLg7sK9DhkYZaNiD1jVbdGvuXahex3uQl+2bACyQ7Peq/MkpFLy4M75nj3\n"
    "WrydAycw1KCDPENPz2jmdHwGl5+6P7bob0Rqe+4i/9XwGdl8EPH5GFZbaz8aSYiL\n"
    "/aaVvOm+8IYrhbp44s3cOLriPaQDbWtZMZKCiwIDAQABo1MwUTAdBgNVHQ4EFgQU\n"
    "26bvyiqj3uQynNcZru72m3Uv3eswHwYDVR0jBBgwFoAU26bvyiqj3uQynNcZru72\n"
    "m3Uv3eswDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOBgQCHKz8NJg6f\n"
    "qwkFmG+tOsfyn3JHj3NfkMGJugSV6Yf7LYJXHpc4kWmfGuseTtHt57PG/BzCjLs1\n"
    "qTF8svVecDj5Qku/SbGQCf2Vg/tLnq8XidbMmp26nUXrLzNQnTm0MJYEk6PJRiod\n"
    "BIrpuq5z+9r//9f27iXidR94qFbbvServw==\n"
    "-----END CERTIFICATE-----\","
    "\"GUID\":\"{00f79111-51e0-e6e0-76b3b55450d80a1b}\"}"
    "]}";

bool IsLogoutConfirmationDialogShowing() {
  return !!ash::Shell::Get()
               ->logout_confirmation_controller()
               ->dialog_for_testing();
}

void CloseLogoutConfirmationDialog() {
  // TODO(mash): Add mojo test API for this.
  ash::LogoutConfirmationDialog* dialog =
      ash::Shell::Get()->logout_confirmation_controller()->dialog_for_testing();
  ASSERT_TRUE(dialog);
  dialog->GetWidget()->Close();
  base::RunLoop().RunUntilIdle();
}

// Helper that serves extension update manifests to Chrome.
class TestingUpdateManifestProvider
    : public base::RefCountedThreadSafe<TestingUpdateManifestProvider> {
 public:
  // Update manifests will be served at |relative_update_url|.
  explicit TestingUpdateManifestProvider(
      const std::string& relative_update_url);

  // When an update manifest is requested for the given extension |id|, indicate
  // that |version| of the extension can be downloaded at |crx_url|.
  void AddUpdate(const std::string& id,
                 const std::string& version,
                 const GURL& crx_url);

  // This method must be registered with the test's EmbeddedTestServer to start
  // serving update manifests.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

 private:
  struct Update {
   public:
    Update(const std::string& version, const GURL& crx_url);
    Update();

    std::string version;
    GURL crx_url;
  };
  typedef std::map<std::string, Update> UpdateMap;

  TestingUpdateManifestProvider(const TestingUpdateManifestProvider&) = delete;
  TestingUpdateManifestProvider& operator=(
      const TestingUpdateManifestProvider&) = delete;

  ~TestingUpdateManifestProvider();
  friend class RefCountedThreadSafe<TestingUpdateManifestProvider>;

  // Protects other members against concurrent access from main thread and
  // test server io thread.
  base::Lock lock_;

  std::string relative_update_url_;
  UpdateMap updates_;
};

TestingUpdateManifestProvider::Update::Update(const std::string& version,
                                              const GURL& crx_url)
    : version(version), crx_url(crx_url) {}

TestingUpdateManifestProvider::Update::Update() {}

TestingUpdateManifestProvider::TestingUpdateManifestProvider(
    const std::string& relative_update_url)
    : relative_update_url_(relative_update_url) {}

void TestingUpdateManifestProvider::AddUpdate(const std::string& id,
                                              const std::string& version,
                                              const GURL& crx_url) {
  base::AutoLock auto_lock(lock_);
  updates_[id] = Update(version, crx_url);
}

std::unique_ptr<net::test_server::HttpResponse>
TestingUpdateManifestProvider::HandleRequest(
    const net::test_server::HttpRequest& request) {
  base::AutoLock auto_lock(lock_);
  const GURL url("http://localhost" + request.relative_url);
  if (url.path() != relative_update_url_) {
    return nullptr;
  }

  std::vector<extensions::UpdateManifestItem> update_manifest;
  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() != "x") {
      continue;
    }
    // Extract the extension id from the subquery. Since GetValueForKeyInQuery()
    // expects a complete URL, dummy scheme and host must be prepended.
    std::string id;
    net::GetValueForKeyInQuery(GURL("http://dummy?" + it.GetUnescapedValue()),
                               "id", &id);
    UpdateMap::const_iterator entry = updates_.find(id);
    if (entry != updates_.end()) {
      update_manifest.emplace_back(extensions::UpdateManifestItem(id)
                                       .version(entry->second.version)
                                       .codebase(entry->second.crx_url.spec()));
    }
  }

  std::string content =
      extensions::CreateUpdateManifest(std::move(update_manifest));

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(content);
  http_response->set_content_type("text/xml");
  return std::move(http_response);
}

TestingUpdateManifestProvider::~TestingUpdateManifestProvider() {}

bool IsSessionStarted() {
  return session_manager::SessionManager::Get()->IsSessionStarted();
}

const base::Value* RefreshAndWaitForPolicies(
    policy::PolicyService* policy_service,
    const policy::PolicyNamespace& ns) {
  PolicyChangeRegistrar policy_registrar(policy_service, ns);
  TestFuture<const base::Value*, const base::Value*> future;
  policy_registrar.Observe("string", future.GetRepeatingCallback());
  policy_service->RefreshPolicies(base::OnceClosure(),
                                  PolicyFetchReason::kTest);
  return std::get<1>(future.Take());
}

DeviceLocalAccountPolicyBroker* GetDeviceLocalAccountPolicyBroker(
    AccountId account) {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetDeviceLocalAccountPolicyService()
      ->GetBrokerForUser(account.GetUserEmail());
}

bool IsFullManagementDisclosureNeeded(AccountId account) {
  auto* broker = GetDeviceLocalAccountPolicyBroker(account);
  return ash::login::IsFullManagementDisclosureNeeded(broker);
}

ukm::UkmService* GetUkmService() {
  return g_browser_process->GetMetricsServicesManager()->GetUkmService();
}

void EnableUrlKeyedAnonymizedDataCollection(Profile* profile) {
  unified_consent::UnifiedConsentService* consent_service =
      UnifiedConsentServiceFactory::GetForProfile(profile);
  if (consent_service) {
    consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
    g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(
        true);
  }
}

}  // namespace

class DeviceLocalAccountTest : public DevicePolicyCrosBrowserTest,
                               public user_manager::UserManager::Observer,
                               public BrowserListObserver,
                               public extensions::AppWindowRegistry::Observer {
 public:
  DeviceLocalAccountTest(const DeviceLocalAccountTest&) = delete;
  DeviceLocalAccountTest& operator=(const DeviceLocalAccountTest&) = delete;

 protected:
  static constexpr char kDisplayNameTag[] =
      "screenplay-6fef6eb9-1132-4d67-9ff7-f7d68b34fc3c";
  static constexpr char kExtensionsCachedTag[] =
      "screenplay-ac6c2f45-b38f-46b2-b107-36546701bcb2";
  static constexpr char kExtensionsUncachedTag[] =
      "screenplay-0834405c-3800-4c41-b5d5-cc57c9bfd472";
  static constexpr char kUserAvatarImageTag[] =
      "screenplay-91d50c4f-f526-4fad-a04d-5c9e1a90fb2b";
  static constexpr char kSessionLengthLimitTag[] =
      "screenplay-a91d99d7-8ea0-4ec7-9c64-bc614a759d02";
  static constexpr char kDisplayPrefsTag[] =
      "screenplay-5476f7ac-a3c2-47ad-865f-62ff31374865";

  DeviceLocalAccountTest()
      : public_session_input_method_id_(
            base::StringPrintf(kPublicSessionInputMethodIDTemplate,
                               ash::extension_ime_util::kXkbExtensionId)) {
    set_exit_when_last_browser_closes(false);
  }

  ~DeviceLocalAccountTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kLoginManager);
    command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile, "user");
  }

  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    // Clear command-line arguments (but keep command-line switches) so the
    // startup pages policy takes effect.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    base::CommandLine::StringVector argv(command_line->argv());
    argv.erase(argv.begin() + argv.size() - command_line->GetArgs().size(),
               argv.end());
    command_line->InitFromArgv(argv);

    InitializePolicy();
  }

  void SetUpOnMainThread() override {
    DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    BrowserList::AddObserver(this);

    initial_locale_ = g_browser_process->GetApplicationLocale();
    initial_language_ = l10n_util::GetLanguage(initial_locale_);

    ash::LoginOrLockScreenVisibleWaiter().Wait();

    auto* host = ash::LoginDisplayHost::default_host();
    ASSERT_TRUE(host->GetOobeWebContents());

    // Wait for the login UI to be ready.
    ash::OobeUI* oobe_ui = host->GetOobeUI();
    ASSERT_TRUE(oobe_ui);
    base::RunLoop run_loop;
    const bool oobe_ui_ready = oobe_ui->IsJSReady(run_loop.QuitClosure());
    if (!oobe_ui_ready) {
      run_loop.Run();
    }

    // Skip to the login screen.
    ash::OobeScreenWaiter(ash::OobeBaseTest::GetFirstSigninScreen()).Wait();

    ash::test::UserSessionManagerTestApi session_manager_test_api(
        ash::UserSessionManager::GetInstance());
    session_manager_test_api.SetShouldObtainTokenHandleInTests(false);
  }

  void TearDownOnMainThread() override {
    BrowserList::RemoveObserver(this);
    DevicePolicyCrosBrowserTest::TearDownOnMainThread();
  }

  // user_manager::UserManager::Observer:
  void LocalStateChanged(user_manager::UserManager* user_manager) override {
    if (local_state_changed_run_loop_) {
      local_state_changed_run_loop_->Quit();
    }
  }

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override {
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  // extensions::AppWindowRegistry::Observer:
  void OnAppWindowAdded(extensions::AppWindow* app_window) override {
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void OnAppWindowRemoved(extensions::AppWindow* app_window) override {
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void InitializePolicy() {
    device_policy()->policy_data().set_public_key_version(1);
    DeviceLocalAccountTestHelper::SetupDeviceLocalAccount(
        &device_local_account_policy_, kAccountId1, kDisplayName1);
  }

  void BuildDeviceLocalAccountPolicy() {
    device_local_account_policy_.SetDefaultSigningKey();
    device_local_account_policy_.Build();
  }

  void UploadDeviceLocalAccountPolicy() {
    BuildDeviceLocalAccountPolicy();
    policy_test_server_mixin_.UpdatePolicy(
        dm_protocol::kChromePublicAccountPolicyType, kAccountId1,
        device_local_account_policy_.payload().SerializeAsString());
  }

  void UploadAndInstallDeviceLocalAccountPolicy() {
    UploadDeviceLocalAccountPolicy();
    session_manager_client()->set_device_local_account_policy(
        kAccountId1, device_local_account_policy_.GetBlob());
  }

  void SetRecommendedLocales(const char* const recommended_locales[],
                             size_t array_size) {
    em::StringListPolicyProto* session_locales_proto =
        device_local_account_policy_.payload().mutable_sessionlocales();
    session_locales_proto->mutable_policy_options()->set_mode(
        em::PolicyOptions_PolicyMode_RECOMMENDED);
    session_locales_proto->mutable_value()->Clear();
    for (size_t i = 0; i < array_size; ++i) {
      session_locales_proto->mutable_value()->add_entries(
          recommended_locales[i]);
    }
  }

  void WaitForPublicSessionLocalesChange(const AccountId& account_id) {
    std::vector<ash::LocaleItem> locales =
        ash::LoginScreenTestApi::GetPublicSessionLocales(account_id);
    ash::test::TestPredicateWaiter(
        base::BindRepeating(
            [](const std::vector<ash::LocaleItem>& locales,
               const AccountId& account_id) {
              return locales !=
                     ash::LoginScreenTestApi::GetPublicSessionLocales(
                         account_id);
            },
            locales, account_id))
        .Wait();
  }

  void AddPublicSessionToDevicePolicy(const std::string& username) {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    DeviceLocalAccountTestHelper::AddPublicSession(&proto, username);
    RefreshDevicePolicy();
    policy_test_server_mixin_.UpdateDevicePolicy(proto);
  }

  void EnableAutoLogin() {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    em::DeviceLocalAccountsProto* device_local_accounts =
        proto.mutable_device_local_accounts();
    device_local_accounts->set_auto_login_id(kAccountId1);
    device_local_accounts->set_auto_login_delay(0);
    RefreshDevicePolicy();
    policy_test_server_mixin_.UpdateDevicePolicy(proto);
  }

  void CheckPublicSessionPresent(const AccountId& account_id) {
    const user_manager::User* user =
        user_manager::UserManager::Get()->FindUser(account_id);
    ASSERT_TRUE(user) << " account " << account_id.GetUserEmail()
                      << " not found";
    EXPECT_EQ(account_id, user->GetAccountId());
    EXPECT_EQ(user_manager::UserType::kPublicAccount, user->GetType());
  }

  void SetSystemTimezoneAutomaticDetectionPolicy(
      em::SystemTimezoneProto_AutomaticTimezoneDetectionType policy) {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_system_timezone()->set_timezone_detection_type(policy);
    RefreshDevicePolicy();

    LocalStateValueWaiter(prefs::kSystemTimezoneAutomaticDetectionPolicy,
                          base::Value(policy))
        .Wait();
    policy_test_server_mixin_.UpdateDevicePolicy(proto);
  }

  base::FilePath GetExtensionCacheDirectoryForAccountID(
      const std::string& account_id) {
    base::FilePath extension_cache_root_dir;
    if (!base::PathService::Get(ash::DIR_DEVICE_LOCAL_ACCOUNT_EXTENSIONS,
                                &extension_cache_root_dir)) {
      ADD_FAILURE();
    }
    return extension_cache_root_dir.Append(base::HexEncode(account_id));
  }

  base::FilePath GetCacheCRXFilePath(const std::string& id,
                                     const std::string& version,
                                     const base::FilePath& path) {
    return path.Append(
        extensions::LocalExtensionCache::ExtensionFileName(id, version, ""));
  }

  base::FilePath GetCacheCRXFile(const std::string& account_id,
                                 const std::string& id,
                                 const std::string& version) {
    return GetCacheCRXFilePath(
        id, version, GetExtensionCacheDirectoryForAccountID(account_id));
  }

  // Returns a profile which can be used for testing.
  Profile* GetProfileForTest() {
    // Any profile can be used here since this test does not test multi profile.
    return ProfileManager::GetActiveUserProfile();
  }

  void WaitForDisplayName(const std::string& user_id,
                          const std::string& expected_display_name) {
    DictionaryLocalStateValueWaiter("UserDisplayName", expected_display_name,
                                    user_id)
        .Wait();
  }

  void WaitForPolicy() {
    // Wait for the display name becoming available as that indicates
    // device-local account policy is fully loaded, which is a prerequisite for
    // successful login.
    WaitForDisplayName(account_id_1_.GetUserEmail(), kDisplayName1);
  }

  void ExpandPublicSessionPod(bool expect_advanced) {
    ASSERT_TRUE(ash::LoginScreenTestApi::ExpandPublicSessionPod(account_id_1_));
    ASSERT_EQ(ash::LoginScreenTestApi::IsExpandedPublicSessionAdvanced(),
              expect_advanced);
    // Verify that the construction of the pod's language list did not affect
    // the current ICU locale.
    EXPECT_EQ(initial_language_, icu::Locale::getDefault().getLanguage());
  }

  // GetKeyboardLayoutsForLocale() posts a task to a background task runner and
  // handles the response on the main thread. This method flushes both the
  // thread pool backing the background task runner and the main thread.
  void WaitForGetKeyboardLayoutsForLocaleToFinish() {
    content::RunAllTasksUntilIdle();

    // Verify that the construction of the keyboard layout list did not affect
    // the current ICU locale.
    EXPECT_EQ(initial_language_, icu::Locale::getDefault().getLanguage());
  }

  void StartLogin(const std::string& locale, const std::string& input_method) {
    // Start login into the device-local account.
    auto* host = ash::LoginDisplayHost::default_host();
    ASSERT_TRUE(host);
    host->StartSignInScreen();
    auto* controller = ash::ExistingUserController::current_controller();
    ASSERT_TRUE(controller);

    ash::UserContext user_context(user_manager::UserType::kPublicAccount,
                                  account_id_1_);
    user_context.SetPublicSessionLocale(locale);
    user_context.SetPublicSessionInputMethod(input_method);
    controller->Login(user_context, ash::SigninSpecifics());
  }

  void WaitForSessionStart() {
    if (IsSessionStarted()) {
      return;
    }
    if (ash::WizardController::default_controller()) {
      ash::WizardController::default_controller()
          ->SkipPostLoginScreensForTesting();
    }
    ash::test::WaitForPrimaryUserSessionStart();
  }

  void WaitUntilLocalStateChanged() {
    local_state_changed_run_loop_ = std::make_unique<base::RunLoop>();
    user_manager::UserManager::Get()->AddObserver(this);
    local_state_changed_run_loop_->Run();
    user_manager::UserManager::Get()->RemoveObserver(this);
  }

  static std::string GetDefaultKeyboardIdFromLanguageCode(
      const std::string& language_code) {
    auto* input_method_manager = ash::input_method::InputMethodManager::Get();
    std::vector<std::string> layouts_from_locale;
    input_method_manager->GetInputMethodUtil()
        ->GetInputMethodIdsFromLanguageCode(
            language_code, ash::input_method::kKeyboardLayoutsOnly,
            &layouts_from_locale);
    EXPECT_FALSE(layouts_from_locale.empty());
    if (layouts_from_locale.empty()) {
      return std::string();
    }
    return layouts_from_locale.front();
  }
  void VerifyKeyboardLayoutMatchesLocale() {
    auto* input_method_manager = ash::input_method::InputMethodManager::Get();
    EXPECT_EQ(GetDefaultKeyboardIdFromLanguageCode(
                  g_browser_process->GetApplicationLocale()),
              input_method_manager->GetActiveIMEState()
                  ->GetCurrentInputMethod()
                  .id());
  }

  void RunWithRecommendedLocale(const char* const locales[],
                                size_t locales_size) {
    SetRecommendedLocales(locales, locales_size);
    UploadAndInstallDeviceLocalAccountPolicy();
    AddPublicSessionToDevicePolicy(kAccountId1);
    EnableAutoLogin();

    WaitForPolicy();

    WaitForSessionStart();

    EXPECT_EQ(locales[0], g_browser_process->GetApplicationLocale());
    EXPECT_EQ(l10n_util::GetLanguage(locales[0]),
              icu::Locale::getDefault().getLanguage());
    VerifyKeyboardLayoutMatchesLocale();
  }

  void SetSessionLengthLimitPolicy(int limit) {
    device_local_account_policy_.payload()
        .mutable_sessionlengthlimit()
        ->set_value(limit);
    UploadAndInstallDeviceLocalAccountPolicy();
    AddPublicSessionToDevicePolicy(kAccountId1);
    WaitForPolicy();
  }

  const AccountId account_id_1_ = AccountId::FromUserEmail(
      GenerateDeviceLocalAccountUserId(kAccountId1,
                                       DeviceLocalAccountType::kPublicSession));
  const AccountId account_id_2_ = AccountId::FromUserEmail(
      GenerateDeviceLocalAccountUserId(kAccountId2,
                                       DeviceLocalAccountType::kPublicSession));
  const std::string public_session_input_method_id_;

  std::string initial_locale_;
  std::string initial_language_;

  std::unique_ptr<base::RunLoop> run_loop_;

  // Specifically exists to assist with waiting for a LocalStateChanged()
  // invocation.
  std::unique_ptr<base::RunLoop> local_state_changed_run_loop_;

  UserPolicyBuilder device_local_account_policy_;
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};

  // These are member variables so they're guaranteed that the destructors
  // (which may delete a directory) run in a scope where file IO is allowed.
  base::ScopedTempDir temp_dir_;
  base::ScopedTempDir cache_dir_;

 private:
  extensions::SandboxedUnpacker::ScopedVerifierFormatOverrideForTest
      verifier_format_override_{crx_file::VerifierFormat::CRX3};
};

static bool IsKnownUser(const AccountId& account_id) {
  return user_manager::UserManager::Get()->IsKnownUser(account_id);
}

// Helper that listen extension installation when new profile is created.
class ExtensionInstallObserver : public ProfileManagerObserver,
                                 public extensions::ExtensionRegistryObserver {
 public:
  explicit ExtensionInstallObserver(const std::string& extension_id)
      : registry_(nullptr),
        waiting_extension_id_(extension_id),
        observed_(false) {
    profile_manager_observer_.Observe(g_browser_process->profile_manager());
  }

  ExtensionInstallObserver(const ExtensionInstallObserver&) = delete;
  ExtensionInstallObserver& operator=(const ExtensionInstallObserver&) = delete;

  ~ExtensionInstallObserver() override {
    if (registry_ != nullptr) {
      registry_->RemoveObserver(this);
    }
  }

  // Wait until an extension with |extension_id| is installed.
  void Wait() {
    if (!observed_) {
      run_loop_.Run();
    }
  }

 private:
  // extensions::ExtensionRegistryObserver:
  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const extensions::Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override {
    if (waiting_extension_id_ == extension->id()) {
      observed_ = true;
      run_loop_.Quit();
    }
  }

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override {
    // Ignore lock screen apps profile.
    if (ash::ProfileHelper::IsLockScreenAppProfile(profile)) {
      return;
    }
    registry_ = extensions::ExtensionRegistry::Get(profile);
    profile_manager_observer_.Reset();

    // Check if extension is already installed with newly created profile.
    if (registry_->GetInstalledExtension(waiting_extension_id_)) {
      observed_ = true;
      run_loop_.Quit();
      return;
    }

    // Start listening for extension installation.
    registry_->AddObserver(this);
  }

  raw_ptr<extensions::ExtensionRegistry> registry_;
  base::RunLoop run_loop_;
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observer_{this};
  std::string waiting_extension_id_;
  bool observed_;
};

// Fake implementation to advance the clock for SessionLengthLimiter.
class FakeDelegateImpl : public ash::SessionLengthLimiter::Delegate {
 public:
  FakeDelegateImpl() { clock_.SetNow(base::Time::Now()); }

  FakeDelegateImpl(const FakeDelegateImpl&) = delete;
  FakeDelegateImpl& operator=(const FakeDelegateImpl&) = delete;

  ~FakeDelegateImpl() override {}

  const base::Clock* GetClock() const override { return &clock_; }
  void StopSession() override {
    chrome::AttemptUserExit();
    session_stopped_ = true;
  }

  void AdvanceClock(base::TimeDelta delta) { clock_.Advance(delta); }
  bool session_stopped() const { return session_stopped_; }

 private:
  base::SimpleTestClock clock_;
  bool session_stopped_ = false;
};

// Tests that the data associated with a device local account is removed when
// that local account is no longer part of policy.
IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, PRE_DataIsRemoved) {
  AddPublicSessionToDevicePolicy(kAccountId1);
  WaitUntilLocalStateChanged();

  EXPECT_TRUE(IsKnownUser(account_id_1_));
  CheckPublicSessionPresent(account_id_1_);

  // Data for the account is normally added after successful authentication.
  // Shortcut that here.
  ScopedDictPrefUpdate given_name_update(g_browser_process->local_state(),
                                         "UserGivenName");
  given_name_update->Set(account_id_1_.GetUserEmail(), "Elaine");

  // Add some arbitrary data to make sure the "UserGivenName" dictionary isn't
  // cleaning up itself.
  given_name_update->Set("sanity.check@example.com", "Anne");
}

// Disabled on ASan and LSAn builds due to a consistent failure. See
// crbug.com/1004228
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_DataIsRemoved DISABLED_DataIsRemoved
#else
#define MAYBE_DataIsRemoved DataIsRemoved
#endif
IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, MAYBE_DataIsRemoved) {
  // The device local account should have been removed.
  EXPECT_FALSE(g_browser_process->local_state()
                   ->GetDict("UserGivenName")
                   .Find(account_id_1_.GetUserEmail()));

  // The arbitrary data remains.
  const std::string* value = g_browser_process->local_state()
                                 ->GetDict("UserGivenName")
                                 .FindString("sanity.check@example.com");
  ASSERT_TRUE(value);
  EXPECT_EQ("Anne", *value);
}

// Test is flaky: https://crbug.com/1334470
IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, DISABLED_LoginScreen) {
  AddPublicSessionToDevicePolicy(kAccountId1);
  AddPublicSessionToDevicePolicy(kAccountId2);

  WaitUntilLocalStateChanged();
  EXPECT_TRUE(IsKnownUser(account_id_1_));
  EXPECT_TRUE(IsKnownUser(account_id_2_));

  CheckPublicSessionPresent(account_id_1_);
  CheckPublicSessionPresent(account_id_2_);

  ASSERT_TRUE(user_manager::UserManager::Get()->FindUser(account_id_1_));
  EXPECT_TRUE(user_manager::UserManager::Get()
                  ->FindUser(account_id_1_)
                  ->IsAffiliated());

  ASSERT_TRUE(user_manager::UserManager::Get()->FindUser(account_id_2_));
  EXPECT_TRUE(user_manager::UserManager::Get()
                  ->FindUser(account_id_2_)
                  ->IsAffiliated());
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, DisplayName) {
  base::AddFeatureIdTagToTestResult(DeviceLocalAccountTest::kDisplayNameTag);

  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  // Verify that the display name is shown in the UI.
  std::string display_name =
      ash::LoginScreenTestApi::GetDisplayedName(account_id_1_);
  EXPECT_EQ(kDisplayName1, display_name);
  // Click on the pod to expand it.
  ASSERT_TRUE(ash::LoginScreenTestApi::ExpandPublicSessionPod(account_id_1_));
  // Change the display name.
  device_local_account_policy_.payload().mutable_userdisplayname()->set_value(
      kDisplayName2);
  UploadAndInstallDeviceLocalAccountPolicy();
  DeviceLocalAccountPolicyBroker* broker =
      GetDeviceLocalAccountPolicyBroker(account_id_1_);
  ASSERT_TRUE(broker);
  broker->core()->client()->FetchPolicy(PolicyFetchReason::kTest);
  WaitForDisplayName(account_id_1_.GetUserEmail(), kDisplayName2);

  // Verify that the new display name is shown in the UI.
  display_name = ash::LoginScreenTestApi::GetDisplayedName(account_id_1_);
  EXPECT_EQ(kDisplayName2, display_name);
  // Verify that the pod is still expanded. This indicates that the UI updated
  // without reloading and losing state.
  EXPECT_TRUE(ash::LoginScreenTestApi::IsPublicSessionExpanded());
}

// Tests that display name is saved in kUserDisplayName pref in local state.
IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, CachedDisplayName) {
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForDisplayName(account_id_1_.GetUserEmail(), kDisplayName1);
  const auto& dict =
      g_browser_process->local_state()->GetDict(key::kUserDisplayName);
  ASSERT_TRUE(dict.Find(account_id_1_.GetUserEmail()) != nullptr);
  EXPECT_EQ(kDisplayName1, *dict.FindString(account_id_1_.GetUserEmail()));
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, PolicyDownload) {
  UploadDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  // Sanity check: The policy should be present now.
  ASSERT_FALSE(session_manager_client()
                   ->device_local_account_policy(kAccountId1)
                   .empty());
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, AccountListChange) {
  AddPublicSessionToDevicePolicy(kAccountId1);
  AddPublicSessionToDevicePolicy(kAccountId2);

  WaitUntilLocalStateChanged();
  EXPECT_TRUE(IsKnownUser(account_id_1_));
  EXPECT_TRUE(IsKnownUser(account_id_2_));

  // Update policy to remove kAccountId2.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_device_local_accounts()->clear_account();
  AddPublicSessionToDevicePolicy(kAccountId1);

  em::ChromeDeviceSettingsProto policy;
  policy.mutable_show_user_names()->set_show_user_names(true);
  em::DeviceLocalAccountInfoProto* account1 =
      policy.mutable_device_local_accounts()->add_account();
  account1->set_account_id(kAccountId1);
  account1->set_type(
      em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);

  policy_test_server_mixin_.UpdateDevicePolicy(policy);
  g_browser_process->policy_service()->RefreshPolicies(
      base::OnceClosure(), PolicyFetchReason::kTest);

  // Make sure the second device-local account disappears.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsKnownUser(account_id_2_));
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, StartSession) {
  // Specify startup pages.
  device_local_account_policy_.payload().mutable_restoreonstartup()->set_value(
      SessionStartupPref::kPrefValueURLs);
  em::StringListPolicyProto* startup_urls_proto =
      device_local_account_policy_.payload().mutable_restoreonstartupurls();
  for (size_t i = 0; i < std::size(kStartupURLs); ++i) {
    startup_urls_proto->mutable_value()->add_entries(kStartupURLs[i]);
  }
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));
  WaitForSessionStart();

  // Check that the startup pages specified in policy were opened.
  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());
  Browser* browser = browser_list->get(0);
  ASSERT_TRUE(browser);

  TabStripModel* tabs = browser->tab_strip_model();
  ASSERT_TRUE(tabs);
  int expected_tab_count = static_cast<int>(std::size(kStartupURLs));
  EXPECT_EQ(expected_tab_count, tabs->count());
  for (int i = 0; i < expected_tab_count && i < tabs->count(); ++i) {
    EXPECT_EQ(GURL(kStartupURLs[i]),
              tabs->GetWebContentsAt(i)->GetVisibleURL());
  }

  // Verify that the session is not considered to be logged in with a GAIA
  // account.
  Profile* profile = GetProfileForTest();
  ASSERT_TRUE(profile);
  EXPECT_FALSE(
      IdentityManagerFactory::GetForProfile(profile)->HasPrimaryAccount(
          signin::ConsentLevel::kSignin));
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, FullscreenAllowed) {
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));
  WaitForSessionStart();

  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());
  Browser* browser = browser_list->get(0);
  ASSERT_TRUE(browser);
  BrowserWindow* browser_window = browser->window();
  ASSERT_TRUE(browser_window);

  // Verify that an attempt to enter fullscreen mode is allowed.
  EXPECT_FALSE(browser_window->IsFullscreen());
  chrome::ToggleFullscreenMode(browser);
  EXPECT_TRUE(browser_window->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, ExtensionsUncached) {
  base::AddFeatureIdTagToTestResult(
      DeviceLocalAccountTest::kExtensionsUncachedTag);

  // Make it possible to force-install a hosted app and an extension.
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  scoped_refptr<TestingUpdateManifestProvider> testing_update_manifest_provider(
      new TestingUpdateManifestProvider(kRelativeUpdateURL));
  testing_update_manifest_provider->AddUpdate(
      kHostedAppID, kHostedAppVersion,
      embedded_test_server()->GetURL(std::string("/") + kHostedAppCRXPath));
  testing_update_manifest_provider->AddUpdate(
      kGoodExtensionID, kGoodExtensionVersion,
      embedded_test_server()->GetURL(std::string("/") + kGoodExtensionCRXPath));
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&TestingUpdateManifestProvider::HandleRequest,
                          testing_update_manifest_provider));
  embedded_test_server()->StartAcceptingConnections();

  // Specify policy to force-install the hosted app and the extension.
  em::StringList* forcelist = device_local_account_policy_.payload()
                                  .mutable_extensioninstallforcelist()
                                  ->mutable_value();
  forcelist->add_entries(base::StringPrintf(
      "%s;%s", kHostedAppID,
      embedded_test_server()->GetURL(kRelativeUpdateURL).spec().c_str()));
  forcelist->add_entries(base::StringPrintf(
      "%s;%s", kGoodExtensionID,
      embedded_test_server()->GetURL(kRelativeUpdateURL).spec().c_str()));

  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  // Start listening for app/extension installation results.
  ExtensionInstallObserver hosted_app_install_observer(kHostedAppID);
  ExtensionInstallObserver extension_install_observer(kGoodExtensionID);
  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));

  // Wait for the hosted app & extension installations to succeed.
  hosted_app_install_observer.Wait();
  extension_install_observer.Wait();

  // Verify that the hosted app & extension were installed.
  Profile* profile = GetProfileForTest();
  ASSERT_TRUE(profile);
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  EXPECT_TRUE(extension_registry->enabled_extensions().GetByID(kHostedAppID));
  EXPECT_TRUE(
      extension_registry->enabled_extensions().GetByID(kGoodExtensionID));

  // Verify that the hosted app & extension were downloaded to the account's
  // extension cache.
  base::FilePath test_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  EXPECT_TRUE(ContentsEqual(
      GetCacheCRXFile(kAccountId1, kHostedAppID, kHostedAppVersion),
      test_dir.Append(kHostedAppCRXPath)));
  EXPECT_TRUE(ContentsEqual(
      GetCacheCRXFile(kAccountId1, kGoodExtensionID, kGoodExtensionVersion),
      test_dir.Append(kGoodExtensionCRXPath)));
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, ExtensionsCached) {
  base::AddFeatureIdTagToTestResult(
      DeviceLocalAccountTest::kExtensionsCachedTag);

  ASSERT_TRUE(embedded_test_server()->Start());

  // Pre-populate the device local account's extension cache with a hosted app
  // and an extension.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::CreateDirectory(
        GetExtensionCacheDirectoryForAccountID(kAccountId1)));
  }
  base::FilePath test_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  const base::FilePath cached_hosted_app =
      GetCacheCRXFile(kAccountId1, kHostedAppID, kHostedAppVersion);
  const base::FilePath cached_extension =
      GetCacheCRXFile(kAccountId1, kGoodExtensionID, kGoodExtensionVersion);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(
        CopyFile(test_dir.Append(kHostedAppCRXPath), cached_hosted_app));
    EXPECT_TRUE(
        CopyFile(test_dir.Append(kGoodExtensionCRXPath), cached_extension));
  }

  // Specify policy to force-install the hosted app & the extension.
  em::StringList* forcelist = device_local_account_policy_.payload()
                                  .mutable_extensioninstallforcelist()
                                  ->mutable_value();
  forcelist->add_entries(base::StringPrintf(
      "%s;%s", kHostedAppID,
      embedded_test_server()->GetURL(kRelativeUpdateURL).spec().c_str()));
  forcelist->add_entries(base::StringPrintf(
      "%s;%s", kGoodExtensionID,
      embedded_test_server()->GetURL(kRelativeUpdateURL).spec().c_str()));

  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  // Start listening for app/extension installation results.
  ExtensionInstallObserver hosted_app_install_observer(kHostedAppID);
  ExtensionInstallObserver extension_install_observer(kGoodExtensionID);

  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));

  // Wait for the hosted app & extension installations to succeed.
  hosted_app_install_observer.Wait();
  extension_install_observer.Wait();

  // Verify that the hosted app & extension were installed.
  Profile* profile = GetProfileForTest();
  ASSERT_TRUE(profile);
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  EXPECT_TRUE(extension_registry->enabled_extensions().GetByID(kHostedAppID));
  EXPECT_TRUE(
      extension_registry->enabled_extensions().GetByID(kGoodExtensionID));

  // Verify that the hosted app is still in the account's extension cache.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(PathExists(cached_hosted_app));
  }

  // Verify that the extension is still in the account's extension cache.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(PathExists(cached_extension));
  }
}

static void OnPutExtension(std::unique_ptr<base::RunLoop>* run_loop,
                           const base::FilePath& file_path,
                           bool file_ownership_passed) {
  ASSERT_TRUE(*run_loop);
  (*run_loop)->Quit();
}

static void OnExtensionCacheImplInitialized(
    std::unique_ptr<base::RunLoop>* run_loop) {
  ASSERT_TRUE(*run_loop);
  (*run_loop)->Quit();
}

static void CreateFile(const base::FilePath& file,
                       size_t size,
                       const base::Time& timestamp) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string data(size, 0);
  EXPECT_TRUE(base::WriteFile(file, data));
  EXPECT_TRUE(base::TouchFile(file, timestamp, timestamp));
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, ExtensionCacheImplTest) {
  // Make it possible to force-install a hosted app and an extension.
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  scoped_refptr<TestingUpdateManifestProvider> testing_update_manifest_provider(
      new TestingUpdateManifestProvider(kRelativeUpdateURL));
  testing_update_manifest_provider->AddUpdate(
      kHostedAppID, kHostedAppVersion,
      embedded_test_server()->GetURL(std::string("/") + kHostedAppCRXPath));
  testing_update_manifest_provider->AddUpdate(
      kGoodExtensionID, kGoodExtensionVersion,
      embedded_test_server()->GetURL(std::string("/") + kGoodExtensionCRXPath));
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&TestingUpdateManifestProvider::HandleRequest,
                          testing_update_manifest_provider));
  embedded_test_server()->StartAcceptingConnections();
  // Create and initialize local cache.
  base::FilePath impl_path;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(cache_dir_.CreateUniqueTempDir());
    impl_path = cache_dir_.GetPath();
    EXPECT_TRUE(base::CreateDirectory(impl_path));
  }
  CreateFile(impl_path.Append(
                 extensions::LocalExtensionCache::kCacheReadyFlagFileName),
             0, base::Time::Now());
  extensions::ExtensionCacheImpl cache_impl(
      std::make_unique<extensions::ChromeOSExtensionCacheDelegate>(impl_path));
  auto run_loop = std::make_unique<base::RunLoop>();
  cache_impl.Start(base::BindOnce(&OnExtensionCacheImplInitialized, &run_loop));
  run_loop->Run();

  // Put extension in the local cache.
  base::FilePath temp_path;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    temp_path = temp_dir_.GetPath();
    EXPECT_TRUE(base::CreateDirectory(temp_path));
  }
  const base::FilePath temp_file =
      GetCacheCRXFilePath(kGoodExtensionID, kGoodExtensionVersion, temp_path);
  base::FilePath test_dir;
  std::string hash;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(CopyFile(test_dir.Append(kGoodExtensionCRXPath), temp_file));
  }
  cache_impl.AllowCaching(kGoodExtensionID);
  run_loop = std::make_unique<base::RunLoop>();
  cache_impl.PutExtension(kGoodExtensionID, hash, temp_file,
                          kGoodExtensionVersion,
                          base::BindOnce(&OnPutExtension, &run_loop));
  run_loop->Run();

  // Verify that the extension file was added to the local cache.
  const base::FilePath local_file =
      GetCacheCRXFilePath(kGoodExtensionID, kGoodExtensionVersion, impl_path);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(PathExists(local_file));
  }

  // Specify policy to force-install the hosted app and the extension.
  em::StringList* forcelist = device_local_account_policy_.payload()
                                  .mutable_extensioninstallforcelist()
                                  ->mutable_value();
  forcelist->add_entries(base::StringPrintf(
      "%s;%s", kHostedAppID,
      embedded_test_server()->GetURL(kRelativeUpdateURL).spec().c_str()));
  forcelist->add_entries(base::StringPrintf(
      "%s;%s", kGoodExtensionID,
      embedded_test_server()->GetURL(kRelativeUpdateURL).spec().c_str()));

  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  // Start listening for app/extension installation results.
  ExtensionInstallObserver hosted_app_install_observer(kHostedAppID);
  ExtensionInstallObserver extension_install_observer(kGoodExtensionID);

  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));

  // Wait for the hosted app & extension installations to succeed.
  hosted_app_install_observer.Wait();
  extension_install_observer.Wait();

  // Verify that the extension was kept in the local cache.
  EXPECT_TRUE(
      cache_impl.GetExtension(kGoodExtensionID, hash, nullptr, nullptr));

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    // Verify that the extension file was kept in the local cache.
    EXPECT_TRUE(PathExists(local_file));
  }
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, ExternalData) {
  // user_manager::UserManager requests an external data fetch whenever
  // the key::kUserAvatarImage policy is set. Since this test wants to
  // verify that the underlying policy subsystem will start a fetch
  // without this request as well, the user_manager::UserManager must be
  // prevented from seeing the policy change.
  g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->OnUserManagerShutdown();

  UploadDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  // Start serving external data.
  std::unique_ptr<base::RunLoop> run_loop(new base::RunLoop);
  EXPECT_TRUE(embedded_test_server()->InitializeAndListen());
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != kExternalDataPath) {
          return nullptr;
        }

        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_content(kExternalData);

        quit_closure.Run();
        return response;
      },
      run_loop->QuitClosure()));
  embedded_test_server()->StartAcceptingConnections();

  // Specify an external data reference for the key::kUserAvatarImage policy.
  base::Value::Dict metadata = test::ConstructExternalDataReference(
      embedded_test_server()->GetURL(kExternalDataPath).spec(), kExternalData);
  std::string policy;
  base::JSONWriter::Write(metadata, &policy);
  device_local_account_policy_.payload().mutable_useravatarimage()->set_value(
      policy);
  UploadAndInstallDeviceLocalAccountPolicy();
  DeviceLocalAccountPolicyBroker* broker =
      GetDeviceLocalAccountPolicyBroker(account_id_1_);
  ASSERT_TRUE(broker);
  broker->core()->store()->Load();

  // The external data should be fetched and cached automatically. Wait for this
  // fetch.
  run_loop->Run();

  // Stop serving external data.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  const PolicyMap::Entry* policy_entry =
      broker->core()->store()->policy_map().Get(key::kUserAvatarImage);
  ASSERT_TRUE(policy_entry);
  ASSERT_TRUE(policy_entry->external_data_fetcher);

  // Retrieve the external data. Although the data is no longer being served,
  // the retrieval should succeed because the data has been cached.
  {
    base::test::TestFuture<std::unique_ptr<std::string>, const base::FilePath&>
        fetch_data_future;
    policy_entry->external_data_fetcher->Fetch(fetch_data_future.GetCallback());
    ASSERT_TRUE(fetch_data_future.Get<std::unique_ptr<std::string>>());
    EXPECT_EQ(kExternalData,
              *fetch_data_future.Get<std::unique_ptr<std::string>>());
  }

  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));
  WaitForSessionStart();

  // Verify that the external data reference has propagated to the device-local
  // account's ProfilePolicyConnector.
  ProfilePolicyConnector* policy_connector =
      GetProfileForTest()->GetProfilePolicyConnector();
  ASSERT_TRUE(policy_connector);
  const PolicyMap& policies = policy_connector->policy_service()->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  policy_entry = policies.Get(key::kUserAvatarImage);
  ASSERT_TRUE(policy_entry);
  ASSERT_TRUE(policy_entry->value(base::Value::Type::DICT));
  EXPECT_EQ(metadata, policy_entry->value(base::Value::Type::DICT)->GetDict());
  ASSERT_TRUE(policy_entry->external_data_fetcher);

  // Retrieve the external data via the ProfilePolicyConnector. The retrieval
  // should succeed because the data has been cached.
  {
    base::test::TestFuture<std::unique_ptr<std::string>, const base::FilePath&>
        fetch_data_future;
    policy_entry->external_data_fetcher->Fetch(fetch_data_future.GetCallback());
    ASSERT_TRUE(fetch_data_future.Get<std::unique_ptr<std::string>>());
    EXPECT_EQ(kExternalData,
              *fetch_data_future.Get<std::unique_ptr<std::string>>());
  }
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, UserAvatarImage) {
  base::AddFeatureIdTagToTestResult(
      DeviceLocalAccountTest::kUserAvatarImageTag);

  ASSERT_TRUE(embedded_test_server()->Start());

  UploadDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  base::FilePath test_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  std::string image_data;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(
        test_dir.Append(ash::test::kUserAvatarImage1RelativePath),
        &image_data));
  }

  std::string policy;
  base::JSONWriter::Write(
      test::ConstructExternalDataReference(
          embedded_test_server()
              ->GetURL(std::string("/") +
                       ash::test::kUserAvatarImage1RelativePath)
              .spec(),
          image_data),
      &policy);
  device_local_account_policy_.payload().mutable_useravatarimage()->set_value(
      policy);
  UploadAndInstallDeviceLocalAccountPolicy();
  DeviceLocalAccountPolicyBroker* broker =
      GetDeviceLocalAccountPolicyBroker(account_id_1_);
  ASSERT_TRUE(broker);

  broker->core()->store()->Load();
  WaitUntilLocalStateChanged();

  gfx::ImageSkia policy_image =
      ash::test::ImageLoader(
          test_dir.Append(ash::test::kUserAvatarImage1RelativePath))
          .Load();
  ASSERT_FALSE(policy_image.isNull());

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id_1_);
  ASSERT_TRUE(user);

  base::FilePath user_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  const base::FilePath saved_image_path =
      user_data_dir.Append(account_id_1_.GetUserEmail()).AddExtension("jpg");

  EXPECT_EQ(user_manager::UserImage::Type::kExternal, user->image_index());
  EXPECT_TRUE(ash::test::AreImagesEqual(policy_image, user->GetImage()));
  const base::Value::Dict& images_pref =
      g_browser_process->local_state()->GetDict("user_image_info");
  const base::Value::Dict* image_properties =
      images_pref.FindDict(account_id_1_.GetUserEmail());
  ASSERT_TRUE(image_properties);
  std::optional<int> image_index = image_properties->FindInt("index");
  const std::string* image_path = image_properties->FindString("path");
  ASSERT_TRUE(image_index.has_value());
  ASSERT_TRUE(image_path);
  EXPECT_EQ(user_manager::UserImage::Type::kExternal, image_index.value());
  EXPECT_EQ(saved_image_path.value(), *image_path);

  gfx::ImageSkia saved_image = ash::test::ImageLoader(saved_image_path).Load();
  ASSERT_FALSE(saved_image.isNull());

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(policy_image.width(), saved_image.width());
  EXPECT_EQ(policy_image.height(), saved_image.height());
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, LastWindowClosedLogoutReminder) {
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));
  WaitForSessionStart();

  Profile* profile = GetProfileForTest();
  ASSERT_TRUE(profile);
  extensions::AppWindowRegistry* app_window_registry =
      extensions::AppWindowRegistry::Get(profile);
  app_window_registry->AddObserver(this);

  // Verify that the logout confirmation dialog is not showing.
  EXPECT_FALSE(IsLogoutConfirmationDialogShowing());

  // Remove policy that allows only explicitly allowlisted apps to be installed
  // in a public session.
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  ASSERT_TRUE(extension_system);
  extension_system->management_policy()->UnregisterAllProviders();

  // Install and a platform app.
  scoped_refptr<extensions::CrxInstaller> installer =
      extensions::CrxInstaller::CreateSilent(
          extension_system->extension_service());
  installer->set_allow_silent_install(true);
  installer->set_install_cause(extension_misc::INSTALL_CAUSE_USER_DOWNLOAD);
  installer->set_creation_flags(extensions::Extension::FROM_WEBSTORE);

  {
    TestFuture<const std::optional<CrxInstallError>&> installer_done_future;
    installer->AddInstallerCallback(installer_done_future.GetCallback());

    base::FilePath test_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
    installer->InstallCrx(test_dir.Append(kPackagedAppCRXPath));

    const std::optional<CrxInstallError>& error = installer_done_future.Get();
    EXPECT_THAT(error, testing::Eq(std::nullopt));
  }

  const extensions::Extension* app = installer->extension();

  // Start the platform app, causing it to open a window.
  run_loop_ = std::make_unique<base::RunLoop>();
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  proxy->Launch(app->id(),
                apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                                    false /* preferred_containner */),
                apps::LaunchSource::kFromChromeInternal,
                std::make_unique<apps::WindowInfo>(
                    display::Screen::GetScreen()->GetPrimaryDisplay().id()));
  run_loop_->Run();
  EXPECT_EQ(1U, app_window_registry->app_windows().size());

  // Close the only open browser window.
  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());
  Browser* browser = browser_list->get(0);
  ASSERT_TRUE(browser);
  BrowserWindow* browser_window = browser->window();
  ASSERT_TRUE(browser_window);
  run_loop_ = std::make_unique<base::RunLoop>();
  browser_window->Close();
  browser_window = nullptr;
  run_loop_->Run();
  browser = nullptr;
  EXPECT_TRUE(browser_list->empty());

  // Verify that the logout confirmation dialog is not showing because an app
  // window is still open.
  EXPECT_FALSE(IsLogoutConfirmationDialogShowing());

  // Open a browser window.
  Browser* first_browser = CreateBrowser(profile);
  EXPECT_EQ(1U, browser_list->size());

  // Close the app window.
  run_loop_ = std::make_unique<base::RunLoop>();
  ASSERT_EQ(1U, app_window_registry->app_windows().size());
  app_window_registry->app_windows().front()->GetBaseWindow()->Close();
  run_loop_->Run();
  EXPECT_TRUE(app_window_registry->app_windows().empty());

  // Verify that the logout confirmation dialog is not showing because a browser
  // window is still open.
  EXPECT_FALSE(IsLogoutConfirmationDialogShowing());

  // Open a second browser window.
  Browser* second_browser = CreateBrowser(profile);
  EXPECT_EQ(2U, browser_list->size());

  // Close the first browser window.
  browser_window = first_browser->window();
  ASSERT_TRUE(browser_window);
  run_loop_ = std::make_unique<base::RunLoop>();
  browser_window->Close();
  browser_window = nullptr;
  run_loop_->Run();
  first_browser = nullptr;
  EXPECT_EQ(1U, browser_list->size());

  // Verify that the logout confirmation dialog is not showing because a browser
  // window is still open.
  EXPECT_FALSE(IsLogoutConfirmationDialogShowing());

  // Close the second browser window.
  browser_window = second_browser->window();
  ASSERT_TRUE(browser_window);
  run_loop_ = std::make_unique<base::RunLoop>();
  browser_window->Close();
  browser_window = nullptr;
  run_loop_->Run();
  second_browser = nullptr;
  EXPECT_TRUE(browser_list->empty());

  // Verify that the logout confirmation dialog is showing.
  EXPECT_TRUE(IsLogoutConfirmationDialogShowing());

  // Deny the logout.
  ASSERT_NO_FATAL_FAILURE(CloseLogoutConfirmationDialog());

  // Verify that the logout confirmation dialog is no longer showing.
  EXPECT_FALSE(IsLogoutConfirmationDialogShowing());

  // Open a browser window.
  browser = CreateBrowser(profile);
  EXPECT_EQ(1U, browser_list->size());

  // Close the browser window.
  browser_window = browser->window();
  ASSERT_TRUE(browser_window);
  run_loop_ = std::make_unique<base::RunLoop>();
  browser_window->Close();
  browser_window = nullptr;
  run_loop_->Run();
  browser = nullptr;
  EXPECT_TRUE(browser_list->empty());

  // Verify that the logout confirmation dialog is showing again.
  EXPECT_TRUE(IsLogoutConfirmationDialogShowing());

  // Deny the logout.
  ASSERT_NO_FATAL_FAILURE(CloseLogoutConfirmationDialog());

  app_window_registry->RemoveObserver(this);
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, NoRecommendedLocaleNoSwitch) {
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  ExpandPublicSessionPod(false);

  // Click the enter button to start the session.
  ash::LoginScreenTestApi::ClickPublicExpandedSubmitButton();

  WaitForSessionStart();

  // Verify that the locale has not changed and the first keyboard layout
  // applicable to the locale was chosen.
  EXPECT_EQ(initial_locale_, g_browser_process->GetApplicationLocale());
  EXPECT_EQ(initial_language_, icu::Locale::getDefault().getLanguage());
  VerifyKeyboardLayoutMatchesLocale();
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, NoRecommendedLocaleSwitch) {
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  ExpandPublicSessionPod(false);

  // Click the link that switches the pod to its advanced form. Verify that the
  // pod switches from basic to advanced.
  ash::LoginScreenTestApi::ClickPublicExpandedAdvancedViewButton();
  ASSERT_TRUE(ash::LoginScreenTestApi::IsExpandedPublicSessionAdvanced());
  ash::LoginScreenTestApi::SetPublicSessionLocale(kPublicSessionLocale);
  // The UI will have requested an updated list of keyboard layouts at this
  // point. Wait for the constructions of this list to finish.
  WaitForGetKeyboardLayoutsForLocaleToFinish();

  // Manually select a different keyboard layout.
  ash::LoginScreenTestApi::SetPublicSessionKeyboard(
      public_session_input_method_id_);

  ash::LoginScreenTestApi::ClickPublicExpandedSubmitButton();

  WaitForSessionStart();

  // Verify that the locale and keyboard layout have been applied.
  EXPECT_EQ(kPublicSessionLocale, g_browser_process->GetApplicationLocale());
  EXPECT_EQ(l10n_util::GetLanguage(kPublicSessionLocale),
            icu::Locale::getDefault().getLanguage());
  EXPECT_EQ(public_session_input_method_id_,
            ash::input_method::InputMethodManager::Get()
                ->GetActiveIMEState()
                ->GetCurrentInputMethod()
                .id());
}

// Tests whether or not managed guest session users can change the system
// timezone, which should be possible iff the timezone automatic detection
// policy is set to either DISABLED or USERS_DECIDE.
IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, ManagedSessionTimezoneChange) {
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);
  EnableAutoLogin();

  WaitForPolicy();

  WaitForSessionStart();

  CheckPublicSessionPresent(account_id_1_);

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id_1_);
  ASSERT_TRUE(user);
  ASSERT_EQ(user->GetType(), user_manager::UserType::kPublicAccount);

  std::u16string timezone_id1(u"America/Los_Angeles");
  std::string timezone_id2("Europe/Berlin");
  std::u16string timezone_id2_utf16(u"Europe/Berlin");

  ash::system::TimezoneSettings* timezone_settings =
      ash::system::TimezoneSettings::GetInstance();

  timezone_settings->SetTimezoneFromID(timezone_id1);
  SetSystemTimezoneAutomaticDetectionPolicy(em::SystemTimezoneProto::DISABLED);
  ash::system::SetSystemTimezone(user, timezone_id2);
  EXPECT_EQ(timezone_settings->GetCurrentTimezoneID(), timezone_id2_utf16);

  timezone_settings->SetTimezoneFromID(timezone_id1);
  SetSystemTimezoneAutomaticDetectionPolicy(
      em::SystemTimezoneProto::USERS_DECIDE);
  ash::system::SetSystemTimezone(user, timezone_id2);
  EXPECT_EQ(timezone_settings->GetCurrentTimezoneID(), timezone_id2_utf16);

  timezone_settings->SetTimezoneFromID(timezone_id1);
  SetSystemTimezoneAutomaticDetectionPolicy(em::SystemTimezoneProto::IP_ONLY);
  ash::system::SetSystemTimezone(user, timezone_id2);
  EXPECT_NE(timezone_settings->GetCurrentTimezoneID(), timezone_id2_utf16);

  timezone_settings->SetTimezoneFromID(timezone_id1);
  SetSystemTimezoneAutomaticDetectionPolicy(
      em::SystemTimezoneProto::SEND_WIFI_ACCESS_POINTS);
  ash::system::SetSystemTimezone(user, timezone_id2);
  EXPECT_NE(timezone_settings->GetCurrentTimezoneID(), timezone_id2_utf16);

  timezone_settings->SetTimezoneFromID(timezone_id1);
  SetSystemTimezoneAutomaticDetectionPolicy(
      em::SystemTimezoneProto::SEND_ALL_LOCATION_INFO);
  ash::system::SetSystemTimezone(user, timezone_id2);
  EXPECT_NE(timezone_settings->GetCurrentTimezoneID(), timezone_id2_utf16);
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, OneRecommendedLocale) {
  // Specify a recommended locale.
  SetRecommendedLocales(kSingleRecommendedLocale,
                        std::size(kSingleRecommendedLocale));
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  ExpandPublicSessionPod(false);

  WaitForGetKeyboardLayoutsForLocaleToFinish();

  // Click the enter button to start the session.
  ash::LoginScreenTestApi::ClickPublicExpandedSubmitButton();

  WaitForSessionStart();

  // Verify that the recommended locale has been applied and the first keyboard
  // layout applicable to the locale was chosen.
  EXPECT_EQ(kSingleRecommendedLocale[0],
            g_browser_process->GetApplicationLocale());
  EXPECT_EQ(l10n_util::GetLanguage(kSingleRecommendedLocale[0]),
            icu::Locale::getDefault().getLanguage());
  VerifyKeyboardLayoutMatchesLocale();
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, MultipleRecommendedLocales) {
  // Specify recommended locales.
  SetRecommendedLocales(kRecommendedLocales1, std::size(kRecommendedLocales1));
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);
  AddPublicSessionToDevicePolicy(kAccountId2);

  WaitForPolicy();

  ExpandPublicSessionPod(true);

  // Verify that the pod shows a list of locales beginning with the recommended
  // ones, followed by others.
  std::vector<ash::LocaleItem> locales =
      ash::LoginScreenTestApi::GetExpandedPublicSessionLocales();
  EXPECT_LT(std::size(kRecommendedLocales1), locales.size());

  // Verify that the list starts with the recommended locales, in correct order.
  for (size_t i = 0; i < std::size(kRecommendedLocales1); ++i) {
    EXPECT_EQ(kRecommendedLocales1[i], locales[i].language_code);
  }

  // Verify that the recommended locales do not appear again in the remainder of
  // the list.
  std::set<std::string> recommended_locales;
  for (size_t i = 0; i < std::size(kRecommendedLocales1); ++i) {
    recommended_locales.insert(kRecommendedLocales1[i]);
  }
  for (size_t i = std::size(kRecommendedLocales1); i < locales.size(); ++i) {
    const std::string& locale = locales[i].language_code;
    EXPECT_EQ(recommended_locales.end(), recommended_locales.find(locale));
  }

  // Verify that the first recommended locale is selected.
  std::string selected_locale =
      ash::LoginScreenTestApi::GetExpandedPublicSessionSelectedLocale();

  EXPECT_EQ(kRecommendedLocales1[0], selected_locale);

  // Change the list of recommended locales.
  SetRecommendedLocales(kRecommendedLocales2, std::size(kRecommendedLocales2));

  UploadAndInstallDeviceLocalAccountPolicy();
  DeviceLocalAccountPolicyBroker* broker =
      GetDeviceLocalAccountPolicyBroker(account_id_1_);
  ASSERT_TRUE(broker);
  broker->core()->client()->FetchPolicy(PolicyFetchReason::kTest);
  WaitForPublicSessionLocalesChange(account_id_1_);

  // Verify that the new list of locales is shown in the UI.
  locales = ash::LoginScreenTestApi::GetExpandedPublicSessionLocales();
  EXPECT_LT(std::size(kRecommendedLocales2), locales.size());
  for (size_t i = 0; i < std::size(kRecommendedLocales2); ++i) {
    const std::string& locale = locales[i].language_code;
    EXPECT_EQ(kRecommendedLocales2[i], locale);
  }

  // Verify that the first new recommended locale is selected.
  selected_locale =
      ash::LoginScreenTestApi::GetExpandedPublicSessionSelectedLocale();
  EXPECT_EQ(kRecommendedLocales2[0], selected_locale);

  // Manually select a different locale.
  ash::LoginScreenTestApi::SetPublicSessionLocale(kPublicSessionLocale);

  // Change the list of recommended locales.
  SetRecommendedLocales(kRecommendedLocales1, std::size(kRecommendedLocales1));

  UploadAndInstallDeviceLocalAccountPolicy();
  broker->core()->client()->FetchPolicy(PolicyFetchReason::kTest);
  WaitForPublicSessionLocalesChange(account_id_1_);

  // Verify that the manually selected locale is still selected.
  selected_locale =
      ash::LoginScreenTestApi::GetExpandedPublicSessionSelectedLocale();
  EXPECT_EQ(kPublicSessionLocale, selected_locale);

  // The UI will request an updated list of keyboard layouts at this point. Wait
  // for the constructions of this list to finish.
  WaitForGetKeyboardLayoutsForLocaleToFinish();

  // Manually select a different keyboard layout.
  ash::LoginScreenTestApi::SetPublicSessionKeyboard(
      public_session_input_method_id_);

  ASSERT_TRUE(ash::LoginScreenTestApi::HidePublicSessionExpandedPod());

  // Click on a different pod, causing focus to shift away and the pod to
  // contract.
  ASSERT_TRUE(ash::LoginScreenTestApi::FocusUser(account_id_2_));

  // Click on the pod again, causing it to expand again. Verify that the pod has
  // reset all its state (the advanced form is being shown, the manually
  // selected locale and keyboard layout are selected).
  ASSERT_TRUE(ash::LoginScreenTestApi::FocusUser(account_id_1_));
  ExpandPublicSessionPod(true);
  EXPECT_EQ(kRecommendedLocales1[0],
            ash::LoginScreenTestApi::GetExpandedPublicSessionSelectedLocale());

  std::string selected_keyboard_layout =
      ash::LoginScreenTestApi::GetExpandedPublicSessionSelectedKeyboard();

  std::string default_keyboard_id =
      GetDefaultKeyboardIdFromLanguageCode(kRecommendedLocales1[0]);
  EXPECT_EQ(default_keyboard_id, selected_keyboard_layout);

  ash::LoginScreenTestApi::SetPublicSessionLocale(kPublicSessionLocale);
  WaitForGetKeyboardLayoutsForLocaleToFinish();
  ash::LoginScreenTestApi::SetPublicSessionKeyboard(
      public_session_input_method_id_);

  // Click the enter button to start the session.
  ash::LoginScreenTestApi::ClickPublicExpandedSubmitButton();

  WaitForSessionStart();

  // Verify that the locale and keyboard layout have been applied.
  EXPECT_EQ(kPublicSessionLocale, g_browser_process->GetApplicationLocale());
  EXPECT_EQ(l10n_util::GetLanguage(kPublicSessionLocale),
            icu::Locale::getDefault().getLanguage());
  EXPECT_EQ(public_session_input_method_id_,
            ash::input_method::InputMethodManager::Get()
                ->GetActiveIMEState()
                ->GetCurrentInputMethod()
                .id());
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, InvalidRecommendedLocale) {
  // Specify an invalid recommended locale.
  SetRecommendedLocales(kInvalidRecommendedLocale,
                        std::size(kInvalidRecommendedLocale));
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  // Click on the pod to expand it. Verify that the pod expands to its basic
  // form as there is only one recommended locale.
  ExpandPublicSessionPod(false);
  ash::LoginScreenTestApi::ClickPublicExpandedSubmitButton();

  WaitForSessionStart();

  // Verify that since the recommended locale was invalid, the locale has not
  // changed and the first keyboard layout applicable to the locale was chosen.
  EXPECT_EQ(initial_locale_, g_browser_process->GetApplicationLocale());
  EXPECT_EQ(l10n_util::GetLanguage(initial_locale_),
            icu::Locale::getDefault().getLanguage());
  VerifyKeyboardLayoutMatchesLocale();
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, LocaleWithIME) {
  // Specify a locale that has real IMEs in addition to a keyboard layout one.
  const char* const kSingleLocaleWithIME[] = {"ja"};
  RunWithRecommendedLocale(kSingleLocaleWithIME,
                           std::size(kSingleLocaleWithIME));

  EXPECT_GT(ash::input_method::InputMethodManager::Get()
                ->GetActiveIMEState()
                ->GetNumEnabledInputMethods(),
            1u);
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, LocaleWithNoIME) {
  // Specify a locale that has only keyboard layout.
  const char* const kSingleLocaleWithNoIME[] = {"de"};
  RunWithRecommendedLocale(kSingleLocaleWithNoIME,
                           std::size(kSingleLocaleWithNoIME));

  EXPECT_EQ(1u, ash::input_method::InputMethodManager::Get()
                    ->GetActiveIMEState()
                    ->GetNumEnabledInputMethods());
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest,
                       AutoLoginWithoutRecommendedLocales) {
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);
  EnableAutoLogin();

  WaitForPolicy();

  WaitForSessionStart();

  // Verify that the locale has not changed and the first keyboard layout
  // applicable to the locale was chosen.
  EXPECT_EQ(initial_locale_, g_browser_process->GetApplicationLocale());
  EXPECT_EQ(initial_language_, icu::Locale::getDefault().getLanguage());
  VerifyKeyboardLayoutMatchesLocale();
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest,
                       AutoLoginWithRecommendedLocales) {
  // Specify recommended locales.
  SetRecommendedLocales(kRecommendedLocales1, std::size(kRecommendedLocales1));
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);
  EnableAutoLogin();

  WaitForPolicy();

  WaitForSessionStart();

  // Verify that the first recommended locale has been applied and the first
  // keyboard layout applicable to the locale was chosen.
  EXPECT_EQ(kRecommendedLocales1[0], g_browser_process->GetApplicationLocale());
  EXPECT_EQ(l10n_util::GetLanguage(kRecommendedLocales1[0]),
            icu::Locale::getDefault().getLanguage());
  VerifyKeyboardLayoutMatchesLocale();
}

// TODO(crbug.com/40923043): Flaky.
IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest,
                       DISABLED_TermsOfServiceWithLocaleSwitch) {
  // Specify Terms of Service URL.
  ASSERT_TRUE(embedded_test_server()->Start());
  device_local_account_policy_.payload().mutable_termsofserviceurl()->set_value(
      embedded_test_server()
          ->GetURL(std::string("/") + kExistentTermsOfServicePath)
          .spec());
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);
  WaitForPolicy();

  // Prevent browser start in user session so that we do not need to wait
  // for its initialization.
  ash::test::UserSessionManagerTestApi(ash::UserSessionManager::GetInstance())
      .SetShouldLaunchBrowserInTests(false);

  ExpandPublicSessionPod(false);

  // Select a different locale.
  ash::LoginScreenTestApi::SetPublicSessionLocale(kPublicSessionLocale);

  // The UI will have requested an updated list of keyboard layouts at this
  // point. Wait for the constructions of this list to finish.
  WaitForGetKeyboardLayoutsForLocaleToFinish();

  ash::test::ProfilePreparedWaiter profile_prepared(account_id_1_);
  // Manually select a different keyboard layout and click the enter button to
  // start the session.
  ash::LoginScreenTestApi::SetPublicSessionKeyboard(
      public_session_input_method_id_);
  ash::LoginScreenTestApi::ClickPublicExpandedSubmitButton();
  profile_prepared.Wait();

  // Wait for the Terms of Service screen is being shown.
  ash::OobeScreenWaiter(ash::TermsOfServiceScreenView::kScreenId).Wait();

  // Wait for the Terms of Service to finish downloading.
  ash::test::OobeJS()
      .CreateVisibilityWaiter(true, {"terms-of-service", "step-loaded"})
      ->Wait();

  // Verify that the locale and keyboard layout have been applied.
  EXPECT_EQ(kPublicSessionLocale, g_browser_process->GetApplicationLocale());
  EXPECT_EQ(l10n_util::GetLanguage(kPublicSessionLocale),
            icu::Locale::getDefault().getLanguage());
  EXPECT_EQ(public_session_input_method_id_,
            ash::input_method::InputMethodManager::Get()
                ->GetActiveIMEState()
                ->GetCurrentInputMethod()
                .id());

  // Wait for 'tos-accept-button' to become enabled.
  ash::test::OobeJS()
      .CreateEnabledWaiter(true, {"terms-of-service", "acceptButton"})
      ->Wait();

  // Click the accept button.
  ash::test::OobeJS().ClickOnPath({"terms-of-service", "acceptButton"});

  WaitForSessionStart();

  // Verify that the locale and keyboard layout are still in force.
  EXPECT_EQ(kPublicSessionLocale, g_browser_process->GetApplicationLocale());
  EXPECT_EQ(l10n_util::GetLanguage(kPublicSessionLocale),
            icu::Locale::getDefault().getLanguage());
  EXPECT_EQ(public_session_input_method_id_,
            ash::input_method::InputMethodManager::Get()
                ->GetActiveIMEState()
                ->GetCurrentInputMethod()
                .id());
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, PublicSessionWithLocaleSwitch) {
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();
  ExpandPublicSessionPod(false);

  // Select a different locale.
  EXPECT_NE(kPublicSessionLocale, g_browser_process->GetApplicationLocale());
  ash::LoginScreenTestApi::SetPublicSessionLocale(kPublicSessionLocale);

  // Submit the locale change.
  ash::LoginScreenTestApi::ClickPublicExpandedSubmitButton();

  WaitForSessionStart();

  // Verify that the locale.
  EXPECT_EQ(kPublicSessionLocale, g_browser_process->GetApplicationLocale());
  EXPECT_EQ(l10n_util::GetLanguage(kPublicSessionLocale),
            icu::Locale::getDefault().getLanguage());
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, LoginWarningShown) {
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  ExpandPublicSessionPod(false);

  // Click the link that switches the pod to its advanced form. Verify that the
  // pod switches from basic to advanced.
  ash::LoginScreenTestApi::ClickPublicExpandedAdvancedViewButton();
  ASSERT_TRUE(ash::LoginScreenTestApi::IsExpandedPublicSessionAdvanced());
  ASSERT_TRUE(ash::LoginScreenTestApi::IsPublicSessionWarningShown());
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, SessionLengthLimit) {
  base::AddFeatureIdTagToTestResult(
      DeviceLocalAccountTest::kSessionLengthLimitTag);
  constexpr int kThreeHoursInMs = 3 * 60 * 60 * 1000;
  constexpr int kTwoHoursInMs = 2 * 60 * 60 * 1000;

  PolicyTestAppTerminationObserver observer;

  // Install and refresh the device policy now. This will also fetch the initial
  // user policy for the device-local account now.
  SetSessionLengthLimitPolicy(kThreeHoursInMs);

  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));
  WaitForSessionStart();

  // Setup a fake delegate to advance clock.
  auto delegate_ptr = std::make_unique<FakeDelegateImpl>();
  auto* delegate = delegate_ptr.get();
  static_cast<ash::ChromeSessionManager*>(
      session_manager::SessionManager::Get())
      ->GetSessionLengthLimiterForTesting()
      ->SetDelegateForTesting(std::move(delegate_ptr));

  // Ensure the SessionLengthLimit is updated.
  LocalStateValueWaiter(prefs::kSessionLengthLimit,
                        base::Value(kThreeHoursInMs))
      .Wait();

  // The session is not terminated.
  EXPECT_FALSE(observer.WasAppTerminated());
  EXPECT_FALSE(delegate->session_stopped());

  // Advance the clock by 3 hours.
  delegate->AdvanceClock(base::Hours(3));

  // Update the SessionLengthLimit policy to limit the session by two hours.
  // The session is expected to be terminated asap, because the current time is
  // later than the max session length.
  SetSessionLengthLimitPolicy(kTwoHoursInMs);

  // Fetch the policy update.
  {
    DeviceLocalAccountPolicyBroker* broker =
        GetDeviceLocalAccountPolicyBroker(account_id_1_);
    ASSERT_TRUE(broker);
    broker->core()->client()->FetchPolicy(PolicyFetchReason::kTest);
  }
  // Ensure the SessionLengthLimit is updated.
  LocalStateValueWaiter(prefs::kSessionLengthLimit, base::Value(kTwoHoursInMs))
      .Wait();

  // The session is terminated.
  EXPECT_TRUE(observer.WasAppTerminated());
  EXPECT_TRUE(delegate->session_stopped());
}

struct FeaturesTestParam {
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
};
class DeviceLocalAccountPolicyFetchSha256Test
    : public DeviceLocalAccountTest,
      public ::testing::WithParamInterface<FeaturesTestParam> {
 public:
  DeviceLocalAccountPolicyFetchSha256Test() {
    const FeaturesTestParam& features_test_param = GetParam();
    scoped_feature_list_.InitWithFeatures(
        features_test_param.enabled_features,
        features_test_param.disabled_features);
  }
  ~DeviceLocalAccountPolicyFetchSha256Test() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(DeviceLocalAccountPolicyFetchSha256Test,
                       PolicyForExtensions) {
  // Set up a test update server for the Show Managed Storage app.
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  scoped_refptr<TestingUpdateManifestProvider> testing_update_manifest_provider(
      new TestingUpdateManifestProvider(kRelativeUpdateURL));
  testing_update_manifest_provider->AddUpdate(
      kShowManagedStorageID, kShowManagedStorageVersion,
      embedded_test_server()->GetURL(std::string("/") +
                                     kShowManagedStorageCRXPath));
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&TestingUpdateManifestProvider::HandleRequest,
                          testing_update_manifest_provider));
  embedded_test_server()->StartAcceptingConnections();

  // Force-install the Show Managed Storage app. This app can be installed in
  // public sessions because it's allowlisted for testing purposes.
  em::StringList* forcelist = device_local_account_policy_.payload()
                                  .mutable_extensioninstallforcelist()
                                  ->mutable_value();
  forcelist->add_entries(base::StringPrintf(
      "%s;%s", kShowManagedStorageID,
      embedded_test_server()->GetURL(kRelativeUpdateURL).spec().c_str()));

  // Set a policy for the app at the policy testserver.
  // Note that the policy for the device-local account will be fetched before
  // the session is started, so the policy for the app must be installed before
  // the first device policy fetch.
  policy_test_server_mixin_.UpdateExternalPolicy(
      dm_protocol::kChromeExtensionPolicyType, kShowManagedStorageID,
      "{"
      "  \"string\": {"
      "    \"Value\": \"policy test value one\""
      "  }"
      "}");

  // Install and refresh the device policy now. This will also fetch the initial
  // user policy for the device-local account now.
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);
  WaitForPolicy();

  // Observe the app installation after login.
  ExtensionInstallObserver install_observer(kShowManagedStorageID);
  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));
  WaitForSessionStart();
  install_observer.Wait();

  // Verify that the app was installed.
  Profile* profile = GetProfileForTest();
  ASSERT_TRUE(profile);
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  EXPECT_TRUE(
      extension_registry->enabled_extensions().GetByID(kShowManagedStorageID));

  // Wait for the app policy if it hasn't been fetched yet.
  ProfilePolicyConnector* connector = profile->GetProfilePolicyConnector();
  ASSERT_TRUE(connector);
  PolicyService* policy_service = connector->policy_service();
  ASSERT_TRUE(policy_service);
  const PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, kShowManagedStorageID);
  if (policy_service->GetPolicies(ns).Get("string") == nullptr) {
    RefreshAndWaitForPolicies(policy_service, ns);
  }

  // Verify that the app policy was set.
  base::Value expected_value("policy test value one");
  EXPECT_EQ(expected_value, *policy_service->GetPolicies(ns).GetValue(
                                "string", base::Value::Type::STRING));

  // Now update the policy at the server.
  policy_test_server_mixin_.UpdateExternalPolicy(
      dm_protocol::kChromeExtensionPolicyType, kShowManagedStorageID,
      "{"
      "  \"string\": {"
      "    \"Value\": \"policy test value two\""
      "  }"
      "}");

  // And issue a policy refresh.
  const base::Value* new_value = RefreshAndWaitForPolicies(policy_service, ns);

  // Verify that the app policy was updated.
  base::Value expected_new_value("policy test value two");
  EXPECT_EQ(expected_new_value, *new_value);
}

INSTANTIATE_TEST_SUITE_P(
    DeviceLocalAccountPolicyFetchSha256Test,
    DeviceLocalAccountPolicyFetchSha256Test,
    ::testing::Values(
        FeaturesTestParam{.enabled_features = {policy::kPolicyFetchWithSha256}},
        FeaturesTestParam{
            .disabled_features = {policy::kPolicyFetchWithSha256}}));

class DeviceLocalAccountWarnings : public DeviceLocalAccountTest {
  void SetUpInProcessBrowserTestFixture() override {
    DeviceLocalAccountTest::SetUpInProcessBrowserTestFixture();
    SetManagedSessionsWarningDisabled();
  }

  void SetManagedSessionsWarningDisabled() {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    em::ManagedGuestSessionPrivacyWarningsProto* managed_sessions_warnings =
        proto.mutable_managed_guest_session_privacy_warnings();
    managed_sessions_warnings->set_enabled(false);
    RefreshDevicePolicy();
    policy_test_server_mixin_.UpdateDevicePolicy(proto);
  }
};

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountWarnings, NoLoginWarningShown) {
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  ExpandPublicSessionPod(false);

  // Click the link that switches the pod to its advanced form. Verify that the
  // pod switches from basic to advanced.
  ash::LoginScreenTestApi::ClickPublicExpandedAdvancedViewButton();
  ASSERT_TRUE(ash::LoginScreenTestApi::IsExpandedPublicSessionAdvanced());
  ASSERT_FALSE(ash::LoginScreenTestApi::IsPublicSessionWarningShown());
}

class ManagedSessionsTest : public DeviceLocalAccountTest {
 protected:
  class CertsObserver : public ash::PolicyCertificateProvider::Observer {
   public:
    explicit CertsObserver(base::OnceClosure on_change)
        : on_change_(std::move(on_change)) {}

    void OnPolicyProvidedCertsChanged() override {
      std::move(on_change_).Run();
    }

   private:
    base::OnceClosure on_change_;
  };

  void StartTestExtensionsServer() {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    scoped_refptr<TestingUpdateManifestProvider>
        testing_update_manifest_provider(
            new TestingUpdateManifestProvider(kRelativeUpdateURL));
    testing_update_manifest_provider->AddUpdate(
        kGoodExtensionID, kGoodExtensionVersion,
        embedded_test_server()->GetURL(std::string("/") +
                                       kGoodExtensionCRXPath));
    testing_update_manifest_provider->AddUpdate(
        kHostedAppID, kHostedAppVersion,
        embedded_test_server()->GetURL(std::string("/") + kHostedAppCRXPath));
    testing_update_manifest_provider->AddUpdate(
        kShowManagedStorageID, kShowManagedStorageVersion,
        embedded_test_server()->GetURL(std::string("/") +
                                       kShowManagedStorageCRXPath));
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&TestingUpdateManifestProvider::HandleRequest,
                            testing_update_manifest_provider));
    embedded_test_server()->StartAcceptingConnections();
  }

  void AddExtension(const char* extension_id) {
    // Specify policy to install an extension.
    em::StringList* forcelist = device_local_account_policy_.payload()
                                    .mutable_extensioninstallforcelist()
                                    ->mutable_value();
    forcelist->add_entries(base::StringPrintf(
        "%s;%s", extension_id,
        embedded_test_server()->GetURL(kRelativeUpdateURL).spec().c_str()));
  }

  void AddForceInstalledSafeExtension() { AddExtension(kHostedAppID); }

  void AddForceInstalledUnsafeExtension() {
    // has effective hosts:
    // http://*.example.com/*
    // http://*.google.com/*
    // https://*.google.com/*
    AddExtension(kGoodExtensionID);
  }

  void AddForceInstalledAllowlistedExtension() {
    AddExtension(kShowManagedStorageID);
  }

  void WaitForCertificateUpdate() {
    DeviceNetworkConfigurationUpdaterAsh* updater =
        g_browser_process->platform_part()
            ->browser_policy_connector_ash()
            ->GetDeviceNetworkConfigurationUpdater();
    base::RunLoop run_loop;
    auto observer = std::make_unique<CertsObserver>(run_loop.QuitClosure());
    updater->AddPolicyProvidedCertsObserver(observer.get());
    run_loop.Run();
    updater->RemovePolicyProvidedCertsObserver(observer.get());
  }

  void AddNetworkCertificateToDevicePolicy() {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_open_network_configuration()->set_open_network_configuration(
        kFakeOncWithCertificate);
    RefreshDevicePolicy();
    WaitForCertificateUpdate();
  }
};

IN_PROC_BROWSER_TEST_F(ManagedSessionsTest, ManagedSessionsEnabledNonRisky) {
  // Install and refresh the device policy now. This will also fetch the initial
  // user policy for the device-local account now.
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);
  WaitForPolicy();

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id_1_);
  ASSERT_TRUE(user);

  // Management disclosure warning is shown in the beginning, because
  // kManagedSessionUseFullLoginWarning pref is set to true in the beginning.
  EXPECT_TRUE(IsFullManagementDisclosureNeeded(account_id_1_));

  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));
  WaitForSessionStart();

  // After the login, kManagedSessionUseFullLoginWarning pref is updated.
  // Check that management disclosure warning is not shown when managed sessions
  // are enabled, but policy settings are not risky.
  ASSERT_FALSE(IsFullManagementDisclosureNeeded(account_id_1_));
}

IN_PROC_BROWSER_TEST_F(ManagedSessionsTest, ForceInstalledSafeExtension) {
  StartTestExtensionsServer();
  AddForceInstalledSafeExtension();

  // Install and refresh the device policy now. This will also fetch the initial
  // user policy for the device-local account now.
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);
  WaitForPolicy();

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id_1_);
  ASSERT_TRUE(user);

  // Management disclosure warning is shown in the beginning, because
  // kManagedSessionUseFullLoginWarning pref is set to true in the beginning.
  ASSERT_TRUE(IsFullManagementDisclosureNeeded(account_id_1_));

  ExtensionInstallObserver install_observer(kHostedAppID);

  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));
  WaitForSessionStart();

  install_observer.Wait();

  // After the login, kManagedSessionUseFullLoginWarning pref is updated.
  // Check that force-installed extension activates managed session mode for
  // device-local users.
  EXPECT_FALSE(IsFullManagementDisclosureNeeded(account_id_1_));
}

IN_PROC_BROWSER_TEST_F(ManagedSessionsTest, ForceInstalledUnsafeExtension) {
  StartTestExtensionsServer();
  AddForceInstalledUnsafeExtension();

  // Install and refresh the device policy now. This will also fetch the initial
  // user policy for the device-local account now.
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);
  WaitForPolicy();

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id_1_);
  ASSERT_TRUE(user);

  // Management disclosure warning is shown in the beginning, because
  // kManagedSessionUseFullLoginWarning pref is set to true in the beginning.
  ASSERT_TRUE(IsFullManagementDisclosureNeeded(account_id_1_));

  ExtensionInstallObserver install_observer(kGoodExtensionID);

  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));
  WaitForSessionStart();

  install_observer.Wait();

  // After the login, kManagedSessionUseFullLoginWarning pref is updated.
  // Check that force-installed extension activates managed session mode for
  // device-local users.
  EXPECT_TRUE(IsFullManagementDisclosureNeeded(account_id_1_));
}

IN_PROC_BROWSER_TEST_F(ManagedSessionsTest, AllowlistedExtension) {
  StartTestExtensionsServer();
  AddForceInstalledAllowlistedExtension();

  // Install and refresh the device policy now. This will also fetch the initial
  // user policy for the device-local account now.
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);
  WaitForPolicy();

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id_1_);
  ASSERT_TRUE(user);

  // Management disclosure warning is shown in the beginning, because
  // kManagedSessionUseFullLoginWarning pref is set to true in the beginning.
  ASSERT_TRUE(IsFullManagementDisclosureNeeded(account_id_1_));

  ExtensionInstallObserver install_observer(kShowManagedStorageID);

  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));
  WaitForSessionStart();

  install_observer.Wait();

  // After the login, kManagedSessionUseFullLoginWarning pref is updated.
  // Check that white-listed extension is not considered risky and doesn't
  // activate managed session mode.
  EXPECT_FALSE(IsFullManagementDisclosureNeeded(account_id_1_));
}

IN_PROC_BROWSER_TEST_F(ManagedSessionsTest, NetworkCertificate) {
  device_local_account_policy_.payload()
      .mutable_opennetworkconfiguration()
      ->set_value(kFakeOncWithCertificate);

  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id_1_);
  ASSERT_TRUE(user);

  // Check that network certificate pushed via policy activates managed sessions
  // mode.
  EXPECT_TRUE(IsFullManagementDisclosureNeeded(account_id_1_));
}

IN_PROC_BROWSER_TEST_F(ManagedSessionsTest, AllowCrossOriginAuthPrompt) {
  device_local_account_policy_.payload()
      .mutable_allowcrossoriginauthprompt()
      ->set_value(true);

  // Install and refresh the device policy now. This will also fetch the initial
  // user policy for the device-local account now.
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);
  WaitForPolicy();

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id_1_);
  ASSERT_TRUE(user);

  // Check that setting a value to 'AllowCrossOriginAuthPrompt' activates
  // managed sessions mode.
  ASSERT_TRUE(IsFullManagementDisclosureNeeded(account_id_1_));
}

class TermsOfServiceDownloadTest : public DeviceLocalAccountTest,
                                   public testing::WithParamInterface<bool> {
 public:
  void SetUpOnMainThread() override {
    DeviceLocalAccountTest::SetUpOnMainThread();

    // Prevent browser start in user session so that we do not need to wait
    // for its initialization.
    ash::test::UserSessionManagerTestApi(ash::UserSessionManager::GetInstance())
        .SetShouldLaunchBrowserInTests(false);
  }

  bool UseValidURL() const { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(TermsOfServiceDownloadTest, TermsOfServiceScreen) {
  // Specify Terms of Service URL.
  ASSERT_TRUE(embedded_test_server()->Start());
  device_local_account_policy_.payload().mutable_termsofserviceurl()->set_value(
      embedded_test_server()
          ->GetURL(std::string("/") + (UseValidURL()
                                           ? kExistentTermsOfServicePath
                                           : kNonexistentTermsOfServicePath))
          .spec());
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  ash::test::ProfilePreparedWaiter profile_prepared(account_id_1_);
  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));
  profile_prepared.Wait();

  // Verify that the Terms of Service screen is being shown.
  ash::OobeScreenWaiter(ash::TermsOfServiceScreenView::kScreenId).Wait();

  // Wait for the Terms of Service to finish loading.

  if (!UseValidURL()) {
    // The Terms of Service URL was invalid. Verify that the screen is showing
    // an error and the accept button is disabled.
    ash::test::OobeJS()
        .CreateVisibilityWaiter(true, {"terms-of-service", "step-error"})
        ->Wait();

    ash::test::OobeJS().ExpectDisabledPath(
        {"terms-of-service", "acceptButton"});
    return;
  }

  ash::test::OobeJS()
      .CreateVisibilityWaiter(true, {"terms-of-service", "step-loaded"})
      ->Wait();

  ash::test::OobeJS()
      .CreateVisibilityWaiter(true, {"terms-of-service", "termsOfServiceFrame"})
      ->Wait();

  // Get the Terms Of Service from the webview.
  const std::string content = ash::test::GetWebViewContents(
      {"terms-of-service", "termsOfServiceFrame"});

  // Get the expected values for heading and subheading.
  const std::string expected_heading =
      l10n_util::GetStringFUTF8(IDS_TERMS_OF_SERVICE_SCREEN_HEADING, kDomain);
  const std::string expected_subheading = l10n_util::GetStringFUTF8(
      IDS_TERMS_OF_SERVICE_SCREEN_SUBHEADING, kDomain);

  // Compare heading and subheading
  ash::test::OobeJS().ExpectEQ(
      GetOobeElementPath({"terms-of-service", "tosHeading"}) + ".textContent",
      expected_heading);
  ash::test::OobeJS().ExpectEQ(
      GetOobeElementPath({"terms-of-service", "tosSubheading"}) +
          ".textContent",
      expected_subheading);

  // The Terms of Service URL was valid. Verify that the screen is showing the
  // downloaded Terms of Service and the accept button is enabled.
  base::FilePath test_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  std::string terms_of_service;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(
        test_dir.Append(kExistentTermsOfServicePath), &terms_of_service));
  }
  EXPECT_EQ(terms_of_service, content);

  ash::test::OobeJS()
      .CreateVisibilityWaiter(false, {"terms-of-service", "step-error"})
      ->Wait();

  ash::test::OobeJS().ExpectEnabledPath({"terms-of-service", "acceptButton"});

  // Click the accept button.
  ash::test::OobeJS().ClickOnPath({"terms-of-service", "acceptButton"});

  WaitForSessionStart();
}

IN_PROC_BROWSER_TEST_P(TermsOfServiceDownloadTest, DeclineTermsOfService) {
  // Specify Terms of Service URL.
  ASSERT_TRUE(embedded_test_server()->Start());
  device_local_account_policy_.payload().mutable_termsofserviceurl()->set_value(
      embedded_test_server()
          ->GetURL(std::string("/") + (UseValidURL()
                                           ? kExistentTermsOfServicePath
                                           : kNonexistentTermsOfServicePath))
          .spec());
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  ash::test::ProfilePreparedWaiter profile_prepared(account_id_1_);
  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));
  profile_prepared.Wait();

  // Verify that the Terms of Service screen is being shown.
  ash::OobeScreenWaiter(ash::TermsOfServiceScreenView::kScreenId).Wait();

  if (!UseValidURL()) {
    ash::test::OobeJS()
        .CreateVisibilityWaiter(true, {"terms-of-service", "step-error"})
        ->Wait();
    ash::test::TapOnPathAndWaitForOobeToBeDestroyed(
        {"terms-of-service", "errorBackButton"});
  } else {
    ash::test::OobeJS()
        .CreateVisibilityWaiter(true, {"terms-of-service", "step-loaded"})
        ->Wait();
    ash::test::TapOnPathAndWaitForOobeToBeDestroyed(
        {"terms-of-service", "backButton"});
  }
  EXPECT_TRUE(session_manager_client()->session_stopped());
}

INSTANTIATE_TEST_SUITE_P(TermsOfServiceDownloadTestInstance,
                         TermsOfServiceDownloadTest,
                         testing::Bool());

// Tests that display prefs are updated in MGS when enabled by
// DeviceAllowMGSToStoreDisplayProperties policy.
class MgsDisplayPrefsTest : public DeviceLocalAccountTest,
                            public testing::WithParamInterface<bool> {
 protected:
  void SetUpOnMainThread() override {
    DeviceLocalAccountTest::SetUpOnMainThread();
    local_state_ = g_browser_process->local_state();
    ASSERT_TRUE(local_state_);
  }

  void TearDownOnMainThread() override {
    DeviceLocalAccountTest::TearDownOnMainThread();
    local_state_ = nullptr;
  }

  void SetUpAndWaitForSessionStart() {
    UploadAndInstallDeviceLocalAccountPolicy();
    AddPublicSessionToDevicePolicy(kAccountId1);
    WaitForPolicy();

    ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));
    WaitForSessionStart();
  }

  void SetAllowMgsToStoreDisplayProperties(bool allowed) {
    local_state_->SetBoolean(ash::prefs::kAllowMGSToStoreDisplayProperties,
                             allowed);
  }

  bool IsMgsAllowedToStoreDisplayProperties() { return GetParam(); }

  const base::Value::Dict* GetDisplayProperties() {
    const base::Value::Dict& display_properties =
        local_state_->GetDict(ash::prefs::kDisplayProperties);
    return display_properties.FindDict(
        base::NumberToString(GetPrimaryDisplay().id()));
  }

  void UpdateDisplayProperties(base::Value::Dict properties) {
    ScopedDictPrefUpdate update(local_state_, ash::prefs::kDisplayProperties);
    update->Set(base::NumberToString(GetPrimaryDisplay().id()),
                std::move(properties));
  }

  static void UpdateDisplay(const std::string& display_specs) {
    display::test::DisplayManagerTestApi(GetDisplayManager())
        .UpdateDisplay(display_specs);
    ash::ScreenOrientationControllerTestApi(
        ash::Shell::Get()->screen_orientation_controller())
        .UpdateNaturalOrientation();
  }

  static const display::Display& GetPrimaryDisplay() {
    return GetDisplayManager()->GetPrimaryDisplayCandidate();
  }

  static const display::ManagedDisplayMode GetManagedDisplayMode() {
    display::ManagedDisplayMode display_mode;
    GetDisplayManager()->GetSelectedModeForDisplayId(GetPrimaryDisplay().id(),
                                                     &display_mode);
    return display_mode;
  }

  static ash::DisplayPrefs* GetDisplayPrefs() {
    return ash::Shell::Get()->display_prefs();
  }

  static display::DisplayManager* GetDisplayManager() {
    return ash::Shell::Get()->display_manager();
  }

 private:
  raw_ptr<PrefService> local_state_;
};

IN_PROC_BROWSER_TEST_P(MgsDisplayPrefsTest,
                       PRE_DisplayPropertiesPersistWhenEnabledByPolicy) {
  // Set initial values for the display which will be loaded from `local_state`
  // by `display_prefs`.
  SetAllowMgsToStoreDisplayProperties(true);
  // This adds one display with maximum resolution 1960x1000 and two display
  // modes with resolution 1960x1000 and 1000x600.
  UpdateDisplay("1960x1000#1960x1000*1|1000x600*2");
  UpdateDisplayProperties(
      base::Value::Dict()
          .Set("rotation", display::Display::Rotation::ROTATE_0)
          .Set("width", 1960)
          .Set("height", 1000));
  GetDisplayPrefs()->LoadDisplayPrefsForTest();

  SetUpAndWaitForSessionStart();

  // Verify initial display pref values.
  EXPECT_EQ(display::Display::Rotation::ROTATE_0,
            GetPrimaryDisplay().rotation());
  EXPECT_EQ(GetManagedDisplayMode().size(), gfx::Size(1960, 1000));
  EXPECT_EQ(GetManagedDisplayMode().device_scale_factor(), 1.0f);

  SetAllowMgsToStoreDisplayProperties(IsMgsAllowedToStoreDisplayProperties());

  EXPECT_TRUE(display::test::SetDisplayResolution(
      GetDisplayManager(), GetPrimaryDisplay().id(), gfx::Size(1000, 600)));
  GetDisplayManager()->SetDisplayRotation(
      GetPrimaryDisplay().id(), display::Display::Rotation::ROTATE_270,
      display::Display::RotationSource::USER);
}

IN_PROC_BROWSER_TEST_P(MgsDisplayPrefsTest,
                       DisplayPropertiesPersistWhenEnabledByPolicy) {
  base::AddFeatureIdTagToTestResult(DeviceLocalAccountTest::kDisplayPrefsTag);

  GetDisplayPrefs()->LoadDisplayPrefsForTest();

  if (IsMgsAllowedToStoreDisplayProperties()) {
    EXPECT_EQ(GetPrimaryDisplay().rotation(),
              display::Display::Rotation::ROTATE_270);
    EXPECT_EQ(GetManagedDisplayMode().size(), gfx::Size(1000, 600));
    EXPECT_EQ(GetManagedDisplayMode().device_scale_factor(), 2.0f);
  } else {
    EXPECT_EQ(GetPrimaryDisplay().rotation(),
              display::Display::Rotation::ROTATE_0);
    EXPECT_EQ(GetManagedDisplayMode().size(), gfx::Size(1960, 1000));
    EXPECT_EQ(GetManagedDisplayMode().device_scale_factor(), 1.0f);
  }
}

INSTANTIATE_TEST_SUITE_P(MgsDisplayPrefsTestInstance,
                         MgsDisplayPrefsTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, WebAppsInPublicSession) {
  UploadAndInstallDeviceLocalAccountPolicy();
  // Add an account with DeviceLocalAccountType::kPublicSession.
  AddPublicSessionToDevicePolicy(kAccountId1);
  WaitForPolicy();

  StartLogin(std::string(), std::string());
  WaitForSessionStart();

  // WebAppProvider should be enabled for kPublicSession user account.
  Profile* profile = GetProfileForTest();
  ASSERT_TRUE(profile);
  EXPECT_TRUE(web_app::WebAppProvider::GetForTest(profile));
}

// TODO(b/307518336): move UKM tests to
// chrome/browser/metrics/ukm_browsertest.cc.
class DeviceLocalAccountUkmTest : public DeviceLocalAccountTest {
 public:
  void SetChromeMetricsEnabled(bool value) {
    chrome_metrics_enabled_ = value;
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &chrome_metrics_enabled_);
  }

 private:
  bool chrome_metrics_enabled_;
};

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountUkmTest, PRE_ReportUkmOnShutdown) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  SetChromeMetricsEnabled(true);

  // Setup managed guest session.
  AddPublicSessionToDevicePolicy(kAccountId1);
  UploadAndInstallDeviceLocalAccountPolicy();
  WaitForPolicy();
  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));
  WaitForSessionStart();
  ASSERT_TRUE(chromeos::IsManagedGuestSession());

  EnableUrlKeyedAnonymizedDataCollection(GetProfileForTest());
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());

  // A browser is opened by default in MGS.
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Delete all UKM to check metrics reported during the shutdown.
  ukm_test_helper.PurgeData();
  EXPECT_FALSE(ukm_test_helper.HasUnsentLogs());
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountUkmTest, ReportUkmOnShutdown) {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  SetChromeMetricsEnabled(true);

  // Check metrics from the previous managed guest session.
  ASSERT_TRUE(ukm_test_helper.HasUnsentLogs());
  std::unique_ptr<ukm::Report> report = ukm_test_helper.GetUkmReport();
  ASSERT_EQ(1, report->sources_size());
  EXPECT_EQ(ukm::SourceType::APP_ID, report->sources().Get(0).type());
}

class AmbientAuthenticationManagedGuestSessionTest
    : public DeviceLocalAccountTest,
      public testing::WithParamInterface<net::AmbientAuthAllowedProfileTypes> {
 public:
  void SetAmbientAuthPolicy(net::AmbientAuthAllowedProfileTypes value) {
    device_local_account_policy_.payload()
        .mutable_ambientauthenticationinprivatemodesenabled()
        ->set_value(static_cast<int>(value));
    UploadDeviceLocalAccountPolicy();
  }

  void IsAmbientAuthAllowedForProfilesTest() {
    int policy_value = device_local_account_policy_.payload()
                           .ambientauthenticationinprivatemodesenabled()
                           .value();
    Profile* regular_profile = GetCurrentBrowser()->profile();
    Profile* incognito_profile =
        regular_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);

    EXPECT_TRUE(AmbientAuthenticationTestHelper::IsAmbientAuthAllowedForProfile(
        regular_profile));
    EXPECT_EQ(AmbientAuthenticationTestHelper::IsAmbientAuthAllowedForProfile(
                  incognito_profile),
              AmbientAuthenticationTestHelper::IsIncognitoAllowedInPolicy(
                  policy_value));
  }

  Browser* GetCurrentBrowser() {
    BrowserList* browser_list = BrowserList::GetInstance();
    EXPECT_EQ(1U, browser_list->size());
    Browser* browser = browser_list->get(0);
    DCHECK(browser);
    return browser;
  }
};

IN_PROC_BROWSER_TEST_P(AmbientAuthenticationManagedGuestSessionTest,
                       AmbientAuthenticationInPrivateModesEnabledPolicy) {
  SetAmbientAuthPolicy(GetParam());

  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);
  EnableAutoLogin();

  WaitForPolicy();

  WaitForSessionStart();

  CheckPublicSessionPresent(account_id_1_);

  IsAmbientAuthAllowedForProfilesTest();
}

INSTANTIATE_TEST_SUITE_P(
    AmbientAuthAllPolicyValuesTest,
    AmbientAuthenticationManagedGuestSessionTest,
    testing::Values(net::AmbientAuthAllowedProfileTypes::kRegularOnly,
                    net::AmbientAuthAllowedProfileTypes::kIncognitoAndRegular,
                    net::AmbientAuthAllowedProfileTypes::kGuestAndRegular,
                    net::AmbientAuthAllowedProfileTypes::kAll));

}  // namespace policy

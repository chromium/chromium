// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/session/logout_confirmation_controller.h"
#include "ash/system/session/logout_confirmation_dialog.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/extensions/device_local_account_external_policy_loader.h"
#include "chrome/browser/chromeos/extensions/external_cache.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/terms_of_service_screen.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/session/user_session_manager_test_api.h"
#include "chrome/browser/chromeos/login/signin_specifics.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"
#include "chrome/browser/chromeos/login/test/webview_content_extractor.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager_test_util.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base_test_util.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/policy/device_network_configuration_updater.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/updater/chromeos_extension_cache_delegate.h"
#include "chrome/browser/extensions/updater/extension_cache_impl.h"
#include "chrome/browser/extensions/updater/local_extension_cache.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/terms_of_service_screen_handler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_paths.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/login/auth/mock_auth_status_consumer.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/network/policy_certificate_provider.h"
#include "chromeos/settings/timezone_settings.h"
#include "components/crx_file/crx_verifier.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "crypto/rsa_private_key.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace em = enterprise_management;

using chromeos::LoginScreenContext;
using chromeos::test::GetOobeElementPath;
using testing::_;
using testing::InvokeWithoutArgs;
using testing::Return;

namespace policy {

namespace {

const char kDomain[] = "example.com";
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
const char kUpdateManifestHeader[] =
    "<?xml version='1.0' encoding='UTF-8'?>\n"
    "<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>\n";
const char kUpdateManifestTemplate[] =
    "  <app appid='%s'>\n"
    "    <updatecheck codebase='%s' version='%s' />\n"
    "  </app>\n";
const char kUpdateManifestFooter[] =
    "</gupdate>\n";
const char kHostedAppID[] = "kbmnembihfiondgfjekmnmcbddelicoi";
const char kHostedAppCRXPath[] = "extensions/hosted_app.crx";
const char kHostedAppVersion[] = "1.0.0.0";
const char kGoodExtensionID[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char kGoodExtensionCRXPath[] = "extensions/good.crx";
const char kGoodExtensionVersion[] = "1.0";
// Chrome RDP extension
const char kPublicSessionWhitelistedExtensionID[] =
    "cbkkbcmdlboombapidmoeolnmdacpkch";
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

  ~TestingUpdateManifestProvider();
  friend class RefCountedThreadSafe<TestingUpdateManifestProvider>;

  // Protects other members against concurrent access from main thread and
  // test server io thread.
  base::Lock lock_;

  std::string relative_update_url_;
  UpdateMap updates_;

  DISALLOW_COPY_AND_ASSIGN(TestingUpdateManifestProvider);
};

// Helper base class used for waiting for a pref value stored in local state
// to change to a particular expected value.
class PrefValueWaiter {
 public:
  PrefValueWaiter(const std::string& pref, base::Value expected_value)
      : pref_(pref), expected_value_(std::move(expected_value)) {
    pref_change_registrar_.Init(g_browser_process->local_state());
  }

  PrefValueWaiter(const PrefValueWaiter&) = delete;
  PrefValueWaiter& operator=(const PrefValueWaiter&) = delete;

  virtual bool ExpectedValueFound();
  virtual ~PrefValueWaiter() = default;

  void Wait();

 protected:
  const std::string pref_;
  const base::Value expected_value_;

  PrefChangeRegistrar pref_change_registrar_;

 private:
  base::RunLoop run_loop_;
  void QuitLoopIfExpectedValueFound();
};

bool PrefValueWaiter::ExpectedValueFound() {
  const base::Value* pref_value =
      pref_change_registrar_.prefs()->Get(pref_.c_str());
  if (!pref_value) {
    // Can't use ASSERT_* in non-void functions so this is the next best
    // thing.
    ADD_FAILURE() << "Pref " << pref_ << " not found";
    return true;
  }
  return *pref_value == expected_value_;
}

void PrefValueWaiter::QuitLoopIfExpectedValueFound() {
  if (ExpectedValueFound())
    run_loop_.Quit();
}

void PrefValueWaiter::Wait() {
  pref_change_registrar_.Add(
      pref_.c_str(), base::Bind(&PrefValueWaiter::QuitLoopIfExpectedValueFound,
                                base::Unretained(this)));
  // Necessary if the pref value changes before the run loop is run. It is
  // safe to call RunLoop::Quit before RunLoop::Run (in which case the call
  // to Run will do nothing).
  QuitLoopIfExpectedValueFound();
  run_loop_.Run();
}

class DictionaryPrefValueWaiter : public PrefValueWaiter {
 public:
  DictionaryPrefValueWaiter(const std::string& pref,
                            const std::string& expected_value,
                            const std::string& key)
      : PrefValueWaiter(pref, base::Value(expected_value)), key_(key) {}

  DictionaryPrefValueWaiter(const DictionaryPrefValueWaiter&) = delete;
  DictionaryPrefValueWaiter& operator=(const DictionaryPrefValueWaiter&) =
      delete;
  ~DictionaryPrefValueWaiter() override = default;

 private:
  bool ExpectedValueFound() override;

  const std::string key_;
};

bool DictionaryPrefValueWaiter::ExpectedValueFound() {
  const base::DictionaryValue* pref =
      pref_change_registrar_.prefs()->GetDictionary(pref_.c_str());
  if (!pref) {
    // Can't use ASSERT_* in non-void functions so this is the next best
    // thing.
    ADD_FAILURE() << "Pref " << pref_ << " not found";
    return true;
  }
  std::string actual_value;
  return (pref->GetStringWithoutPathExpansion(key_, &actual_value) &&
          actual_value == expected_value_.GetString());
}

TestingUpdateManifestProvider::Update::Update(const std::string& version,
                                              const GURL& crx_url)
    : version(version),
      crx_url(crx_url) {
}

TestingUpdateManifestProvider::Update::Update() {
}

TestingUpdateManifestProvider::TestingUpdateManifestProvider(
    const std::string& relative_update_url)
    : relative_update_url_(relative_update_url) {
}

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
  if (url.path() != relative_update_url_)
    return std::unique_ptr<net::test_server::HttpResponse>();

  std::string content = kUpdateManifestHeader;
  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() != "x")
      continue;
    // Extract the extension id from the subquery. Since GetValueForKeyInQuery()
    // expects a complete URL, dummy scheme and host must be prepended.
    std::string id;
    net::GetValueForKeyInQuery(GURL("http://dummy?" + it.GetUnescapedValue()),
                               "id", &id);
    UpdateMap::const_iterator entry = updates_.find(id);
    if (entry != updates_.end()) {
      content += base::StringPrintf(kUpdateManifestTemplate,
                                    id.c_str(),
                                    entry->second.crx_url.spec().c_str(),
                                    entry->second.version.c_str());
    }
  }
  content += kUpdateManifestFooter;
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(content);
  http_response->set_content_type("text/xml");
  return std::move(http_response);
}

TestingUpdateManifestProvider::~TestingUpdateManifestProvider() {
}


bool DoesInstallFailureReferToId(const std::string& id,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  return content::Details<const extensions::CrxInstallError>(details)
             ->message()
             .find(base::UTF8ToUTF16(id)) != base::string16::npos;
}

bool IsSessionStarted() {
  return session_manager::SessionManager::Get()->IsSessionStarted();
}

void PolicyChangedCallback(const base::Closure& callback,
                           const base::Value* old_value,
                           const base::Value* new_value) {
  callback.Run();
}

}  // namespace

class DeviceLocalAccountTest : public DevicePolicyCrosBrowserTest,
                               public user_manager::UserManager::Observer,
                               public BrowserListObserver,
                               public extensions::AppWindowRegistry::Observer {
 protected:
  DeviceLocalAccountTest()
      : public_session_input_method_id_(
            base::StringPrintf(kPublicSessionInputMethodIDTemplate,
                               chromeos::extension_ime_util::kXkbExtensionId)),
        contents_(NULL),
        verifier_format_override_(crx_file::VerifierFormat::CRX3) {
    set_exit_when_last_browser_closes(false);
  }

  ~DeviceLocalAccountTest() override {}

  void SetUp() override {
    BrowserList::AddObserver(this);

    DevicePolicyCrosBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");

    // LoginDisplayHostMojo does not support dynamic device policy changes (see
    // https://crbug.com/956456).
    command_line->AppendSwitch(ash::switches::kShowWebUiLogin);
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

    initial_locale_ = g_browser_process->GetApplicationLocale();
    initial_language_ = l10n_util::GetLanguage(initial_locale_);

    content::WindowedNotificationObserver(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources()).Wait();

    chromeos::LoginDisplayHost* host =
        chromeos::LoginDisplayHost::default_host();
    contents_ = host->GetOobeWebContents();
    ASSERT_TRUE(contents_);

    // Wait for the login UI to be ready.
    chromeos::OobeUI* oobe_ui = host->GetOobeUI();
    ASSERT_TRUE(oobe_ui);
    base::RunLoop run_loop;
    const bool oobe_ui_ready = oobe_ui->IsJSReady(run_loop.QuitClosure());
    if (!oobe_ui_ready)
      run_loop.Run();

    // Skip to the login screen.
    chromeos::OobeScreenWaiter(chromeos::GaiaView::kScreenId).Wait();

    chromeos::test::UserSessionManagerTestApi session_manager_test_api(
        chromeos::UserSessionManager::GetInstance());
    session_manager_test_api.SetShouldObtainTokenHandleInTests(false);
  }

  void TearDownOnMainThread() override {
    BrowserList::RemoveObserver(this);

    // This shuts down the login UI.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&chrome::AttemptExit));
    base::RunLoop().RunUntilIdle();
    DevicePolicyCrosBrowserTest::TearDownOnMainThread();
  }

  // user_manager::UserManager::Observer:
  void LocalStateChanged(user_manager::UserManager* user_manager) override {
    if (local_state_changed_run_loop_)
      local_state_changed_run_loop_->Quit();
  }

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override {
    if (run_loop_)
      run_loop_->Quit();
  }

  // extensions::AppWindowRegistry::Observer:
  void OnAppWindowAdded(extensions::AppWindow* app_window) override {
    if (run_loop_)
      run_loop_->Quit();
  }

  void OnAppWindowRemoved(extensions::AppWindow* app_window) override {
    if (run_loop_)
      run_loop_->Quit();
  }

  void InitializePolicy() {
    device_policy()->policy_data().set_public_key_version(1);
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_show_user_names()->set_show_user_names(true);

    device_local_account_policy_.policy_data().set_policy_type(
        dm_protocol::kChromePublicAccountPolicyType);
    device_local_account_policy_.policy_data().set_username(kAccountId1);
    device_local_account_policy_.policy_data().set_settings_entity_id(
        kAccountId1);
    device_local_account_policy_.policy_data().set_public_key_version(1);
    device_local_account_policy_.payload().mutable_userdisplayname()->set_value(
        kDisplayName1);

    // Don't enable new managed sessions, use old public sessions.
    SetManagedSessionsEnabled(/* managed_sessions_enabled */ false);
  }

  void BuildDeviceLocalAccountPolicy() {
    device_local_account_policy_.SetDefaultSigningKey();
    device_local_account_policy_.Build();
  }

  void UploadDeviceLocalAccountPolicy() {
    BuildDeviceLocalAccountPolicy();
    ASSERT_TRUE(local_policy_mixin_.server()->UpdatePolicy(
        dm_protocol::kChromePublicAccountPolicyType, kAccountId1,
        device_local_account_policy_.payload().SerializeAsString()));
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

  void AddPublicSessionToDevicePolicy(const std::string& username) {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    em::DeviceLocalAccountInfoProto* account =
        proto.mutable_device_local_accounts()->add_account();
    account->set_account_id(username);
    account->set_type(
        em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);
    RefreshDevicePolicy();
    ASSERT_TRUE(local_policy_mixin_.UpdateDevicePolicy(proto));
  }

  void SetManagedSessionsEnabled(bool managed_sessions_enabled) {
    device_local_account_policy_.payload()
        .mutable_devicelocalaccountmanagedsessionenabled()
        ->set_value(managed_sessions_enabled);
    UploadDeviceLocalAccountPolicy();
  }

  void EnableAutoLogin() {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    em::DeviceLocalAccountsProto* device_local_accounts =
        proto.mutable_device_local_accounts();
    device_local_accounts->set_auto_login_id(kAccountId1);
    device_local_accounts->set_auto_login_delay(0);
    RefreshDevicePolicy();
    ASSERT_TRUE(local_policy_mixin_.UpdateDevicePolicy(proto));
  }

  void CheckPublicSessionPresent(const AccountId& account_id) {
    const user_manager::User* user =
        user_manager::UserManager::Get()->FindUser(account_id);
    ASSERT_TRUE(user) << " account " << account_id.GetUserEmail()
                      << " not found";
    EXPECT_EQ(account_id, user->GetAccountId());
    EXPECT_EQ(user_manager::USER_TYPE_PUBLIC_ACCOUNT, user->GetType());
  }

  void SetSystemTimezoneAutomaticDetectionPolicy(
      em::SystemTimezoneProto_AutomaticTimezoneDetectionType policy) {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_system_timezone()->set_timezone_detection_type(policy);
    RefreshDevicePolicy();

    PrefValueWaiter(prefs::kSystemTimezoneAutomaticDetectionPolicy,
                    base::Value(policy))
        .Wait();
    ASSERT_TRUE(local_policy_mixin_.UpdateDevicePolicy(proto));
  }

  base::FilePath GetExtensionCacheDirectoryForAccountID(
      const std::string& account_id) {
    base::FilePath extension_cache_root_dir;
    if (!base::PathService::Get(chromeos::DIR_DEVICE_LOCAL_ACCOUNT_EXTENSIONS,
                                &extension_cache_root_dir)) {
      ADD_FAILURE();
    }
    return extension_cache_root_dir.Append(
        base::HexEncode(account_id.c_str(), account_id.size()));
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
    DictionaryPrefValueWaiter("UserDisplayName", expected_display_name, user_id)
        .Wait();
  }

  void WaitForPolicy() {
    // Wait for the display name becoming available as that indicates
    // device-local account policy is fully loaded, which is a prerequisite for
    // successful login.
    WaitForDisplayName(account_id_1_.GetUserEmail(), kDisplayName1);
  }

  void ExpandPublicSessionPod(bool expect_advanced) {
    bool advanced = false;
    // Click on the pod to expand it.
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        contents_,
        base::StringPrintf(
            "var pod ="
            "    document.getElementById('pod-row').getPodWithUsername_('%s');"
            "pod.click();"
            "domAutomationController.send(pod.classList.contains('advanced'));",
            account_id_1_.Serialize().c_str()),
        &advanced));
    // Verify that the pod expanded to its basic/advanced form, as expected.
    EXPECT_EQ(expect_advanced, advanced);

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

  void StartLogin(const std::string& locale,
                  const std::string& input_method) {
    // Start login into the device-local account.
    chromeos::LoginDisplayHost* host =
        chromeos::LoginDisplayHost::default_host();
    ASSERT_TRUE(host);
    host->StartSignInScreen(LoginScreenContext());
    chromeos::ExistingUserController* controller =
        chromeos::ExistingUserController::current_controller();
    ASSERT_TRUE(controller);

    chromeos::UserContext user_context(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                                       account_id_1_);
    user_context.SetPublicSessionLocale(locale);
    user_context.SetPublicSessionInputMethod(input_method);
    controller->Login(user_context, chromeos::SigninSpecifics());
  }

  void WaitForSessionStart() {
    if (IsSessionStarted())
      return;
    chromeos::WizardController::SkipPostLoginScreensForTesting();
    chromeos::test::WaitForPrimaryUserSessionStart();
  }

  void WaitUntilLocalStateChanged() {
    local_state_changed_run_loop_ = std::make_unique<base::RunLoop>();
    user_manager::UserManager::Get()->AddObserver(this);
    local_state_changed_run_loop_->Run();
    user_manager::UserManager::Get()->RemoveObserver(this);
  }

  void VerifyKeyboardLayoutMatchesLocale() {
    chromeos::input_method::InputMethodManager* input_method_manager =
        chromeos::input_method::InputMethodManager::Get();
    std::vector<std::string> layouts_from_locale;
    input_method_manager->GetInputMethodUtil()->
        GetInputMethodIdsFromLanguageCode(
            g_browser_process->GetApplicationLocale(),
            chromeos::input_method::kKeyboardLayoutsOnly,
            &layouts_from_locale);
    ASSERT_FALSE(layouts_from_locale.empty());
    EXPECT_EQ(layouts_from_locale.front(),
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

  const AccountId account_id_1_ =
      AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
          kAccountId1,
          DeviceLocalAccount::TYPE_PUBLIC_SESSION));
  const AccountId account_id_2_ =
      AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
          kAccountId2,
          DeviceLocalAccount::TYPE_PUBLIC_SESSION));
  const std::string public_session_input_method_id_;

  std::string initial_locale_;
  std::string initial_language_;

  std::unique_ptr<base::RunLoop> run_loop_;

  // Specifically exists to assist with waiting for a LocalStateChanged()
  // invocation.
  std::unique_ptr<base::RunLoop> local_state_changed_run_loop_;

  UserPolicyBuilder device_local_account_policy_;
  chromeos::LocalPolicyTestServerMixin local_policy_mixin_{&mixin_host_};

  content::WebContents* contents_;

  // These are member variables so they're guaranteed that the destructors
  // (which may delete a directory) run in a scope where file IO is allowed.
  base::ScopedTempDir temp_dir_;
  base::ScopedTempDir cache_dir_;

 private:
  extensions::SandboxedUnpacker::ScopedVerifierFormatOverrideForTest
      verifier_format_override_;
  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountTest);
};

static bool IsKnownUser(const AccountId& account_id) {
  return user_manager::UserManager::Get()->IsKnownUser(account_id);
}

// Helper that listen extension installation when new profile is created.
class ExtensionInstallObserver : public content::NotificationObserver,
                                 public extensions::ExtensionRegistryObserver {
 public:
  explicit ExtensionInstallObserver(const std::string& extension_id)
      : registry_(nullptr),
        waiting_extension_id_(extension_id),
        observed_(false) {
    registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
                   content::NotificationService::AllSources());
  }

  ~ExtensionInstallObserver() override {
    if (registry_ != nullptr)
      registry_->RemoveObserver(this);
  }

  // Wait until an extension with |extension_id| is installed.
  void Wait() {
    if (!observed_)
      run_loop_.Run();
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

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK_EQ(chrome::NOTIFICATION_PROFILE_CREATED, type);

    Profile* profile = content::Source<Profile>(source).ptr();
    // Ignore lock screen apps profile.
    if (chromeos::ProfileHelper::IsLockScreenAppProfile(profile))
      return;
    registry_ = extensions::ExtensionRegistry::Get(profile);

    // Check if extension is already installed with newly created profile.
    if (registry_->GetInstalledExtension(waiting_extension_id_)) {
      observed_ = true;
      run_loop_.Quit();
      return;
    }

    // Start listening for extension installation.
    registry_->AddObserver(this);
  }

  extensions::ExtensionRegistry* registry_;
  base::RunLoop run_loop_;
  content::NotificationRegistrar registrar_;
  std::string waiting_extension_id_;
  bool observed_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallObserver);
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
  DictionaryPrefUpdate given_name_update(g_browser_process->local_state(),
                                         "UserGivenName");
  given_name_update->SetKey(account_id_1_.GetUserEmail(),
                            base::Value("Elaine"));

  // Add some arbitrary data to make sure the "UserGivenName" dictionary isn't
  // cleaning up itself.
  given_name_update->SetKey("sanity.check@example.com", base::Value("Anne"));
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
                   ->GetDictionary("UserGivenName")
                   ->FindKey(account_id_1_.GetUserEmail()));

  // The arbitrary data remains.
  const std::string* value = g_browser_process->local_state()
                                 ->GetDictionary("UserGivenName")
                                 ->FindStringKey("sanity.check@example.com");
  ASSERT_TRUE(value);
  EXPECT_EQ("Anne", *value);
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, LoginScreen) {
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

// Flaky: http://crbug.com/512670.
IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, DISABLED_DisplayName) {
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  // Verify that the display name is shown in the UI.
  const std::string get_compact_pod_display_name = base::StringPrintf(
      "domAutomationController.send(document.getElementById('pod-row')"
      "    .getPodWithUsername_('%s').nameElement.textContent);",
      account_id_1_.Serialize().c_str());
  std::string display_name;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      contents_,
      get_compact_pod_display_name,
      &display_name));
  EXPECT_EQ(kDisplayName1, display_name);
  const std::string get_expanded_pod_display_name = base::StringPrintf(
      "domAutomationController.send(document.getElementById('pod-row')"
      "    .getPodWithUsername_('%s').querySelector('.expanded-pane-name')"
      "        .textContent);",
      account_id_1_.Serialize().c_str());
  display_name.clear();
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      contents_,
      get_expanded_pod_display_name,
      &display_name));
  EXPECT_EQ(kDisplayName1, display_name);

  // Click on the pod to expand it.
  ASSERT_TRUE(content::ExecuteScript(
      contents_,
      base::StringPrintf(
          "document.getElementById('pod-row').getPodWithUsername_('%s')"
          "    .click();",
          account_id_1_.Serialize().c_str())));

  // Change the display name.
  device_local_account_policy_.payload().mutable_userdisplayname()->set_value(
      kDisplayName2);
  UploadAndInstallDeviceLocalAccountPolicy();
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  DeviceLocalAccountPolicyBroker* broker =
      connector->GetDeviceLocalAccountPolicyService()->GetBrokerForUser(
          account_id_1_.GetUserEmail());
  ASSERT_TRUE(broker);
  broker->core()->store()->Load();
  WaitForDisplayName(account_id_1_.GetUserEmail(), kDisplayName2);

  // Verify that the new display name is shown in the UI.
  display_name.clear();
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      contents_,
      get_compact_pod_display_name,
      &display_name));
  EXPECT_EQ(kDisplayName2, display_name);
  display_name.clear();
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      contents_,
      get_expanded_pod_display_name,
      &display_name));
  EXPECT_EQ(kDisplayName2, display_name);

  // Verify that the pod is still expanded. This indicates that the UI updated
  // without reloading and losing state.
  bool expanded = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      contents_,
      base::StringPrintf(
          "domAutomationController.send(document.getElementById('pod-row')"
          "    .getPodWithUsername_('%s').expanded);",
          account_id_1_.Serialize().c_str()),
      &expanded));
  EXPECT_TRUE(expanded);
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, PolicyDownload) {
  UploadDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  // Sanity check: The policy should be present now.
  ASSERT_FALSE(session_manager_client()->device_local_account_policy(
      kAccountId1).empty());
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

  local_policy_mixin_.UpdateDevicePolicy(policy);
  g_browser_process->policy_service()->RefreshPolicies(base::Closure());

  // Make sure the second device-local account disappears.
  WaitUntilLocalStateChanged();
  EXPECT_FALSE(IsKnownUser(account_id_2_));
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, StartSession) {
  // Specify startup pages.
  device_local_account_policy_.payload().mutable_restoreonstartup()->set_value(
      SessionStartupPref::kPrefValueURLs);
  em::StringListPolicyProto* startup_urls_proto =
      device_local_account_policy_.payload().mutable_restoreonstartupurls();
  for (size_t i = 0; i < base::size(kStartupURLs); ++i)
    startup_urls_proto->mutable_value()->add_entries(kStartupURLs[i]);
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
  int expected_tab_count = static_cast<int>(base::size(kStartupURLs));
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
      IdentityManagerFactory::GetForProfile(profile)->HasPrimaryAccount());
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
      base::Bind(&TestingUpdateManifestProvider::HandleRequest,
                 testing_update_manifest_provider));
  embedded_test_server()->StartAcceptingConnections();

  // Specify policy to force-install the hosted app and the extension.
  em::StringList* forcelist = device_local_account_policy_.payload()
      .mutable_extensioninstallforcelist()->mutable_value();
  forcelist->add_entries(base::StringPrintf(
      "%s;%s",
      kHostedAppID,
      embedded_test_server()->GetURL(kRelativeUpdateURL).spec().c_str()));
  forcelist->add_entries(base::StringPrintf(
      "%s;%s",
      kGoodExtensionID,
      embedded_test_server()->GetURL(kRelativeUpdateURL).spec().c_str()));

  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  // Start listening for app/extension installation results.
  ExtensionInstallObserver install_observer(kHostedAppID);
  content::WindowedNotificationObserver extension_observer(
      extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR,
      base::Bind(DoesInstallFailureReferToId, kGoodExtensionID));
  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));

  // Wait for the hosted app installation to succeed and the extension
  // installation to fail (because hosted apps are whitelisted for use in
  // device-local accounts and extensions are not).
  install_observer.Wait();
  extension_observer.Wait();

  // Verify that the hosted app was installed.
  Profile* profile = GetProfileForTest();
  ASSERT_TRUE(profile);
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  EXPECT_TRUE(extension_registry->enabled_extensions().GetByID(kHostedAppID));

  // Verify that the extension was not installed.
  EXPECT_FALSE(extension_registry->GetExtensionById(
      kGoodExtensionID, extensions::ExtensionRegistry::EVERYTHING));

  // Verify that the app was downloaded to the account's extension cache.
  base::FilePath test_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  EXPECT_TRUE(ContentsEqual(
          GetCacheCRXFile(kAccountId1, kHostedAppID, kHostedAppVersion),
          test_dir.Append(kHostedAppCRXPath)));

  // Verify that the extension was removed from the account's extension cache
  // after the installation failure.
  DeviceLocalAccountPolicyBroker* broker =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetDeviceLocalAccountPolicyService()
          ->GetBrokerForUser(account_id_1_.GetUserEmail());
  ASSERT_TRUE(broker);
  chromeos::ExternalCache* cache =
      broker->extension_loader()->GetExternalCacheForTesting();
  ASSERT_TRUE(cache);
  EXPECT_FALSE(cache->GetExtension(kGoodExtensionID, NULL, NULL));
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, ExtensionsCached) {
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
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(
        CopyFile(test_dir.Append(kHostedAppCRXPath), cached_hosted_app));
    EXPECT_TRUE(CopyFile(
        test_dir.Append(kGoodExtensionCRXPath),
        GetCacheCRXFile(kAccountId1, kGoodExtensionID, kGoodExtensionVersion)));
  }

  // Specify policy to force-install the hosted app.
  em::StringList* forcelist = device_local_account_policy_.payload()
      .mutable_extensioninstallforcelist()->mutable_value();
  forcelist->add_entries(base::StringPrintf(
      "%s;%s",
      kHostedAppID,
      embedded_test_server()->GetURL(kRelativeUpdateURL).spec().c_str()));
  forcelist->add_entries(base::StringPrintf(
      "%s;%s",
      kGoodExtensionID,
      embedded_test_server()->GetURL(kRelativeUpdateURL).spec().c_str()));

  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  // Start listening for app/extension installation results.
  ExtensionInstallObserver install_observer(kHostedAppID);
  content::WindowedNotificationObserver extension_observer(
      extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR,
      base::Bind(DoesInstallFailureReferToId, kGoodExtensionID));

  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));

  // Wait for the hosted app installation to succeed and the extension
  // installation to fail.
  install_observer.Wait();
  extension_observer.Wait();

  // Verify that the hosted app was installed.
  Profile* profile = GetProfileForTest();
  ASSERT_TRUE(profile);
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  EXPECT_TRUE(extension_registry->enabled_extensions().GetByID(kHostedAppID));

  // Verify that the extension was not installed.
  EXPECT_FALSE(extension_registry->GetExtensionById(
      kGoodExtensionID, extensions::ExtensionRegistry::EVERYTHING));

  // Verify that the app is still in the account's extension cache.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(PathExists(cached_hosted_app));
  }

  // Verify that the extension was removed from the account's extension cache.
  DeviceLocalAccountPolicyBroker* broker =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetDeviceLocalAccountPolicyService()
          ->GetBrokerForUser(account_id_1_.GetUserEmail());
  ASSERT_TRUE(broker);
  chromeos::ExternalCache* cache =
      broker->extension_loader()->GetExternalCacheForTesting();
  ASSERT_TRUE(cache);
  EXPECT_FALSE(cache->GetExtension(kGoodExtensionID, NULL, NULL));
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
  EXPECT_EQ(base::WriteFile(file, data.data(), data.size()), int(size));
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
      base::Bind(&TestingUpdateManifestProvider::HandleRequest,
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
  std::unique_ptr<base::RunLoop> run_loop;
  run_loop.reset(new base::RunLoop);
  cache_impl.Start(base::Bind(&OnExtensionCacheImplInitialized, &run_loop));
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
  run_loop.reset(new base::RunLoop);
  cache_impl.PutExtension(kGoodExtensionID, hash, temp_file,
                          kGoodExtensionVersion,
                          base::Bind(&OnPutExtension, &run_loop));
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
      .mutable_extensioninstallforcelist()->mutable_value();
  forcelist->add_entries(base::StringPrintf(
      "%s;%s",
      kHostedAppID,
      embedded_test_server()->GetURL(kRelativeUpdateURL).spec().c_str()));
  forcelist->add_entries(base::StringPrintf(
      "%s;%s",
      kGoodExtensionID,
      embedded_test_server()->GetURL(kRelativeUpdateURL).spec().c_str()));

  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  // Start listening for app/extension installation results.
  ExtensionInstallObserver install_observer(kHostedAppID);
  content::WindowedNotificationObserver extension_observer(
      extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR,
      base::Bind(DoesInstallFailureReferToId, kGoodExtensionID));

  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));

  // Wait for the hosted app installation to succeed and the extension
  // installation to fail (because hosted apps are whitelisted for use in
  // device-local accounts and extensions are not).
  install_observer.Wait();
  extension_observer.Wait();

  // Verify that the extension was kept in the local cache.
  EXPECT_TRUE(cache_impl.GetExtension(kGoodExtensionID, hash, NULL, NULL));

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
  reinterpret_cast<chromeos::ChromeUserManagerImpl*>(
      user_manager::UserManager::Get())->StopPolicyObserverForTesting();

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
        if (request.relative_url != kExternalDataPath)
          return nullptr;

        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_content(kExternalData);

        quit_closure.Run();
        return response;
      },
      run_loop->QuitClosure()));
  embedded_test_server()->StartAcceptingConnections();

  // Specify an external data reference for the key::kUserAvatarImage policy.
  std::unique_ptr<base::DictionaryValue> metadata =
      test::ConstructExternalDataReference(
          embedded_test_server()->GetURL(kExternalDataPath).spec(),
          kExternalData);
  std::string policy;
  base::JSONWriter::Write(*metadata, &policy);
  device_local_account_policy_.payload().mutable_useravatarimage()->set_value(
      policy);
  UploadAndInstallDeviceLocalAccountPolicy();
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  DeviceLocalAccountPolicyBroker* broker =
      connector->GetDeviceLocalAccountPolicyService()->GetBrokerForUser(
          account_id_1_.GetUserEmail());
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
  run_loop.reset(new base::RunLoop);
  std::unique_ptr<std::string> fetched_external_data;
  base::FilePath file_path;
  policy_entry->external_data_fetcher->Fetch(
      base::BindOnce(&test::ExternalDataFetchCallback, &fetched_external_data,
                     &file_path, run_loop->QuitClosure()));
  run_loop->Run();

  ASSERT_TRUE(fetched_external_data);
  EXPECT_EQ(kExternalData, *fetched_external_data);

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
  EXPECT_EQ(*metadata, *policy_entry->value);
  ASSERT_TRUE(policy_entry->external_data_fetcher);

  // Retrieve the external data via the ProfilePolicyConnector. The retrieval
  // should succeed because the data has been cached.
  run_loop.reset(new base::RunLoop);
  fetched_external_data.reset();
  policy_entry->external_data_fetcher->Fetch(
      base::BindOnce(&test::ExternalDataFetchCallback, &fetched_external_data,
                     &file_path, run_loop->QuitClosure()));
  run_loop->Run();

  ASSERT_TRUE(fetched_external_data);
  EXPECT_EQ(kExternalData, *fetched_external_data);
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, UserAvatarImage) {
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
        test_dir.Append(chromeos::test::kUserAvatarImage1RelativePath),
        &image_data));
  }

  std::string policy;
  base::JSONWriter::Write(
      *test::ConstructExternalDataReference(
          embedded_test_server()
              ->GetURL(std::string("/") +
                       chromeos::test::kUserAvatarImage1RelativePath)
              .spec(),
          image_data),
      &policy);
  device_local_account_policy_.payload().mutable_useravatarimage()->set_value(
      policy);
  UploadAndInstallDeviceLocalAccountPolicy();
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  DeviceLocalAccountPolicyBroker* broker =
      connector->GetDeviceLocalAccountPolicyService()->GetBrokerForUser(
          account_id_1_.GetUserEmail());
  ASSERT_TRUE(broker);

  broker->core()->store()->Load();
  WaitUntilLocalStateChanged();

  gfx::ImageSkia policy_image =
      chromeos::test::ImageLoader(
          test_dir.Append(chromeos::test::kUserAvatarImage1RelativePath))
          .Load();
  ASSERT_FALSE(policy_image.isNull());

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id_1_);
  ASSERT_TRUE(user);

  base::FilePath user_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  const base::FilePath saved_image_path =
      user_data_dir.Append(account_id_1_.GetUserEmail()).AddExtension("jpg");

  EXPECT_FALSE(user->HasDefaultImage());
  EXPECT_EQ(user_manager::User::USER_IMAGE_EXTERNAL, user->image_index());
  EXPECT_TRUE(chromeos::test::AreImagesEqual(policy_image, user->GetImage()));
  const base::DictionaryValue* images_pref =
      g_browser_process->local_state()->GetDictionary("user_image_info");
  ASSERT_TRUE(images_pref);
  const base::DictionaryValue* image_properties;
  ASSERT_TRUE(images_pref->GetDictionaryWithoutPathExpansion(
      account_id_1_.GetUserEmail(), &image_properties));
  int image_index;
  std::string image_path;
  ASSERT_TRUE(image_properties->GetInteger("index", &image_index));
  ASSERT_TRUE(image_properties->GetString("path", &image_path));
  EXPECT_EQ(user_manager::User::USER_IMAGE_EXTERNAL, image_index);
  EXPECT_EQ(saved_image_path.value(), image_path);

  gfx::ImageSkia saved_image =
      chromeos::test::ImageLoader(saved_image_path).Load();
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

  // Remove policy that allows only explicitly whitelisted apps to be installed
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
  content::WindowedNotificationObserver app_install_observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  base::FilePath test_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  installer->InstallCrx(test_dir.Append(kPackagedAppCRXPath));
  app_install_observer.Wait();
  const extensions::Extension* app =
      content::Details<const extensions::Extension>(
          app_install_observer.details()).ptr();

  // Start the platform app, causing it to open a window.
  run_loop_.reset(new base::RunLoop);
  apps::LaunchService::Get(profile)->OpenApplication(apps::AppLaunchParams(
      app->id(), apps::mojom::LaunchContainer::kLaunchContainerNone,
      WindowOpenDisposition::NEW_WINDOW,
      apps::mojom::AppLaunchSource::kSourceTest));
  run_loop_->Run();
  EXPECT_EQ(1U, app_window_registry->app_windows().size());

  // Close the only open browser window.
  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());
  Browser* browser = browser_list->get(0);
  ASSERT_TRUE(browser);
  BrowserWindow* browser_window = browser->window();
  ASSERT_TRUE(browser_window);
  run_loop_.reset(new base::RunLoop);
  browser_window->Close();
  browser_window = NULL;
  run_loop_->Run();
  browser = NULL;
  EXPECT_TRUE(browser_list->empty());

  // Verify that the logout confirmation dialog is not showing because an app
  // window is still open.
  EXPECT_FALSE(IsLogoutConfirmationDialogShowing());

  // Open a browser window.
  Browser* first_browser = CreateBrowser(profile);
  EXPECT_EQ(1U, browser_list->size());

  // Close the app window.
  run_loop_.reset(new base::RunLoop);
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
  run_loop_.reset(new base::RunLoop);
  browser_window->Close();
  browser_window = NULL;
  run_loop_->Run();
  first_browser = NULL;
  EXPECT_EQ(1U, browser_list->size());

  // Verify that the logout confirmation dialog is not showing because a browser
  // window is still open.
  EXPECT_FALSE(IsLogoutConfirmationDialogShowing());

  // Close the second browser window.
  browser_window = second_browser->window();
  ASSERT_TRUE(browser_window);
  run_loop_.reset(new base::RunLoop);
  browser_window->Close();
  browser_window = NULL;
  run_loop_->Run();
  second_browser = NULL;
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
  run_loop_.reset(new base::RunLoop);
  browser_window->Close();
  browser_window = NULL;
  run_loop_->Run();
  browser = NULL;
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
  ASSERT_TRUE(content::ExecuteScript(
      contents_,
      base::StringPrintf(
          "document.getElementById('pod-row').getPodWithUsername_('%s')"
          "    .querySelector('.enter-button').click();",
          account_id_1_.Serialize().c_str())));

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
  bool advanced = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      contents_,
      base::StringPrintf(
          "var pod ="
          "    document.getElementById('pod-row').getPodWithUsername_('%s');"
          "pod.querySelector('.language-and-input').click();"
          "domAutomationController.send(pod.classList.contains('advanced'));",
          account_id_1_.Serialize().c_str()),
      &advanced));
  // Public session pods switch to advanced form immediately upon being
  // clicked, instead of waiting for animation to end which were in the old UI.
  EXPECT_TRUE(advanced);

  // Manually select a different locale.
  ASSERT_TRUE(content::ExecuteScript(
      contents_,
      base::StringPrintf(
          "var languageSelect = document.getElementById('pod-row')"
          "    .getPodWithUsername_('%s').querySelector('.language-select');"
          "languageSelect.value = '%s';"
          "var event = document.createEvent('HTMLEvents');"
          "event.initEvent('change', false, true);"
          "languageSelect.dispatchEvent(event);",
          account_id_1_.Serialize().c_str(), kPublicSessionLocale)));

  // The UI will have requested an updated list of keyboard layouts at this
  // point. Wait for the constructions of this list to finish.
  WaitForGetKeyboardLayoutsForLocaleToFinish();

  // Manually select a different keyboard layout and click the enter button to
  // start the session.
  ASSERT_TRUE(content::ExecuteScript(
      contents_,
      base::StringPrintf(
          "var pod ="
          "    document.getElementById('pod-row').getPodWithUsername_('%s');"
          "pod.querySelector('.keyboard-select').value = '%s';"
          "pod.querySelector('.enter-button').click();",
          account_id_1_.Serialize().c_str(),
          public_session_input_method_id_.c_str())));

  WaitForSessionStart();

  // Verify that the locale and keyboard layout have been applied.
  EXPECT_EQ(kPublicSessionLocale, g_browser_process->GetApplicationLocale());
  EXPECT_EQ(l10n_util::GetLanguage(kPublicSessionLocale),
            icu::Locale::getDefault().getLanguage());
  EXPECT_EQ(public_session_input_method_id_,
            chromeos::input_method::InputMethodManager::Get()
                ->GetActiveIMEState()
                ->GetCurrentInputMethod()
                .id());
}

// Tests whether or not managed guest session users can change the system
// timezone, which should be possible iff the timezone automatic detection
// policy is set to either DISABLED or USERS_DECIDE.
IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, ManagedSessionTimezoneChange) {
  SetManagedSessionsEnabled(true);
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);
  EnableAutoLogin();

  WaitForPolicy();

  WaitForSessionStart();

  CheckPublicSessionPresent(account_id_1_);

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id_1_);
  ASSERT_TRUE(user);
  ASSERT_EQ(user->GetType(), user_manager::USER_TYPE_PUBLIC_ACCOUNT);

  std::string timezone_id1("America/Los_Angeles");
  std::string timezone_id2("Europe/Berlin");
  base::string16 timezone_id1_utf16(base::UTF8ToUTF16(timezone_id1));
  base::string16 timezone_id2_utf16(base::UTF8ToUTF16(timezone_id2));

  chromeos::system::TimezoneSettings* timezone_settings =
      chromeos::system::TimezoneSettings::GetInstance();

  timezone_settings->SetTimezoneFromID(timezone_id1_utf16);
  SetSystemTimezoneAutomaticDetectionPolicy(em::SystemTimezoneProto::DISABLED);
  chromeos::system::SetSystemTimezone(user, timezone_id2);
  EXPECT_EQ(timezone_settings->GetCurrentTimezoneID(), timezone_id2_utf16);

  timezone_settings->SetTimezoneFromID(timezone_id1_utf16);
  SetSystemTimezoneAutomaticDetectionPolicy(
      em::SystemTimezoneProto::USERS_DECIDE);
  chromeos::system::SetSystemTimezone(user, timezone_id2);
  EXPECT_EQ(timezone_settings->GetCurrentTimezoneID(), timezone_id2_utf16);

  timezone_settings->SetTimezoneFromID(timezone_id1_utf16);
  SetSystemTimezoneAutomaticDetectionPolicy(em::SystemTimezoneProto::IP_ONLY);
  chromeos::system::SetSystemTimezone(user, timezone_id2);
  EXPECT_NE(timezone_settings->GetCurrentTimezoneID(), timezone_id2_utf16);

  timezone_settings->SetTimezoneFromID(timezone_id1_utf16);
  SetSystemTimezoneAutomaticDetectionPolicy(
      em::SystemTimezoneProto::SEND_WIFI_ACCESS_POINTS);
  chromeos::system::SetSystemTimezone(user, timezone_id2);
  EXPECT_NE(timezone_settings->GetCurrentTimezoneID(), timezone_id2_utf16);

  timezone_settings->SetTimezoneFromID(timezone_id1_utf16);
  SetSystemTimezoneAutomaticDetectionPolicy(
      em::SystemTimezoneProto::SEND_ALL_LOCATION_INFO);
  chromeos::system::SetSystemTimezone(user, timezone_id2);
  EXPECT_NE(timezone_settings->GetCurrentTimezoneID(), timezone_id2_utf16);
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, OneRecommendedLocale) {
  // Specify a recommended locale.
  SetRecommendedLocales(kSingleRecommendedLocale,
                        base::size(kSingleRecommendedLocale));
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  ExpandPublicSessionPod(false);

  // Click the enter button to start the session.
  ASSERT_TRUE(content::ExecuteScript(
      contents_,
      base::StringPrintf(
          "document.getElementById('pod-row').getPodWithUsername_('%s')"
          "    .querySelector('.enter-button').click();",
          account_id_1_.Serialize().c_str())));

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
  SetRecommendedLocales(kRecommendedLocales1, base::size(kRecommendedLocales1));
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);
  AddPublicSessionToDevicePolicy(kAccountId2);

  WaitForPolicy();

  ExpandPublicSessionPod(true);

  // Verify that the pod shows a list of locales beginning with the recommended
  // ones, followed by others.
  const std::string get_locale_list = base::StringPrintf(
      "var languageSelect = document.getElementById('pod-row')"
      "    .getPodWithUsername_('%s').querySelector('.language-select');"
      "var locales = [];"
      "for (var i = 0; i < languageSelect.length; ++i)"
      "  locales.push(languageSelect.options[i].value);"
      "domAutomationController.send(JSON.stringify(locales));",
      account_id_1_.Serialize().c_str());
  std::string json;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(contents_,
                                                     get_locale_list,
                                                     &json));
  std::unique_ptr<base::Value> value_ptr =
      base::JSONReader::ReadDeprecated(json);
  const base::ListValue* locales = NULL;
  ASSERT_TRUE(value_ptr);
  ASSERT_TRUE(value_ptr->GetAsList(&locales));
  EXPECT_LT(base::size(kRecommendedLocales1), locales->GetSize());

  // Verify that the list starts with the recommended locales, in correct order.
  for (size_t i = 0; i < base::size(kRecommendedLocales1); ++i) {
    std::string locale;
    EXPECT_TRUE(locales->GetString(i, &locale));
    EXPECT_EQ(kRecommendedLocales1[i], locale);
  }

  // Verify that the recommended locales do not appear again in the remainder of
  // the list.
  std::set<std::string> recommended_locales;
  for (size_t i = 0; i < base::size(kRecommendedLocales1); ++i)
    recommended_locales.insert(kRecommendedLocales1[i]);
  for (size_t i = base::size(kRecommendedLocales1); i < locales->GetSize();
       ++i) {
    std::string locale;
    EXPECT_TRUE(locales->GetString(i, &locale));
    EXPECT_EQ(recommended_locales.end(), recommended_locales.find(locale));
  }

  // Verify that the first recommended locale is selected.
  const std::string get_selected_locale = base::StringPrintf(
      "domAutomationController.send(document.getElementById('pod-row')"
      "    .getPodWithUsername_('%s').querySelector('.language-select')"
      "        .value);",
      account_id_1_.Serialize().c_str());
  std::string selected_locale;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(contents_,
                                                     get_selected_locale,
                                                     &selected_locale));
  EXPECT_EQ(kRecommendedLocales1[0], selected_locale);

  // Change the list of recommended locales.
  SetRecommendedLocales(kRecommendedLocales2, base::size(kRecommendedLocales2));

  // Also change the display name as it is easy to ensure that policy has been
  // updated by waiting for a display name change.
  device_local_account_policy_.payload().mutable_userdisplayname()->set_value(
      kDisplayName2);
  UploadAndInstallDeviceLocalAccountPolicy();
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  DeviceLocalAccountPolicyBroker* broker =
      connector->GetDeviceLocalAccountPolicyService()->GetBrokerForUser(
          account_id_1_.GetUserEmail());
  ASSERT_TRUE(broker);
  broker->core()->store()->Load();
  WaitForDisplayName(account_id_1_.GetUserEmail(), kDisplayName2);

  // Verify that the new list of locales is shown in the UI.
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(contents_,
                                                     get_locale_list,
                                                     &json));
  value_ptr = base::JSONReader::ReadDeprecated(json);
  locales = NULL;
  ASSERT_TRUE(value_ptr);
  ASSERT_TRUE(value_ptr->GetAsList(&locales));
  EXPECT_LT(base::size(kRecommendedLocales2), locales->GetSize());
  for (size_t i = 0; i < base::size(kRecommendedLocales2); ++i) {
    std::string locale;
    EXPECT_TRUE(locales->GetString(i, &locale));
    EXPECT_EQ(kRecommendedLocales2[i], locale);
  }

  // Verify that the first new recommended locale is selected.
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(contents_,
                                                     get_selected_locale,
                                                     &selected_locale));
  EXPECT_EQ(kRecommendedLocales2[0], selected_locale);

  // Manually select a different locale.
  ASSERT_TRUE(content::ExecuteScript(
      contents_,
      base::StringPrintf(
          "var languageSelect = document.getElementById('pod-row')"
          "    .getPodWithUsername_('%s').querySelector('.language-select');"
          "languageSelect.value = '%s';"
          "var event = document.createEvent('HTMLEvents');"
          "event.initEvent('change', false, true);"
          "languageSelect.dispatchEvent(event);",
          account_id_1_.Serialize().c_str(), kPublicSessionLocale)));

  // Change the list of recommended locales.
  SetRecommendedLocales(kRecommendedLocales2, base::size(kRecommendedLocales2));
  device_local_account_policy_.payload().mutable_userdisplayname()->set_value(
      kDisplayName1);
  UploadAndInstallDeviceLocalAccountPolicy();
  broker->core()->store()->Load();
  WaitForDisplayName(account_id_1_.GetUserEmail(), kDisplayName1);

  // Verify that the manually selected locale is still selected.
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(contents_,
                                                     get_selected_locale,
                                                     &selected_locale));
  EXPECT_EQ(kPublicSessionLocale, selected_locale);

  // The UI will request an updated list of keyboard layouts at this point. Wait
  // for the constructions of this list to finish.
  WaitForGetKeyboardLayoutsForLocaleToFinish();

  // Manually select a different keyboard layout.
  ASSERT_TRUE(content::ExecuteScript(
      contents_,
      base::StringPrintf(
          "document.getElementById('pod-row').getPodWithUsername_('%s')"
          "    .querySelector('.keyboard-select').value = '%s';",
          account_id_1_.Serialize().c_str(),
          public_session_input_method_id_.c_str())));

  // Click on a different pod, causing focus to shift away and the pod to
  // contract.
  ASSERT_TRUE(content::ExecuteScript(
      contents_,
      base::StringPrintf(
          "document.getElementById('pod-row').getPodWithUsername_('%s')"
          "    .click();",
          account_id_2_.Serialize().c_str())));

  // Click on the pod again, causing it to expand again. Verify that the pod has
  // kept all its state (the advanced form is being shown, the manually selected
  // locale and keyboard layout are selected).
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      contents_,
      base::StringPrintf(
          "var pod ="
          "    document.getElementById('pod-row').getPodWithUsername_('%s');"
          "pod.click();"
          "var state = {};"
          "state.advanced = pod.classList.contains('advanced');"
          "state.locale = pod.querySelector('.language-select').value;"
          "state.keyboardLayout = pod.querySelector('.keyboard-select').value;"
          "console.log(JSON.stringify(state));"
          "domAutomationController.send(JSON.stringify(state));",
          account_id_1_.Serialize().c_str()),
      &json));
  LOG(ERROR) << json;
  value_ptr = base::JSONReader::ReadDeprecated(json);
  const base::DictionaryValue* state = NULL;
  ASSERT_TRUE(value_ptr);
  ASSERT_TRUE(value_ptr->GetAsDictionary(&state));
  bool advanced = false;
  EXPECT_TRUE(state->GetBoolean("advanced", &advanced));
  EXPECT_TRUE(advanced);
  EXPECT_TRUE(state->GetString("locale", &selected_locale));
  EXPECT_EQ(kPublicSessionLocale, selected_locale);
  std::string selected_keyboard_layout;
  EXPECT_TRUE(state->GetString("keyboardLayout", &selected_keyboard_layout));
  EXPECT_EQ(public_session_input_method_id_, selected_keyboard_layout);

  // Click the enter button to start the session.
  ASSERT_TRUE(content::ExecuteScript(
      contents_,
      base::StringPrintf(
          "document.getElementById('pod-row').getPodWithUsername_('%s')"
          "    .querySelector('.enter-button').click();",
          account_id_1_.Serialize().c_str())));

  WaitForSessionStart();

  // Verify that the locale and keyboard layout have been applied.
  EXPECT_EQ(kPublicSessionLocale, g_browser_process->GetApplicationLocale());
  EXPECT_EQ(l10n_util::GetLanguage(kPublicSessionLocale),
            icu::Locale::getDefault().getLanguage());
  EXPECT_EQ(public_session_input_method_id_,
            chromeos::input_method::InputMethodManager::Get()
                ->GetActiveIMEState()
                ->GetCurrentInputMethod()
                .id());
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, InvalidRecommendedLocale) {
  // Specify an invalid recommended locale.
  SetRecommendedLocales(kInvalidRecommendedLocale,
                        base::size(kInvalidRecommendedLocale));
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  // Click on the pod to expand it. Verify that the pod expands to its basic
  // form as there is only one recommended locale.
  bool advanced = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      contents_,
      base::StringPrintf(
          "var pod ="
          "    document.getElementById('pod-row').getPodWithUsername_('%s');"
          "pod.click();"
          "domAutomationController.send(pod.classList.contains('advanced'));",
          account_id_1_.Serialize().c_str()),
      &advanced));
  EXPECT_FALSE(advanced);
  EXPECT_EQ(l10n_util::GetLanguage(initial_locale_),
            icu::Locale::getDefault().getLanguage());

  // Click the enter button to start the session.
  ASSERT_TRUE(content::ExecuteScript(
      contents_,
      base::StringPrintf(
          "document.getElementById('pod-row').getPodWithUsername_('%s')"
          "    .querySelector('.enter-button').click();",
          account_id_1_.Serialize().c_str())));

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
                           base::size(kSingleLocaleWithIME));

  EXPECT_GT(chromeos::input_method::InputMethodManager::Get()
                ->GetActiveIMEState()
                ->GetNumActiveInputMethods(),
            1u);
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, LocaleWithNoIME) {
  // Specify a locale that has only keyboard layout.
  const char* const kSingleLocaleWithNoIME[] = {"de"};
  RunWithRecommendedLocale(kSingleLocaleWithNoIME,
                           base::size(kSingleLocaleWithNoIME));

  EXPECT_EQ(1u, chromeos::input_method::InputMethodManager::Get()
                    ->GetActiveIMEState()
                    ->GetNumActiveInputMethods());
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
  SetRecommendedLocales(kRecommendedLocales1, base::size(kRecommendedLocales1));
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

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, TermsOfServiceWithLocaleSwitch) {
  // Specify Terms of Service URL.
  ASSERT_TRUE(embedded_test_server()->Start());
  device_local_account_policy_.payload().mutable_termsofserviceurl()->set_value(
      embedded_test_server()->GetURL(
          std::string("/") + kExistentTermsOfServicePath).spec());
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  // Select a different locale.
  ASSERT_TRUE(content::ExecuteScript(
      contents_,
      base::StringPrintf(
          "var languageSelect = document.getElementById('pod-row')"
          "    .getPodWithUsername_('%s').querySelector('.language-select');"
          "languageSelect.value = '%s';"
          "var event = document.createEvent('HTMLEvents');"
          "event.initEvent('change', false, true);"
          "languageSelect.dispatchEvent(event);",
          account_id_1_.Serialize().c_str(), kPublicSessionLocale)));

  // The UI will have requested an updated list of keyboard layouts at this
  // point. Wait for the constructions of this list to finish.
  WaitForGetKeyboardLayoutsForLocaleToFinish();

  // Set up an observer that will quit the message loop when login has succeeded
  // and the first wizard screen, if any, is being shown.
  base::RunLoop login_wait_run_loop;
  chromeos::MockAuthStatusConsumer login_status_consumer(
      login_wait_run_loop.QuitClosure());
  EXPECT_CALL(login_status_consumer, OnAuthSuccess(_)).Times(1).WillOnce(
      InvokeWithoutArgs(&login_wait_run_loop, &base::RunLoop::Quit));
  chromeos::ExistingUserController* controller =
      chromeos::ExistingUserController::current_controller();
  ASSERT_TRUE(controller);
  controller->set_login_status_consumer(&login_status_consumer);

  // Manually select a different keyboard layout and click the enter button to
  // start the session.
  ASSERT_TRUE(content::ExecuteScript(
      contents_,
      base::StringPrintf(
          "var pod ="
          "    document.getElementById('pod-row').getPodWithUsername_('%s');"
          "pod.querySelector('.keyboard-select').value = '%s';"
          "pod.querySelector('.enter-button').click();",
          account_id_1_.Serialize().c_str(),
          public_session_input_method_id_.c_str())));

  // Spin the loop until the login observer fires. Then, unregister the
  // observer.
  login_wait_run_loop.Run();
  controller->set_login_status_consumer(NULL);

  // Verify that the Terms of Service screen is being shown.
  chromeos::WizardController* wizard_controller =
        chromeos::WizardController::default_controller();
  ASSERT_TRUE(wizard_controller);
  ASSERT_TRUE(wizard_controller->current_screen());
  EXPECT_EQ(chromeos::TermsOfServiceScreenView::kScreenId.AsId(),
            wizard_controller->current_screen()->screen_id());

  // Wait for the Terms of Service to finish downloading.
  chromeos::test::OobeJS()
      .CreateWaiter(GetOobeElementPath({"terms-of-service"}) + ".uiState == " +
                    base::NumberToString(static_cast<int>(
                        chromeos::TermsOfServiceScreen::ScreenState::LOADED)))
      ->Wait();

  // Verify that the locale and keyboard layout have been applied.
  EXPECT_EQ(kPublicSessionLocale, g_browser_process->GetApplicationLocale());
  EXPECT_EQ(l10n_util::GetLanguage(kPublicSessionLocale),
            icu::Locale::getDefault().getLanguage());
  EXPECT_EQ(public_session_input_method_id_,
            chromeos::input_method::InputMethodManager::Get()
                ->GetActiveIMEState()
                ->GetCurrentInputMethod()
                .id());

  // Wait for 'tos-accept-button' to become enabled.
  chromeos::test::OobeJS()
      .CreateEnabledWaiter(true, {"terms-of-service", "acceptButton"})
      ->Wait();

  // Click the accept button.
  chromeos::test::OobeJS().ClickOnPath({"terms-of-service", "acceptButton"});

  WaitForSessionStart();

  // Verify that the locale and keyboard layout are still in force.
  EXPECT_EQ(kPublicSessionLocale, g_browser_process->GetApplicationLocale());
  EXPECT_EQ(l10n_util::GetLanguage(kPublicSessionLocale),
            icu::Locale::getDefault().getLanguage());
  EXPECT_EQ(public_session_input_method_id_,
            chromeos::input_method::InputMethodManager::Get()
                ->GetActiveIMEState()
                ->GetCurrentInputMethod()
                .id());
}

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, PolicyForExtensions) {
  // Set up a test update server for the Show Managed Storage app.
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  scoped_refptr<TestingUpdateManifestProvider> testing_update_manifest_provider(
      new TestingUpdateManifestProvider(kRelativeUpdateURL));
  testing_update_manifest_provider->AddUpdate(
      kShowManagedStorageID, kShowManagedStorageVersion,
      embedded_test_server()->GetURL(std::string("/") +
                                     kShowManagedStorageCRXPath));
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&TestingUpdateManifestProvider::HandleRequest,
                 testing_update_manifest_provider));
  embedded_test_server()->StartAcceptingConnections();

  // Force-install the Show Managed Storage app. This app can be installed in
  // public sessions because it's whitelisted for testing purposes.
  em::StringList* forcelist = device_local_account_policy_.payload()
      .mutable_extensioninstallforcelist()->mutable_value();
  forcelist->add_entries(base::StringPrintf(
      "%s;%s",
      kShowManagedStorageID,
      embedded_test_server()->GetURL(kRelativeUpdateURL).spec().c_str()));

  // Set a policy for the app at the policy testserver.
  // Note that the policy for the device-local account will be fetched before
  // the session is started, so the policy for the app must be installed before
  // the first device policy fetch.
  ASSERT_TRUE(local_policy_mixin_.server()->UpdatePolicyData(
      dm_protocol::kChromeExtensionPolicyType, kShowManagedStorageID,
      "{"
      "  \"string\": {"
      "    \"Value\": \"policy test value one\""
      "  }"
      "}"));

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
  if (policy_service->GetPolicies(ns).empty()) {
    PolicyChangeRegistrar policy_registrar(policy_service, ns);
    base::RunLoop run_loop;
    policy_registrar.Observe(
        "string", base::Bind(&PolicyChangedCallback, run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Verify that the app policy was set.
  base::Value expected_value("policy test value one");
  EXPECT_EQ(expected_value,
            *policy_service->GetPolicies(ns).GetValue("string"));

  // Now update the policy at the server.
  ASSERT_TRUE(local_policy_mixin_.server()->UpdatePolicyData(
      dm_protocol::kChromeExtensionPolicyType, kShowManagedStorageID,
      "{"
      "  \"string\": {"
      "    \"Value\": \"policy test value two\""
      "  }"
      "}"));

  // And issue a policy refresh.
  {
    PolicyChangeRegistrar policy_registrar(policy_service, ns);
    base::RunLoop run_loop;
    policy_registrar.Observe(
        "string", base::Bind(&PolicyChangedCallback, run_loop.QuitClosure()));
    policy_service->RefreshPolicies(base::Closure());
    run_loop.Run();
  }

  // Verify that the app policy was updated.
  base::Value expected_new_value("policy test value two");
  EXPECT_EQ(expected_new_value,
            *policy_service->GetPolicies(ns).GetValue("string"));
}

class ManagedSessionsTest : public DeviceLocalAccountTest {
 protected:
  class CertsObserver : public chromeos::PolicyCertificateProvider::Observer {
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
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&TestingUpdateManifestProvider::HandleRequest,
                            testing_update_manifest_provider));
    embedded_test_server()->StartAcceptingConnections();
  }

  DeviceLocalAccountPolicyBroker* GetDeviceLocalAccountPolicyBroker() {
    return g_browser_process->platform_part()
        ->browser_policy_connector_chromeos()
        ->GetDeviceLocalAccountPolicyService()
        ->GetBrokerForUser(account_id_1_.GetUserEmail());
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

  void AddForceInstalledExtension() { AddExtension(kGoodExtensionID); }

  void AddForceInstalledWhitelistedExtension() {
    AddExtension(kPublicSessionWhitelistedExtensionID);
  }

  void WaitForCertificateUpdate() {
    policy::DeviceNetworkConfigurationUpdater* updater =
        g_browser_process->platform_part()
            ->browser_policy_connector_chromeos()
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

IN_PROC_BROWSER_TEST_F(ManagedSessionsTest, ManagedSessionsDisabled) {
  SetManagedSessionsEnabled(/* managed_sessions_enabled */ false);

  // Install and refresh the device policy now. This will also fetch the initial
  // user policy for the device-local account now.
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);
  WaitForPolicy();

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id_1_);
  ASSERT_TRUE(user);
  auto* broker = GetDeviceLocalAccountPolicyBroker();
  ASSERT_TRUE(broker);

  // Check that managed sessions mode is disabled.
  EXPECT_FALSE(
      chromeos::ChromeUserManager::Get()->IsManagedSessionEnabledForUser(
          *user));

  // Check that disabled managed sessions mode hides full management disclosure
  // warning.
  EXPECT_FALSE(
      chromeos::ChromeUserManager::Get()->IsFullManagementDisclosureNeeded(
          broker));
}

IN_PROC_BROWSER_TEST_F(ManagedSessionsTest, ManagedSessionsEnabledNonRisky) {
  SetManagedSessionsEnabled(/* managed_sessions_enabled */ true);

  // Install and refresh the device policy now. This will also fetch the initial
  // user policy for the device-local account now.
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);
  WaitForPolicy();

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id_1_);
  ASSERT_TRUE(user);
  auto* broker = GetDeviceLocalAccountPolicyBroker();
  ASSERT_TRUE(broker);

  // Check that managed sessions mode is enabled.
  ASSERT_TRUE(
      chromeos::ChromeUserManager::Get()->IsManagedSessionEnabledForUser(
          *user));

  // Check that management disclosure warning is not shown when managed sessions
  // are enabled, but policy settings are not risky.
  ASSERT_FALSE(
      chromeos::ChromeUserManager::Get()->IsFullManagementDisclosureNeeded(
          broker));
}

IN_PROC_BROWSER_TEST_F(ManagedSessionsTest, ForceInstalledExtension) {
  SetManagedSessionsEnabled(/* managed_sessions_enabled */ true);
  StartTestExtensionsServer();
  AddForceInstalledExtension();

  // Install and refresh the device policy now. This will also fetch the initial
  // user policy for the device-local account now.
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);
  WaitForPolicy();

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id_1_);
  ASSERT_TRUE(user);
  auto* broker = GetDeviceLocalAccountPolicyBroker();
  ASSERT_TRUE(broker);

  // Check that 'DeviceLocalAccountManagedSessionEnabled' policy was applied
  // correctly.
  EXPECT_TRUE(
      chromeos::ChromeUserManager::Get()->IsManagedSessionEnabledForUser(
          *user));

  // Check that force-installed extension activates managed session mode for
  // device-local users.
  EXPECT_TRUE(
      chromeos::ChromeUserManager::Get()->IsFullManagementDisclosureNeeded(
          broker));
}

IN_PROC_BROWSER_TEST_F(ManagedSessionsTest, WhitelistedExtension) {
  SetManagedSessionsEnabled(/* managed_sessions_enabled */ true);
  StartTestExtensionsServer();
  AddForceInstalledWhitelistedExtension();

  // Install and refresh the device policy now. This will also fetch the initial
  // user policy for the device-local account now.
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);
  WaitForPolicy();

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id_1_);
  ASSERT_TRUE(user);
  auto* broker = GetDeviceLocalAccountPolicyBroker();
  ASSERT_TRUE(broker);

  // Check that 'DeviceLocalAccountManagedSessionEnabled' policy was applied
  // correctly.
  EXPECT_TRUE(
      chromeos::ChromeUserManager::Get()->IsManagedSessionEnabledForUser(
          *user));

  // Check that white-listed extension is not considered risky and doesn't
  // activate managed session mode.
  EXPECT_FALSE(
      chromeos::ChromeUserManager::Get()->IsFullManagementDisclosureNeeded(
          broker));
}

IN_PROC_BROWSER_TEST_F(ManagedSessionsTest, NetworkCertificate) {
  SetManagedSessionsEnabled(/* managed_sessions_enabled */ true);

  device_local_account_policy_.payload()
      .mutable_opennetworkconfiguration()
      ->set_value(kFakeOncWithCertificate);

  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id_1_);
  ASSERT_TRUE(user);
  auto* broker = GetDeviceLocalAccountPolicyBroker();
  ASSERT_TRUE(broker);

  // Check that 'DeviceLocalAccountManagedSessionEnabled' policy was applied
  // correctly.
  EXPECT_TRUE(
      chromeos::ChromeUserManager::Get()->IsManagedSessionEnabledForUser(
          *user));

  // Check that network certificate pushed via policy activates managed sessions
  // mode.
  EXPECT_TRUE(
      chromeos::ChromeUserManager::Get()->IsFullManagementDisclosureNeeded(
          broker));
}

IN_PROC_BROWSER_TEST_F(ManagedSessionsTest, AllowCrossOriginAuthPrompt) {
  SetManagedSessionsEnabled(/* managed_sessions_enabled */ true);

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
  auto* broker = GetDeviceLocalAccountPolicyBroker();
  ASSERT_TRUE(broker);

  // Check that 'DeviceLocalAccountManagedSessionEnabled' policy was applied
  // correctly.
  ASSERT_TRUE(
      chromeos::ChromeUserManager::Get()->IsManagedSessionEnabledForUser(
          *user));

  // Check that setting a value to 'AllowCrossOriginAuthPrompt' activates
  // managed sessions mode.
  ASSERT_TRUE(
      chromeos::ChromeUserManager::Get()->IsFullManagementDisclosureNeeded(
          broker));
}

class TermsOfServiceDownloadTest : public DeviceLocalAccountTest,
                                   public testing::WithParamInterface<bool> {
};

IN_PROC_BROWSER_TEST_P(TermsOfServiceDownloadTest, TermsOfServiceScreen) {
  // Parameterization for using valid and invalid URLs.
  const bool use_valid_url = GetParam();

  // Specify Terms of Service URL.
  ASSERT_TRUE(embedded_test_server()->Start());
  device_local_account_policy_.payload().mutable_termsofserviceurl()->set_value(
      embedded_test_server()
          ->GetURL(std::string("/") + (use_valid_url
                                           ? kExistentTermsOfServicePath
                                           : kNonexistentTermsOfServicePath))
          .spec());
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));

  // Set up an observer that will quit the message loop when login has succeeded
  // and the first wizard screen, if any, is being shown.
  base::RunLoop login_wait_run_loop;
  chromeos::MockAuthStatusConsumer login_status_consumer(
      login_wait_run_loop.QuitClosure());
  EXPECT_CALL(login_status_consumer, OnAuthSuccess(_)).Times(1).WillOnce(
      InvokeWithoutArgs(&login_wait_run_loop, &base::RunLoop::Quit));

  // Spin the loop until the observer fires. Then, unregister the observer.
  chromeos::ExistingUserController* controller =
      chromeos::ExistingUserController::current_controller();
  ASSERT_TRUE(controller);
  controller->set_login_status_consumer(&login_status_consumer);
  login_wait_run_loop.Run();
  controller->set_login_status_consumer(NULL);

  // Verify that the Terms of Service screen is being shown.
  chromeos::WizardController* wizard_controller =
        chromeos::WizardController::default_controller();
  ASSERT_TRUE(wizard_controller);
  ASSERT_TRUE(wizard_controller->current_screen());
  EXPECT_EQ(chromeos::TermsOfServiceScreenView::kScreenId.AsId(),
            wizard_controller->current_screen()->screen_id());

  // Wait for the Terms of Service to finish loading.

  if (!use_valid_url) {
    // The Terms of Service URL was invalid. Verify that the screen is showing
    // an error and the accept button is disabled.
    chromeos::test::OobeJS()
        .CreateVisibilityWaiter(
            true, {"terms-of-service", "termsOfServiceErrorDialog"})
        ->Wait();

    chromeos::test::OobeJS().ExpectTrue(
        GetOobeElementPath({"terms-of-service"}) + ".uiState == " +
        base::NumberToString(static_cast<int>(
            chromeos::TermsOfServiceScreen::ScreenState::ERROR)));

    chromeos::test::OobeJS().ExpectDisabledPath(
        {"terms-of-service", "acceptButton"});
    return;
  }

  chromeos::test::OobeJS()
      .CreateWaiter(GetOobeElementPath({"terms-of-service"}) + ".uiState == " +
                    base::NumberToString(static_cast<int>(
                        chromeos::TermsOfServiceScreen::ScreenState::LOADED)))
      ->Wait();

  chromeos::test::OobeJS()
      .CreateVisibilityWaiter(true, {"terms-of-service", "termsOfServiceFrame"})
      ->Wait();

  // Get the Terms Of Service from the webview.
  const std::string content = chromeos::test::GetWebViewContents(
      {"terms-of-service", "termsOfServiceFrame"});

  // Get the expected values for heading and subheading.
  const std::string expected_heading = l10n_util::GetStringFUTF8(
      IDS_TERMS_OF_SERVICE_SCREEN_HEADING, base::UTF8ToUTF16(kDomain));
  const std::string expected_subheading = l10n_util::GetStringFUTF8(
      IDS_TERMS_OF_SERVICE_SCREEN_SUBHEADING, base::UTF8ToUTF16(kDomain));

  // Compare heading and subheading
  chromeos::test::OobeJS().ExpectEQ(
      GetOobeElementPath({"terms-of-service", "tosHeading"}) + ".textContent",
      expected_heading);
  chromeos::test::OobeJS().ExpectEQ(
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

  chromeos::test::OobeJS().ExpectFalse(
      GetOobeElementPath({"terms-of-service"}) + ".uiState == " +
      base::NumberToString(static_cast<int>(
          chromeos::TermsOfServiceScreen::ScreenState::ERROR)));

  chromeos::test::OobeJS().ExpectEnabledPath(
      {"terms-of-service", "acceptButton"});

  // Click the accept button.
  chromeos::test::OobeJS().ClickOnPath({"terms-of-service", "acceptButton"});

  WaitForSessionStart();
}

IN_PROC_BROWSER_TEST_P(TermsOfServiceDownloadTest, DeclineTermsOfService) {
  // Specify Terms of Service URL.
  ASSERT_TRUE(embedded_test_server()->Start());
  device_local_account_policy_.payload().mutable_termsofserviceurl()->set_value(
      embedded_test_server()
          ->GetURL(std::string("/") + (GetParam()
                                           ? kExistentTermsOfServicePath
                                           : kNonexistentTermsOfServicePath))
          .spec());
  UploadAndInstallDeviceLocalAccountPolicy();
  AddPublicSessionToDevicePolicy(kAccountId1);

  WaitForPolicy();

  ASSERT_NO_FATAL_FAILURE(StartLogin(std::string(), std::string()));

  // Set up an observer that will quit the message loop when login has succeeded
  // and the first wizard screen, if any, is being shown.
  base::RunLoop login_wait_run_loop;
  chromeos::MockAuthStatusConsumer login_status_consumer(
      login_wait_run_loop.QuitClosure());
  EXPECT_CALL(login_status_consumer, OnAuthSuccess(_))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(&login_wait_run_loop, &base::RunLoop::Quit));

  // Spin the loop until the observer fires. Then, unregister the observer.
  chromeos::ExistingUserController* controller =
      chromeos::ExistingUserController::current_controller();
  ASSERT_TRUE(controller);
  controller->set_login_status_consumer(&login_status_consumer);
  login_wait_run_loop.Run();
  controller->set_login_status_consumer(NULL);

  // Verify that the Terms of Service screen is being shown.
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  ASSERT_TRUE(wizard_controller);
  ASSERT_TRUE(wizard_controller->current_screen());
  EXPECT_EQ(chromeos::TermsOfServiceScreenView::kScreenId.AsId(),
            wizard_controller->current_screen()->screen_id());

  // Click the back button.
  chromeos::test::OobeJS().ClickOnPath({"terms-of-service", "backButton"});

  EXPECT_TRUE(session_manager_client()->session_stopped());
}

INSTANTIATE_TEST_SUITE_P(TermsOfServiceDownloadTestInstance,
                         TermsOfServiceDownloadTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_F(DeviceLocalAccountTest, WebAppsInPublicSession) {
  UploadAndInstallDeviceLocalAccountPolicy();
  // Add an account with DeviceLocalAccount::Type::TYPE_PUBLIC_SESSION.
  AddPublicSessionToDevicePolicy(kAccountId1);
  WaitForPolicy();

  StartLogin(std::string(), std::string());
  WaitForSessionStart();

  // WebAppProvider should be enabled for TYPE_PUBLIC_SESSION user account.
  Profile* profile = GetProfileForTest();
  ASSERT_TRUE(profile);
  EXPECT_TRUE(web_app::WebAppProvider::Get(profile));
}

}  // namespace policy

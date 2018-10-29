// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/signin_partition_manager.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/onc/onc_constants.h"
#include "components/onc/onc_pref_names.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "media/base/media_switches.h"
#include "net/cookies/canonical_cookie.h"
#include "net/test/cert_test_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace em = enterprise_management;

namespace chromeos {

namespace {

constexpr char kTestGuid[] = "cccccccc-cccc-4ccc-0ccc-ccccccccccc1";
constexpr char kTestCookieName[] = "TestCookie";
constexpr char kTestCookieValue[] = "present";
constexpr char kTestCookieHost[] = "host1.com";

void InjectCookieDoneCallback(base::OnceClosure done_closure, bool result) {
  ASSERT_TRUE(result);
  std::move(done_closure).Run();
}

// Injects a cookie into |storage_partition|, so we can test for cookie presence
// later to infer if the StoragePartition has been cleared.
void InjectCookie(content::StoragePartition* storage_partition) {
  network::mojom::CookieManagerPtr cookie_manager;
  storage_partition->GetNetworkContext()->GetCookieManager(
      mojo::MakeRequest(&cookie_manager));

  base::RunLoop run_loop;
  cookie_manager->SetCanonicalCookie(
      net::CanonicalCookie(kTestCookieName, kTestCookieValue, kTestCookieHost,
                           "/", base::Time(), base::Time(), base::Time(), false,
                           false, net::CookieSameSite::NO_RESTRICTION,
                           net::COOKIE_PRIORITY_MEDIUM),
      false, false,
      base::Bind(&InjectCookieDoneCallback, run_loop.QuitClosure()));
  run_loop.Run();
}

void GetAllCookiesCallback(std::string* cookies_out,
                           base::OnceClosure done_closure,
                           const std::vector<net::CanonicalCookie>& cookies) {
  *cookies_out = net::CanonicalCookie::BuildCookieLine(cookies);
  std::move(done_closure).Run();
}

// Returns all cookies present in |storage_partition| as a HTTP header cookie
// line. Will be an empty string if there are no cookies.
std::string GetAllCookies(content::StoragePartition* storage_partition) {
  network::mojom::CookieManagerPtr cookie_manager;
  storage_partition->GetNetworkContext()->GetCookieManager(
      mojo::MakeRequest(&cookie_manager));

  std::string cookies;
  base::RunLoop run_loop;
  cookie_manager->GetAllCookies(
      base::BindOnce(&GetAllCookiesCallback, &cookies, run_loop.QuitClosure()));
  run_loop.Run();
  return cookies;
}

void PolicyChangedCallback(base::RepeatingClosure callback,
                           const base::Value* old_value,
                           const base::Value* new_value) {
  callback.Run();
}

// Spins the loop until a notification is received from |prefs| that the value
// of |pref_name| has changed. If the notification is received before Wait()
// has been called, Wait() returns immediately and no loop is spun.
class PrefChangeWatcher {
 public:
  PrefChangeWatcher(const std::string& pref_name, PrefService* prefs);

  void Wait();

 private:
  void OnPrefChange();

  bool pref_changed_ = false;

  base::RunLoop run_loop_;
  PrefChangeRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PrefChangeWatcher);
};

PrefChangeWatcher::PrefChangeWatcher(const std::string& pref_name,
                                     PrefService* prefs) {
  registrar_.Init(prefs);
  registrar_.Add(pref_name, base::Bind(&PrefChangeWatcher::OnPrefChange,
                                       base::Unretained(this)));
}

void PrefChangeWatcher::Wait() {
  if (!pref_changed_)
    run_loop_.Run();
}

void PrefChangeWatcher::OnPrefChange() {
  pref_changed_ = true;
  run_loop_.Quit();
}

}  // namespace

class WebviewLoginTest : public OobeBaseTest {
 public:
  WebviewLoginTest() {}
  ~WebviewLoginTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
    command_line->AppendSwitch(::switches::kUseFakeDeviceForMediaStream);
    command_line->AppendSwitch(switches::kStubCrosSettings);
    OobeBaseTest::SetUpCommandLine(command_line);
  }

 protected:
  void ClickNext() {
    ExecuteJsInSigninFrame("document.getElementById('nextButton').click();");
  }

  void ExpectIdentifierPage() {
    // First page: no back button, no close button, refresh button, #identifier
    // input field.
    JsExpect("!$('gaia-navigation').backVisible");
    JsExpect("!$('gaia-navigation').closeVisible");
    JsExpect("$('gaia-navigation').refreshVisible");
    JsExpect("$('signin-frame').src.indexOf('#identifier') != -1");
  }

  void ExpectPasswordPage() {
    // Second page: back button, close button, no refresh button,
    // #challengepassword input field.
    JsExpect("$('gaia-navigation').backVisible");
    JsExpect("$('gaia-navigation').closeVisible");
    JsExpect("!$('gaia-navigation').refreshVisible");
    JsExpect("$('signin-frame').src.indexOf('#challengepassword') != -1");
  }

  bool WebViewVisited(content::BrowserContext* browser_context,
                      content::StoragePartition* expected_storage_partition,
                      bool* out_web_view_found,
                      content::WebContents* guest_contents) {
    content::StoragePartition* guest_storage_partition =
        content::BrowserContext::GetStoragePartition(
            browser_context, guest_contents->GetSiteInstance());
    if (guest_storage_partition == expected_storage_partition)
      *out_web_view_found = true;

    // Returns true if found - this will exit the iteration early.
    return *out_web_view_found;
  }

  // Returns true if a webview which has a WebContents associated with
  // |storage_partition| currently exists in the login UI's main WebContents.
  bool IsLoginScreenHasWebviewWithStoragePartition(
      content::StoragePartition* storage_partition) {
    bool web_view_found = false;

    content::WebContents* web_contents = GetLoginUI()->GetWebContents();
    content::BrowserContext* browser_context =
        web_contents->GetBrowserContext();
    guest_view::GuestViewManager* guest_view_manager =
        guest_view::GuestViewManager::FromBrowserContext(browser_context);
    guest_view_manager->ForEachGuest(
        web_contents,
        base::BindRepeating(&WebviewLoginTest::WebViewVisited,
                            base::Unretained(this), browser_context,
                            storage_partition, &web_view_found));

    return web_view_found;
  }

 protected:
  ScopedCrosSettingsTestHelper settings_helper_{
      /* create_settings_service= */ false};

 private:
  DISALLOW_COPY_AND_ASSIGN(WebviewLoginTest);
};

// Basic signin with username and password.
IN_PROC_BROWSER_TEST_F(WebviewLoginTest, Basic) {
  WaitForGaiaPageLoadAndPropertyUpdate();

  ExpectIdentifierPage();

  SetSignFormField("identifier", OobeBaseTest::kFakeUserEmail);
  ClickNext();
  WaitForGaiaPageBackButtonUpdate();
  ExpectPasswordPage();

  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());

  SetSignFormField("services", "[]");
  SetSignFormField("password", OobeBaseTest::kFakeUserPassword);
  ClickNext();

  session_start_waiter.Wait();
}

// Fails: http://crbug.com/512648.
IN_PROC_BROWSER_TEST_F(WebviewLoginTest, DISABLED_BackButton) {
  WaitForGaiaPageLoadAndPropertyUpdate();

  // Start with identifer page.
  ExpectIdentifierPage();

  // Move to password page.
  SetSignFormField("identifier", OobeBaseTest::kFakeUserEmail);
  ClickNext();
  WaitForGaiaPageBackButtonUpdate();
  ExpectPasswordPage();

  // Click back to identifier page.
  JS().Evaluate("$('gaia-navigation').$.backButton.click();");
  WaitForGaiaPageBackButtonUpdate();
  ExpectIdentifierPage();

  // Click next to password page, user id is remembered.
  ClickNext();
  WaitForGaiaPageBackButtonUpdate();
  ExpectPasswordPage();

  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());

  // Finish sign-up.
  SetSignFormField("services", "[]");
  SetSignFormField("password", OobeBaseTest::kFakeUserPassword);
  ClickNext();

  session_start_waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(WebviewLoginTest, AllowGuest) {
  WaitForGaiaPageLoad();
  JsExpect("!$('guest-user-header-bar-item').hidden");
  settings_helper_.SetBoolean(kAccountsPrefAllowGuest, false);
  JsExpect("$('guest-user-header-bar-item').hidden");
}

// Create new account option should be available only if the settings allow it.
IN_PROC_BROWSER_TEST_F(WebviewLoginTest, AllowNewUser) {
  WaitForGaiaPageLoad();

  std::string frame_url = "$('gaia-signin').gaiaAuthHost_.reloadUrl_";
  // New users are allowed.
  JsExpect(frame_url + ".search('flow=nosignup') == -1");

  // Disallow new users - we also need to set a whitelist due to weird logic.
  settings_helper_.Set(kAccountsPrefUsers, base::ListValue());
  settings_helper_.SetBoolean(kAccountsPrefAllowNewUser, false);
  WaitForGaiaPageReload();

  // flow=nosignup indicates that user creation is not allowed.
  JsExpect(frame_url + ".search('flow=nosignup') != -1");
}

IN_PROC_BROWSER_TEST_F(WebviewLoginTest, EmailPrefill) {
  WaitForGaiaPageLoad();
  JS().ExecuteAsync("Oobe.showSigninUI('user@example.com')");
  WaitForGaiaPageReload();
  EXPECT_EQ(fake_gaia_->prefilled_email(), "user@example.com");
}

IN_PROC_BROWSER_TEST_F(WebviewLoginTest, StoragePartitionHandling) {
  WaitForGaiaPageLoadAndPropertyUpdate();

  // Start with identifer page.
  ExpectIdentifierPage();

  // WebContents of the embedding frame
  content::WebContents* web_contents = GetLoginUI()->GetWebContents();
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();

  std::string signin_frame_partition_name_1 =
      JS().GetString("$('signin-frame').partition");
  content::StoragePartition* signin_frame_partition_1 =
      login::GetSigninPartition();

  EXPECT_FALSE(signin_frame_partition_name_1.empty());
  EXPECT_EQ(login::SigninPartitionManager::Factory::GetForBrowserContext(
                browser_context)
                ->GetCurrentStoragePartitionName(),
            signin_frame_partition_name_1);
  EXPECT_TRUE(
      IsLoginScreenHasWebviewWithStoragePartition(signin_frame_partition_1));
  // Inject a cookie into the currently used StoragePartition, so we can test
  // later if it has been cleared.
  InjectCookie(signin_frame_partition_1);

  // Press the back button at a sign-in screen without pre-existing users to
  // start a new sign-in attempt.
  JS().Evaluate("$('signin-back-button').fire('tap')");
  WaitForGaiaPageBackButtonUpdate();
  // Expect that we got back to the identifier page, as there are no known users
  // so the sign-in screen will not display user pods.
  ExpectIdentifierPage();

  std::string signin_frame_partition_name_2 =
      JS().GetString("$('signin-frame').partition");
  content::StoragePartition* signin_frame_partition_2 =
      login::GetSigninPartition();

  EXPECT_FALSE(signin_frame_partition_name_2.empty());
  EXPECT_EQ(login::SigninPartitionManager::Factory::GetForBrowserContext(
                browser_context)
                ->GetCurrentStoragePartitionName(),
            signin_frame_partition_name_2);
  EXPECT_TRUE(
      IsLoginScreenHasWebviewWithStoragePartition(signin_frame_partition_2));
  InjectCookie(signin_frame_partition_2);

  // Make sure that the partitions differ and that the old one is not in use
  // anymore.
  EXPECT_NE(signin_frame_partition_name_1, signin_frame_partition_name_2);
  EXPECT_NE(signin_frame_partition_1, signin_frame_partition_2);
  EXPECT_FALSE(
      IsLoginScreenHasWebviewWithStoragePartition(signin_frame_partition_1));

  // The StoragePartition which is not in use is supposed to have been cleared.
  EXPECT_EQ("", GetAllCookies(signin_frame_partition_1));
  EXPECT_NE("", GetAllCookies(signin_frame_partition_2));
}

// Tests that requesting webcam access from the login screen works correctly.
// This is needed for taking profile pictures.
IN_PROC_BROWSER_TEST_F(WebviewLoginTest, RequestCamera) {
  WaitForGaiaPageLoad();

  // Video devices should be allowed from the login screen.
  content::WebContents* web_contents = GetLoginUI()->GetWebContents();
  bool getUserMediaSuccess = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents->GetMainFrame(),
      "navigator.getUserMedia("
      "    {video: true},"
      "    function() { window.domAutomationController.send(true); },"
      "    function() { window.domAutomationController.send(false); });",
      &getUserMediaSuccess));
  EXPECT_TRUE(getUserMediaSuccess);

  // Audio devices should be denied from the login screen.
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents->GetMainFrame(),
      "navigator.getUserMedia("
      "    {audio: true},"
      "    function() { window.domAutomationController.send(true); },"
      "    function() { window.domAutomationController.send(false); });",
      &getUserMediaSuccess));
  EXPECT_FALSE(getUserMediaSuccess);
}

class WebviewClientCertsLoginTest : public WebviewLoginTest {
 public:
  WebviewClientCertsLoginTest() {}

  // Installs a testing system slot and imports a client certificate into it.
  void SetUpClientCertInSystemSlot() {
    {
      bool system_slot_constructed_successfully = false;
      base::RunLoop loop;
      base::PostTaskWithTraitsAndReply(
          FROM_HERE, {content::BrowserThread::IO},
          base::BindOnce(&WebviewClientCertsLoginTest::SetUpTestSystemSlotOnIO,
                         base::Unretained(this),
                         &system_slot_constructed_successfully),
          loop.QuitClosure());
      loop.Run();
      ASSERT_TRUE(system_slot_constructed_successfully);
    }

    // Import a second client cert signed by another CA than client_1 into the
    // system wide key slot.
    client_cert_ = net::ImportClientCertAndKeyFromFile(
        net::GetTestCertsDirectory(), "client_1.pem", "client_1.pk8",
        test_system_slot_->slot());
    ASSERT_TRUE(client_cert_.get());
  }

  // Sets up the DeviceLoginScreenAutoSelectCertificateForUrls policy.
  void SetAutoSelectCertificatePatterns(
      const std::vector<std::string>& autoselect_patterns) {
    em::ChromeDeviceSettingsProto& proto(
        device_policy_test_helper_.device_policy()->payload());
    auto* field =
        proto.mutable_device_login_screen_auto_select_certificate_for_urls();
    for (const std::string& autoselect_pattern : autoselect_patterns)
      field->add_login_screen_auto_select_certificate_rules(autoselect_pattern);

    device_policy_test_helper_.device_policy()->Build();

    fake_session_manager_client_->set_device_policy(
        device_policy_test_helper_.device_policy()->GetBlob());
    PrefChangeWatcher watcher(prefs::kManagedAutoSelectCertificateForUrls,
                              ProfileHelper::GetSigninProfile()->GetPrefs());
    fake_session_manager_client_->OnPropertyChangeComplete(true);

    watcher.Wait();
  }

  // Adds the certificate from |authority_file_path| (PEM) as untrusted
  // authority in device OpenNetworkConfiguration policy.
  void SetIntermediateAuthorityInDeviceOncPolicy(
      const base::FilePath& authority_file_path) {
    std::string x509_contents;
    ASSERT_TRUE(base::ReadFileToString(authority_file_path, &x509_contents));
    base::DictionaryValue onc_dict =
        BuildDeviceOncDictForUntrustedAuthority(x509_contents);

    em::ChromeDeviceSettingsProto& proto(
        device_policy_test_helper_.device_policy()->payload());
    base::JSONWriter::Write(onc_dict,
                            proto.mutable_open_network_configuration()
                                ->mutable_open_network_configuration());

    device_policy_test_helper_.device_policy()->Build();

    fake_session_manager_client_->set_device_policy(
        device_policy_test_helper_.device_policy()->GetBlob());
    PrefChangeWatcher watcher(onc::prefs::kDeviceOpenNetworkConfiguration,
                              g_browser_process->local_state());
    fake_session_manager_client_->OnPropertyChangeComplete(true);
    watcher.Wait();
  }

  // Starts the Test HTTPS server with |ssl_options|.
  void StartHttpsServer(const net::SpawnedTestServer::SSLOptions& ssl_options) {
    https_server_ = std::make_unique<net::SpawnedTestServer>(
        net::SpawnedTestServer::TYPE_HTTPS, ssl_options, base::FilePath());
    ASSERT_TRUE(https_server_->Start());
  }

  // Requests |http_server_|'s client-cert test page in the webview with the id
  // |webview_id|.  Returns the content of the client-cert test page.  If
  // |watch_new_webcontents| is true, this function will watch for newly-created
  // WebContents when determining if the navigation to the test page has
  // finished. This can be used for webviews which have not navigated yet, as
  // their WebContents will be created on-demand.
  std::string RequestClientCertTestPageInFrame(std::string webview_id,
                                               bool watch_new_webcontents) {
    const GURL url = https_server_->GetURL("client-cert");
    content::TestNavigationObserver navigation_observer(url);
    if (watch_new_webcontents)
      navigation_observer.StartWatchingNewWebContents();
    else
      navigation_observer.WatchExistingWebContents();

    JS().Evaluate(base::StringPrintf("$('%s').src='%s'", webview_id.c_str(),
                                     url.spec().c_str()));

    navigation_observer.Wait();

    content::WebContents* main_web_contents = GetLoginUI()->GetWebContents();
    content::WebContents* frame_web_contents =
        signin::GetAuthFrameWebContents(main_web_contents, webview_id);
    test::JSChecker frame_js_checker(frame_web_contents);
    const std::string https_reply_content =
        frame_js_checker.GetString("document.body.textContent");

    return https_reply_content;
  }

  void ShowEulaScreen() {
    LoginDisplayHost::default_host()->StartWizard(OobeScreen::SCREEN_OOBE_EULA);
    OobeScreenWaiter(OobeScreen::SCREEN_OOBE_EULA).Wait();
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kDisableSigninFrameClientCertUserSelection);
    WebviewLoginTest::SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    auto fake_session_manager_client =
        std::make_unique<FakeSessionManagerClient>();
    fake_session_manager_client_ = fake_session_manager_client.get();
    DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::move(fake_session_manager_client));
    device_policy_test_helper_.InstallOwnerKey();
    device_policy_test_helper_.MarkAsEnterpriseOwned();

    fake_session_manager_client_->set_device_policy(
        device_policy_test_helper_.device_policy()->GetBlob());

    WebviewLoginTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownOnMainThread() override { TearDownTestSystemSlot(); }

 private:
  void SetUpTestSystemSlotOnIO(bool* out_system_slot_constructed_successfully) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    test_system_slot_ = std::make_unique<crypto::ScopedTestSystemNSSKeySlot>();
    *out_system_slot_constructed_successfully =
        test_system_slot_->ConstructedSuccessfully();
  }

  void TearDownTestSystemSlot() {
    if (!test_system_slot_)
      return;

    base::RunLoop loop;
    base::PostTaskWithTraitsAndReply(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&WebviewClientCertsLoginTest::TearDownTestSystemSlotOnIO,
                       base::Unretained(this)),
        loop.QuitClosure());
    loop.Run();
  }

  void TearDownTestSystemSlotOnIO() { test_system_slot_.reset(); }

  // Builds a device ONC dictionary defining a single untrusted authority
  // certificate.
  base::DictionaryValue BuildDeviceOncDictForUntrustedAuthority(
      const std::string& x509_authority_cert) {
    base::DictionaryValue onc_certificate;
    onc_certificate.SetKey(onc::certificate::kGUID, base::Value(kTestGuid));
    onc_certificate.SetKey(onc::certificate::kType,
                           base::Value(onc::certificate::kAuthority));
    onc_certificate.SetKey(onc::certificate::kX509,
                           base::Value(x509_authority_cert));

    base::ListValue onc_certificates;
    onc_certificates.GetList().emplace_back(std::move(onc_certificate));

    base::DictionaryValue onc_dict;
    onc_dict.SetKey(onc::toplevel_config::kCertificates,
                    std::move(onc_certificates));
    onc_dict.SetKey(
        onc::toplevel_config::kType,
        base::Value(onc::toplevel_config::kUnencryptedConfiguration));
    return onc_dict;
  }

  policy::DevicePolicyCrosTestHelper device_policy_test_helper_;
  // Unowned pointer - owned by DBusThreadManager.
  FakeSessionManagerClient* fake_session_manager_client_;
  std::unique_ptr<crypto::ScopedTestSystemNSSKeySlot> test_system_slot_;
  scoped_refptr<net::X509Certificate> client_cert_;
  std::unique_ptr<net::SpawnedTestServer> https_server_;

  DISALLOW_COPY_AND_ASSIGN(WebviewClientCertsLoginTest);
};

// Test that client certificate authentication using certificates from the
// system slot is enabled in the sign-in frame. The server does not request
// certificates signed by a specific authority.
//
// Disabled due to flaky timeouts: https://crbug.com/830337.
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_SigninFrameNoAuthorityGiven DISABLED_SigninFrameNoAuthorityGiven
#else
#define MAYBE_SigninFrameNoAuthorityGiven SigninFrameNoAuthorityGiven
#endif
IN_PROC_BROWSER_TEST_F(WebviewClientCertsLoginTest,
                       MAYBE_SigninFrameNoAuthorityGiven) {
  ASSERT_NO_FATAL_FAILURE(SetUpClientCertInSystemSlot());
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  ASSERT_NO_FATAL_FAILURE(StartHttpsServer(ssl_options));

  const std::vector<std::string> autoselect_patterns = {
      R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})"};
  SetAutoSelectCertificatePatterns(autoselect_patterns);

  WaitForGaiaPageLoad();

  std::string https_reply_content = RequestClientCertTestPageInFrame(
      gaia_frame_parent_, false /* watch_new_webcontents */);
  EXPECT_EQ(
      "got client cert with fingerprint: "
      "c66145f49caca4d1325db96ace0f12f615ba4981",
      https_reply_content);
}

// Test that client certificate autoselect selects the right certificate even
// with multiple filters for the same pattern.
//
// Disabled due to flaky timeouts: https://crbug.com/830337.
IN_PROC_BROWSER_TEST_F(WebviewClientCertsLoginTest,
                       DISABLED_SigninFrameCertMultipleFiltersAutoSelected) {
  ASSERT_NO_FATAL_FAILURE(SetUpClientCertInSystemSlot());
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  ASSERT_NO_FATAL_FAILURE(StartHttpsServer(ssl_options));

  const std::vector<std::string> autoselect_patterns = {
      R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})",
      R"({"pattern": "*", "filter": {"ISSUER": {"CN": "foo baz bar"}}})"};
  SetAutoSelectCertificatePatterns(autoselect_patterns);

  WaitForGaiaPageLoad();

  std::string https_reply_content = RequestClientCertTestPageInFrame(
      gaia_frame_parent_, false /* watch_new_webcontents */);
  EXPECT_EQ(
      "got client cert with fingerprint: "
      "c66145f49caca4d1325db96ace0f12f615ba4981",
      https_reply_content);
}

// Test that if no client certificate is auto-selected using policy on the
// sign-in frame, the client does not send up any client certificate.
//
// Disabled due to flaky timeouts: https://crbug.com/830337.
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_SigninFrameCertNotAutoSelected \
  DISABLED_SigninFrameCertNotAutoSelected
#else
#define MAYBE_SigninFrameCertNotAutoSelected SigninFrameCertNotAutoSelected
#endif
IN_PROC_BROWSER_TEST_F(WebviewClientCertsLoginTest,
                       MAYBE_SigninFrameCertNotAutoSelected) {
  ASSERT_NO_FATAL_FAILURE(SetUpClientCertInSystemSlot());
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  ASSERT_NO_FATAL_FAILURE(StartHttpsServer(ssl_options));

  WaitForGaiaPageLoad();

  std::string https_reply_content = RequestClientCertTestPageInFrame(
      gaia_frame_parent_, false /* watch_new_webcontents */);

  EXPECT_EQ("got no client cert", https_reply_content);
}

// Test that client certificate authentication using certificates from the
// system slot is enabled in the sign-in frame. The server requests
// a certificate signed by a specific authority.
//
// Disabled due to flaky timeouts: https://crbug.com/830337.
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_SigninFrameAuthorityGiven DISABLED_SigninFrameAuthorityGiven
#else
#define MAYBE_SigninFrameAuthorityGiven SigninFrameAuthorityGiven
#endif
IN_PROC_BROWSER_TEST_F(WebviewClientCertsLoginTest,
                       MAYBE_SigninFrameAuthorityGiven) {
  ASSERT_NO_FATAL_FAILURE(SetUpClientCertInSystemSlot());
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  base::FilePath ca_path =
      net::GetTestCertsDirectory().Append(FILE_PATH_LITERAL("client_1_ca.pem"));
  ssl_options.client_authorities.push_back(ca_path);
  ASSERT_NO_FATAL_FAILURE(StartHttpsServer(ssl_options));

  const std::vector<std::string> autoselect_patterns = {
      R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})"};
  SetAutoSelectCertificatePatterns(autoselect_patterns);

  WaitForGaiaPageLoad();

  std::string https_reply_content = RequestClientCertTestPageInFrame(
      gaia_frame_parent_, false /* watch_new_webcontents */);
  EXPECT_EQ(
      "got client cert with fingerprint: "
      "c66145f49caca4d1325db96ace0f12f615ba4981",
      https_reply_content);
}

// Test that client certificate authentication using certificates from the
// system slot is enabled in the sign-in frame. The server requests
// a certificate signed by a specific authority. The client doesn't have a
// matching certificate.
//
// Disabled due to flaky timeouts: https://crbug.com/830337.
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_SigninFrameAuthorityGivenNoMatchingCert \
  DISABLED_SigninFrameAuthorityGivenNoMatchingCert
#else
#define MAYBE_SigninFrameAuthorityGivenNoMatchingCert \
  SigninFrameAuthorityGivenNoMatchingCert
#endif
IN_PROC_BROWSER_TEST_F(WebviewClientCertsLoginTest,
                       MAYBE_SigninFrameAuthorityGivenNoMatchingCert) {
  ASSERT_NO_FATAL_FAILURE(SetUpClientCertInSystemSlot());
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  base::FilePath ca_path =
      net::GetTestCertsDirectory().Append(FILE_PATH_LITERAL("client_2_ca.pem"));
  ssl_options.client_authorities.push_back(ca_path);
  ASSERT_NO_FATAL_FAILURE(StartHttpsServer(ssl_options));

  const std::vector<std::string> autoselect_patterns = {
      R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})"};
  SetAutoSelectCertificatePatterns(autoselect_patterns);

  WaitForGaiaPageLoad();

  std::string https_reply_content = RequestClientCertTestPageInFrame(
      gaia_frame_parent_, false /* watch_new_webcontents */);
  EXPECT_EQ("got no client cert", https_reply_content);
}

// Test that client certificate will not be discovered if the server requests
// certificates signed by a root authority, the installed certificate has been
// issued by an intermediate authority, and the intermediate authority is not
// known on the device (it has not been made available through device ONC
// policy).
//
// Disabled due to flaky timeouts: https://crbug.com/830337.
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_SigninFrameIntermediateAuthorityUnknown \
  DISABLED_SigninFrameIntermediateAuthorityUnknown
#else
#define MAYBE_SigninFrameIntermediateAuthorityUnknown \
  SigninFrameIntermediateAuthorityUnknown
#endif
IN_PROC_BROWSER_TEST_F(WebviewClientCertsLoginTest,
                       MAYBE_SigninFrameIntermediateAuthorityUnknown) {
  ASSERT_NO_FATAL_FAILURE(SetUpClientCertInSystemSlot());
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  base::FilePath ca_path = net::GetTestCertsDirectory().Append(
      FILE_PATH_LITERAL("client_root_ca.pem"));
  ssl_options.client_authorities.push_back(ca_path);
  ASSERT_NO_FATAL_FAILURE(StartHttpsServer(ssl_options));

  const std::vector<std::string> autoselect_patterns = {
      R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})"};
  SetAutoSelectCertificatePatterns(autoselect_patterns);

  WaitForGaiaPageLoad();

  std::string https_reply_content = RequestClientCertTestPageInFrame(
      gaia_frame_parent_, false /* watch_new_webcontents */);
  EXPECT_EQ("got no client cert", https_reply_content);
}

// Test that client certificate will be discovered if the server requests
// certificates signed by a root authority, the installed certificate has been
// issued by an intermediate authority, and the intermediate authority is
// known on the device (it has been made available through device ONC policy).
//
// Disabled due to flaky timeouts: https://crbug.com/830337.
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_SigninFrameIntermediateAuthorityKnown \
  DISABLED_SigninFrameIntermediateAuthorityKnown
#else
#define MAYBE_SigninFrameIntermediateAuthorityKnown \
  SigninFrameIntermediateAuthorityKnown
#endif
IN_PROC_BROWSER_TEST_F(WebviewClientCertsLoginTest,
                       MAYBE_SigninFrameIntermediateAuthorityKnown) {
  ASSERT_NO_FATAL_FAILURE(SetUpClientCertInSystemSlot());
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  base::FilePath ca_path = net::GetTestCertsDirectory().Append(
      FILE_PATH_LITERAL("client_root_ca.pem"));
  ssl_options.client_authorities.push_back(ca_path);
  ASSERT_NO_FATAL_FAILURE(StartHttpsServer(ssl_options));

  const std::vector<std::string> autoselect_patterns = {
      R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})"};
  SetAutoSelectCertificatePatterns(autoselect_patterns);

  base::FilePath intermediate_ca_path =
      net::GetTestCertsDirectory().Append(FILE_PATH_LITERAL("client_1_ca.pem"));
  ASSERT_NO_FATAL_FAILURE(
      SetIntermediateAuthorityInDeviceOncPolicy(intermediate_ca_path));

  WaitForGaiaPageLoad();

  std::string https_reply_content = RequestClientCertTestPageInFrame(
      gaia_frame_parent_, false /* watch_new_webcontents */);
  EXPECT_EQ(
      "got client cert with fingerprint: "
      "c66145f49caca4d1325db96ace0f12f615ba4981",
      https_reply_content);
}

// Tests that client certificate authentication is not enabled in a webview on
// the sign-in screen which is not the sign-in frame. In this case, the EULA
// webview is used.
// TODO(pmarko): This is DISABLED because the eula UI it depends on has been
// deprecated and removed. https://crbug.com/849710.
IN_PROC_BROWSER_TEST_F(WebviewClientCertsLoginTest,
                       DISABLED_ClientCertRequestedInOtherWebView) {
  ASSERT_NO_FATAL_FAILURE(SetUpClientCertInSystemSlot());
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  ASSERT_NO_FATAL_FAILURE(StartHttpsServer(ssl_options));

  const std::vector<std::string> autoselect_patterns = {
      R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})"};
  SetAutoSelectCertificatePatterns(autoselect_patterns);

  ShowEulaScreen();

  // Use |watch_new_webcontents| because the EULA webview has not navigated yet.
  std::string https_reply_content = RequestClientCertTestPageInFrame(
      "cros-eula-frame", true /* watch_new_webcontents */);
  EXPECT_EQ("got no client cert", https_reply_content);
}

class WebviewProxyAuthLoginTest : public WebviewLoginTest {
 public:
  WebviewProxyAuthLoginTest()
      : auth_proxy_server_(std::make_unique<net::SpawnedTestServer>(
            net::SpawnedTestServer::TYPE_BASIC_AUTH_PROXY,
            base::FilePath())) {}

 protected:
  void SetUp() override {
    // Start proxy server
    auth_proxy_server_->set_redirect_connect_to_localhost(true);
    ASSERT_TRUE(auth_proxy_server_->Start());

    // Prepare device policy which will be used for two purposes:
    // - given to |fake_session_manager_client_|, so the device appears to have
    //   registered for policy.
    // - the payload is given to |policy_test_server_|, so we can download fresh
    //   policy.
    device_policy_test_helper_.device_policy()
        ->policy_data()
        .set_public_key_version(1);
    device_policy_test_helper_.device_policy()->Build();

    // Start policy server. Use the DMToken and DeviceId from PolicyBuilder.
    // These also used in |device_policy_test_helper_| and was passed to
    // |fake_session_manager_client_| above, so the device will request policy
    // with these identifiers.
    policy_test_server_.RegisterClient(policy::PolicyBuilder::kFakeToken,
                                       policy::PolicyBuilder::kFakeDeviceId);
    UpdateServedPolicyFromDevicePolicyTestHelper();
    ASSERT_TRUE(policy_test_server_.Start());

    WebviewLoginTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        ::switches::kProxyServer,
        auth_proxy_server_->host_port_pair().ToString());
    command_line->AppendSwitchASCII(policy::switches::kDeviceManagementUrl,
                                    policy_test_server_.GetServiceURL().spec());
    WebviewLoginTest::SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    WebviewLoginTest::SetUpInProcessBrowserTestFixture();

    // Use a fake SessionManagerClient to be able to pretend that the device has
    // been enrolled and registered for policy (and has a device DMToken).
    auto fake_session_manager_client =
        std::make_unique<FakeSessionManagerClient>();
    fake_session_manager_client_ = fake_session_manager_client.get();
    DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::move(fake_session_manager_client));
    device_policy_test_helper_.InstallOwnerKey();
    device_policy_test_helper_.MarkAsEnterpriseOwned();

    fake_session_manager_client_->set_device_policy(
        device_policy_builder()->GetBlob());

    // Set some fake state keys to make sure they are not empty.
    std::vector<std::string> state_keys;
    state_keys.push_back("1");
    fake_session_manager_client_->set_server_backed_state_keys(state_keys);
  }

  void SetUpOnMainThread() override {
    // Setup the observer reacting on NOTIFICATION_AUTH_NEEDED before the test
    // runs because there is no action we actively trigger to request proxy
    // authentication. Instead, the sign-in screen automatically shows the gaia
    // webview, which will request the gaia URL, which leads to a login prompt.
    auth_needed_wait_loop_ = std::make_unique<base::RunLoop>();
    auth_needed_observer_ =
        std::make_unique<content::WindowedNotificationObserver>(
            chrome::NOTIFICATION_AUTH_NEEDED,
            base::BindRepeating(&WebviewProxyAuthLoginTest::OnAuthRequested,
                                base::Unretained(this)));

    WebviewLoginTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    WebviewLoginTest::TearDownOnMainThread();

    auth_needed_observer_.reset();
    auth_needed_wait_loop_.reset();
  }

  bool OnAuthRequested(const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    // Only care for notifications originating from the frame which is
    // displaying gaia.
    content::WebContents* main_web_contents = GetLoginUI()->GetWebContents();
    content::WebContents* gaia_frame_web_contents =
        signin::GetAuthFrameWebContents(main_web_contents, gaia_frame_parent_);
    LoginHandler* login_handler =
        content::Details<LoginNotificationDetails>(details)->handler();
    if (login_handler->GetWebContentsForLogin() != gaia_frame_web_contents)
      return false;

    gaia_frame_login_handler_ = login_handler;
    auth_needed_wait_loop_->Quit();
    return true;
  }

  // Waits until proxy authentication has been requested by the frame displaying
  // gaia. Returns the LoginHandler handling this authentication request.
  LoginHandler* WaitForAuthRequested() {
    auth_needed_wait_loop_->Run();
    return gaia_frame_login_handler_;
  }

  void UpdateServedPolicyFromDevicePolicyTestHelper() {
    policy_test_server_.UpdatePolicy(
        policy::dm_protocol::kChromeDevicePolicyType,
        std::string() /* entity_id */,
        device_policy_builder()->payload().SerializeAsString());
  }

  policy::DevicePolicyBuilder* device_policy_builder() {
    return device_policy_test_helper_.device_policy();
  }

  content::WindowedNotificationObserver* auth_needed_observer() {
    return auth_needed_observer_.get();
  }

 private:
  std::unique_ptr<content::WindowedNotificationObserver> auth_needed_observer_;
  std::unique_ptr<base::RunLoop> auth_needed_wait_loop_;
  // Unowned pointer - set to the LoginHandler of the frame displaying gaia.
  LoginHandler* gaia_frame_login_handler_ = nullptr;

  // A proxy server which requires authentication using the 'Basic'
  // authentication method.
  std::unique_ptr<net::SpawnedTestServer> auth_proxy_server_;
  policy::LocalPolicyTestServer policy_test_server_;
  policy::DevicePolicyCrosTestHelper device_policy_test_helper_;

  // FakeDBusThreadManager uses FakeSessionManagerClient.
  std::unique_ptr<chromeos::DBusThreadManagerSetter> dbus_setter_;
  // Unowned pointer - owned by DBusThreadManager.
  chromeos::FakeSessionManagerClient* fake_session_manager_client_;

  DISALLOW_COPY_AND_ASSIGN(WebviewProxyAuthLoginTest);
};

// Disabled fails on msan and also non-msan bots: https://crbug.com/849128.
IN_PROC_BROWSER_TEST_F(WebviewProxyAuthLoginTest, DISABLED_ProxyAuthTransfer) {
  WaitForSigninScreen();

  LoginHandler* login_handler = WaitForAuthRequested();

  // Before entering auth data, make |policy_test_server_| serve a policy that
  // we can use to detect if policies have been fetched.
  em::ChromeDeviceSettingsProto& device_policy =
      device_policy_builder()->payload();
  device_policy.mutable_device_login_screen_auto_select_certificate_for_urls()
      ->add_login_screen_auto_select_certificate_rules("test_pattern");
  UpdateServedPolicyFromDevicePolicyTestHelper();

  policy::PolicyChangeRegistrar policy_change_registrar(
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetPolicyService(),
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                              std::string() /* component_id */));

  // Now enter auth data
  login_handler->SetAuth(base::ASCIIToUTF16("foo"), base::ASCIIToUTF16("bar"));
  WaitForGaiaPageLoad();

  base::RunLoop run_loop;
  policy_change_registrar.Observe(
      policy::key::kDeviceLoginScreenAutoSelectCertificateForUrls,
      base::BindRepeating(&PolicyChangedCallback, run_loop.QuitClosure()));
  run_loop.Run();

  // Press the back button at a sign-in screen without pre-existing users to
  // start a new sign-in attempt.
  // This will re-load gaia, rotating the StoragePartition. The new
  // StoragePartition must also have the proxy auth details.
  JS().Evaluate("$('signin-back-button').fire('tap')");
  WaitForGaiaPageBackButtonUpdate();
  // Expect that we got back to the identifier page, as there are no known users
  // so the sign-in screen will not display user pods.
  ExpectIdentifierPage();
}
}  // namespace chromeos

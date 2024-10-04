// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/identity/gaia_remote_consent_flow.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/identity/identity_constants.h"
#include "chrome/browser/extensions/api/identity/identity_get_accounts_function.h"
#include "chrome/browser/extensions/api/identity/identity_get_auth_token_error.h"
#include "chrome/browser/extensions/api/identity/identity_get_auth_token_function.h"
#include "chrome/browser/extensions/api/identity/identity_get_profile_user_info_function.h"
#include "chrome/browser/extensions/api/identity/identity_launch_web_auth_flow_function.h"
#include "chrome/browser/extensions/api/identity/identity_remove_cached_auth_token_function.h"
#include "chrome/browser/extensions/api/identity/launch_web_auth_flow_delegate.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/identity.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/crx_file/id_util.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/guest_view_manager_factory.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/list_accounts_test_utils.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/api/oauth2.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/oauth2_manifest_handler.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"
#include "net/cookies/cookie_util.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/idle/idle.h"
#include "ui/base/idle/scoped_set_idle_state.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/net/network_portal_detector_test_impl.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/signin/signin_util.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#endif

using extensions::ExtensionsAPIClient;
using testing::_;
using testing::Return;

namespace extensions {

namespace {

namespace errors = identity_constants;
namespace utils = api_test_utils;

using api::oauth2::OAuth2Info;

const char kAccessToken[] = "auth_token";
const char kExtensionId[] = "ext_id";

const char kGetAuthTokenResultHistogramName[] =
    "Signin.Extensions.GetAuthTokenResult";
const char kGetAuthTokenResultAfterConsentApprovedHistogramName[] =
    "Signin.Extensions.GetAuthTokenResult.RemoteConsentApproved";

const char kLaunchWebAuthFlowResultHistogramName[] =
    "Signin.Extensions.LaunchWebAuthFlowResult";

#if BUILDFLAG(IS_CHROMEOS_ASH)
void InitNetwork() {
  const ash::NetworkState* default_network =
      ash::NetworkHandler::Get()->network_state_handler()->DefaultNetwork();

  auto* portal_detector = new ash::NetworkPortalDetectorTestImpl();
  portal_detector->SetDefaultNetworkForTesting(default_network->guid());

  ash::network_portal_detector::InitializeForTesting(portal_detector);
}
#endif

// Asynchronous function runner allows tests to manipulate the browser window
// after the call happens.
class AsyncFunctionRunner {
 public:
  void RunFunctionAsync(ExtensionFunction* function,
                        const std::string& args,
                        content::BrowserContext* browser_context) {
    response_delegate_ =
        std::make_unique<api_test_utils::SendResponseHelper>(function);
    function->preserve_results_for_testing();
    base::Value::List parsed_args = base::test::ParseJsonList(args);
    function->SetArgs(std::move(parsed_args));

    if (!function->extension()) {
      scoped_refptr<const Extension> empty_extension(
          ExtensionBuilder("Test").Build());
      function->set_extension(empty_extension.get());
    }

    dispatcher_ =
        std::make_unique<ExtensionFunctionDispatcher>(browser_context);
    function->SetDispatcher(dispatcher_->AsWeakPtr());

    function->set_has_callback(true);
    function->RunWithValidation().Execute();
  }

  std::string WaitForError(ExtensionFunction* function) {
    RunMessageLoopUntilResponse();
    CHECK(function->response_type());
    EXPECT_EQ(ExtensionFunction::FAILED, *function->response_type());
    return function->GetError();
  }

  void WaitForOneResult(ExtensionFunction* function, base::Value* result) {
    RunMessageLoopUntilResponse();
    EXPECT_TRUE(function->GetError().empty())
        << "Unexpected error: " << function->GetError();
    EXPECT_NE(nullptr, function->GetResultListForTest());

    const auto& result_list = *function->GetResultListForTest();
    EXPECT_EQ(1ul, result_list.size());

    *result = result_list[0].Clone();
  }

 private:
  void RunMessageLoopUntilResponse() {
    response_delegate_->WaitForResponse();
    EXPECT_TRUE(response_delegate_->has_response());
  }

  std::unique_ptr<api_test_utils::SendResponseHelper> response_delegate_;
  std::unique_ptr<ExtensionFunctionDispatcher> dispatcher_;
};

class AsyncExtensionBrowserTest : public ExtensionBrowserTest {
 protected:
  // Provide wrappers of AsynchronousFunctionRunner for convenience.
  void RunFunctionAsync(ExtensionFunction* function, const std::string& args) {
    async_function_runners_[function] = std::make_unique<AsyncFunctionRunner>();
    async_function_runners_[function]->RunFunctionAsync(function, args,
                                                        browser()->profile());
  }

  std::string WaitForError(ExtensionFunction* function) {
    return async_function_runners_[function]->WaitForError(function);
  }

  void WaitForOneResult(ExtensionFunction* function, base::Value* result) {
    async_function_runners_[function]->WaitForOneResult(function, result);
  }

 private:
  std::map<ExtensionFunction*, std::unique_ptr<AsyncFunctionRunner>>
      async_function_runners_;
};

class TestHangOAuth2MintTokenFlow : public OAuth2MintTokenFlow {
 public:
  TestHangOAuth2MintTokenFlow()
      : OAuth2MintTokenFlow(nullptr, OAuth2MintTokenFlow::Parameters()) {}

  void Start(scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
             const std::string& access_token) override {
    // Do nothing, simulating a hanging network call.
  }
};

class TestOAuth2MintTokenFlow : public OAuth2MintTokenFlow {
 public:
  enum ResultType {
    REMOTE_CONSENT_SUCCESS,
    MINT_TOKEN_SUCCESS,
    MINT_TOKEN_FAILURE,
    MINT_TOKEN_BAD_CREDENTIALS,
    MINT_TOKEN_SERVICE_ERROR
  };

  TestOAuth2MintTokenFlow(ResultType result,
                          const std::set<std::string>* requested_scopes,
                          const std::set<std::string>& granted_scopes,
                          OAuth2MintTokenFlow::Delegate* delegate)
      : OAuth2MintTokenFlow(delegate, OAuth2MintTokenFlow::Parameters()),
        result_(result),
        requested_scopes_(requested_scopes),
        granted_scopes_(granted_scopes),
        delegate_(delegate) {}

  void Start(scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
             const std::string& access_token) override {
    switch (result_) {
      case REMOTE_CONSENT_SUCCESS: {
        RemoteConsentResolutionData resolution_data;
        delegate_->OnRemoteConsentSuccess(resolution_data);
        break;
      }
      case MINT_TOKEN_SUCCESS: {
        OAuth2MintTokenFlow::MintTokenResult result;
        result.access_token = kAccessToken;
        result.time_to_live = base::Seconds(3600);
        // In these tests, empty `granted_scopes_` means that all requested
        // scopes should be granted.
        result.granted_scopes =
            !granted_scopes_.empty() ? granted_scopes_ : *requested_scopes_;
        delegate_->OnMintTokenSuccess(result);
        break;
      }
      case MINT_TOKEN_FAILURE: {
        GoogleServiceAuthError error(GoogleServiceAuthError::CONNECTION_FAILED);
        delegate_->OnMintTokenFailure(error);
        break;
      }
      case MINT_TOKEN_BAD_CREDENTIALS: {
        GoogleServiceAuthError error(
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
        delegate_->OnMintTokenFailure(error);
        break;
      }
      case MINT_TOKEN_SERVICE_ERROR: {
        GoogleServiceAuthError error =
            GoogleServiceAuthError::FromServiceError("invalid_scope");
        delegate_->OnMintTokenFailure(error);
        break;
      }
    }
  }

 private:
  ResultType result_;
  raw_ptr<const std::set<std::string>> requested_scopes_;
  std::set<std::string> granted_scopes_;
  raw_ptr<OAuth2MintTokenFlow::Delegate> delegate_;
};

std::unique_ptr<net::EmbeddedTestServer> LaunchHttpsServer() {
  std::unique_ptr<net::EmbeddedTestServer> https_server =
      std::make_unique<net::EmbeddedTestServer>(
          net::EmbeddedTestServer::TYPE_HTTPS);
  https_server->ServeFilesFromSourceDirectory(
      "chrome/test/data/extensions/api_test/identity");
  EXPECT_TRUE(https_server->Start());

  return https_server;
}

scoped_refptr<IdentityLaunchWebAuthFlowFunction>
CreateLaunchWebAuthFlowFunction() {
  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function(
      new IdentityLaunchWebAuthFlowFunction());
  scoped_refptr<const Extension> empty_extension(
      ExtensionBuilder("Test").Build());
  function->set_extension(empty_extension);

  return function;
}

// Simulate a redirects to the expected url from the Chrome extension API via
// `EvalJS`.
// Expecting it to work with
// "chrome/test/data/extensions/api_test/identity/consent_page.html" web
// page loaded in the `auth_web_contents`.
// `url_prefix` is used to determine the redirect link, it will match the
// pattern: "https://%s.chromiumapp.org/".
void SimulateUrlRedirect(const std::string& url_prefix,
                         content::WebContents* auth_web_contents) {
  ASSERT_EQ(nullptr, content::EvalJs(auth_web_contents,
                                     "apply_consent(\"" + url_prefix + "\");"));
}

// Similar to SimulateUrlRedirect, but uses provided url instead of the pattern
void SimulateCustomUrlRedirect(const std::string& redirect_url,
                               content::WebContents* auth_web_contents) {
  ASSERT_EQ(nullptr,
            content::EvalJs(auth_web_contents, "window.location.replace(\"" +
                                                   redirect_url + "\");"));
}

}  // namespace

class FakeGetAuthTokenFunction : public IdentityGetAuthTokenFunction {
 public:
  FakeGetAuthTokenFunction()
      : login_access_token_result_(true),
        auto_login_access_token_(true),
        login_ui_result_(true),
        scope_ui_result_(true),
        scope_ui_async_(false),
        scope_ui_failure_(GaiaRemoteConsentFlow::WINDOW_CLOSED),
        login_ui_shown_(false),
        scope_ui_shown_(false) {}

  void set_login_access_token_result(bool result) {
    login_access_token_result_ = result;
  }

  void set_auto_login_access_token(bool automatic) {
    auto_login_access_token_ = automatic;
  }

  void set_login_ui_result(bool result) { login_ui_result_ = result; }

  void push_mint_token_flow(std::unique_ptr<OAuth2MintTokenFlow> flow) {
    flow_queue_.push(std::move(flow));
  }

  void push_mint_token_result(
      TestOAuth2MintTokenFlow::ResultType result_type,
      const std::set<std::string>& granted_scopes = {}) {
    // If `granted_scopes` is empty, `TestOAuth2MintTokenFlow` returns the
    // requested scopes (retrieved from `token_key`) in a mint token success
    // flow by default. Since the scopes in `token_key` may be populated at a
    // later time, the requested scopes cannot be immediately copied, so a
    // pointer is passed instead.
    const ExtensionTokenKey* token_key = GetExtensionTokenKeyForTest();
    push_mint_token_flow(std::make_unique<TestOAuth2MintTokenFlow>(
        result_type, &token_key->scopes, granted_scopes, this));
  }

  // Sets scope UI to not complete immediately. Call
  // CompleteOAuthApprovalDialog() or CompleteRemoteConsentDialog() after
  // |on_scope_ui_shown| is invoked to unblock execution.
  void set_scope_ui_async(base::OnceClosure on_scope_ui_shown) {
    scope_ui_async_ = true;
    on_scope_ui_shown_ = std::move(on_scope_ui_shown);
  }

  void set_scope_ui_failure(GaiaRemoteConsentFlow::Failure failure) {
    scope_ui_result_ = false;
    scope_ui_failure_ = failure;
  }

  void set_remote_consent_gaia_id(const std::string& gaia_id) {
    remote_consent_gaia_id_ = gaia_id;
  }

  bool login_ui_shown() const { return login_ui_shown_; }

  bool scope_ui_shown() const { return scope_ui_shown_; }

  std::vector<std::string> login_access_tokens() const {
    return login_access_tokens_;
  }

  void StartTokenKeyAccountAccessTokenRequest() override {
    if (auto_login_access_token_) {
      std::optional<std::string> access_token("access_token");
      GoogleServiceAuthError error = GoogleServiceAuthError::AuthErrorNone();
      if (!login_access_token_result_) {
        access_token = std::nullopt;
        error = GoogleServiceAuthError(
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
      }
      OnGetAccessTokenComplete(access_token,
                               base::Time::Now() + base::Hours(1LL), error);
    } else {
      // Make a request to the IdentityManager. The test now must tell the
      // service to issue an access token (or an error).
      IdentityGetAuthTokenFunction::StartTokenKeyAccountAccessTokenRequest();
    }
  }

#if BUILDFLAG(IS_CHROMEOS)
  void StartDeviceAccessTokenRequest() override {
    // In these tests requests for the device account just funnel through to
    // requests for the token key account.
    StartTokenKeyAccountAccessTokenRequest();
  }
#endif

  // Fix auth error on secondary account or add a new account.
  void FixOrAddSecondaryAccount() {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(GetProfile());
    std::vector<CoreAccountInfo> accounts =
        identity_manager->GetAccountsWithRefreshTokens();
    CoreAccountId primary_id =
        identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
    bool fixed_auth_error = false;
    for (const auto& account_info : accounts) {
      CoreAccountId account_id = account_info.account_id;
      if (account_id == primary_id)
        continue;
      if (identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
              account_id)) {
        identity_manager->GetAccountsMutator()->AddOrUpdateAccount(
            account_info.gaia, account_info.email, "token",
            account_info.is_under_advanced_protection,
            signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
            signin_metrics::SourceForRefreshTokenOperation::kUnknown);
        fixed_auth_error = true;
      }
    }
    if (!fixed_auth_error) {
      if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
        // This is required to ensure the refresh token is available before
        // 'OnPrimaryAccountChanged()` is fired.
        AccountReconcilor::Lock reconcilor_lock(
            AccountReconcilorFactory::GetForProfile(GetProfile()));
        signin::MakeAccountAvailable(identity_manager, "primary@example.com");
        signin::SetPrimaryAccount(identity_manager, "primary@example.com",
                                  signin::ConsentLevel::kSignin);
      } else {
        signin::MakeAccountAvailable(identity_manager, "secondary@example.com");
      }
    }
  }

  // Simulate signin through a login prompt.
  void ShowExtensionLoginPrompt() override {
    EXPECT_FALSE(login_ui_shown_);
    login_ui_shown_ = true;
    if (login_ui_result_) {
      signin::IdentityManager* identity_manager =
          IdentityManagerFactory::GetForProfile(GetProfile());
      if (IdentityAPI::GetFactoryInstance()
              ->Get(GetProfile())
              ->AreExtensionsRestrictedToPrimaryAccount()) {
        // Set a primary account.
        ASSERT_FALSE(
            identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
        signin::MakeAccountAvailable(identity_manager, "primary@example.com");
        signin::SetPrimaryAccount(identity_manager, "primary@example.com",
                                  signin::ConsentLevel::kSignin);
      } else {
        FixOrAddSecondaryAccount();
      }
    } else {
      SigninFailed();
    }
  }

  void ShowRemoteConsentDialog(
      const RemoteConsentResolutionData& resolution_data) override {
    scope_ui_shown_ = true;
    if (!scope_ui_async_)
      CompleteRemoteConsentDialog();
    else
      std::move(on_scope_ui_shown_).Run();
  }

  void CompleteRemoteConsentDialog() {
    if (scope_ui_result_) {
      OnGaiaRemoteConsentFlowApproved("fake_consent_result",
                                      remote_consent_gaia_id_);
    } else {
      OnGaiaRemoteConsentFlowFailed(scope_ui_failure_);
    }
  }

  void StartGaiaRequest(const std::string& login_access_token) override {
    // Save the login token used in the mint token flow so tests can see
    // what account was used.
    login_access_tokens_.push_back(login_access_token);
    IdentityGetAuthTokenFunction::StartGaiaRequest(login_access_token);
  }

  std::unique_ptr<OAuth2MintTokenFlow> CreateMintTokenFlow() override {
    auto flow = std::move(flow_queue_.front());
    flow_queue_.pop();
    return flow;
  }

  bool enable_granular_permissions() const {
    return IdentityGetAuthTokenFunction::enable_granular_permissions();
  }

  std::string GetSelectedUserId() const {
    return IdentityGetAuthTokenFunction::GetSelectedUserId();
  }

 private:
  ~FakeGetAuthTokenFunction() override {}
  bool login_access_token_result_;
  bool auto_login_access_token_;
  bool login_ui_result_;
  bool scope_ui_result_;
  bool scope_ui_async_;
  base::OnceClosure on_scope_ui_shown_;
  GaiaRemoteConsentFlow::Failure scope_ui_failure_;
  bool login_ui_shown_;
  bool scope_ui_shown_;

  std::queue<std::unique_ptr<OAuth2MintTokenFlow>> flow_queue_;

  std::vector<std::string> login_access_tokens_;

  std::string remote_consent_gaia_id_;
};

class MockQueuedMintRequest : public IdentityMintRequestQueue::Request {
 public:
  MOCK_METHOD1(StartMintToken, void(IdentityMintRequestQueue::MintType));
};

class IdentityTestWithSignin : public AsyncExtensionBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    AsyncExtensionBrowserTest::SetUpInProcessBrowserTestFixture();

    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &IdentityTestWithSignin::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);

    ChromeSigninClientFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                     &test_url_loader_factory_));
  }

  void SetUpOnMainThread() override {
    AsyncExtensionBrowserTest::SetUpOnMainThread();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Fake the network online state so that Gaia requests can come through.
    InitNetwork();
#endif

    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());

    // This test requires these callbacks to be fired on account
    // update/removal.
    identity_test_env()->EnableRemovalOfExtendedAccountInfo();

    identity_test_env()->SetTestURLLoaderFactory(&test_url_loader_factory_);
  }

  void TearDownOnMainThread() override {
    // Must be destroyed before the Profile.
    identity_test_env_profile_adaptor_.reset();
  }

 protected:
  // Signs in (at sync consent level) and returns the account ID of the primary
  // account.
  CoreAccountId SignIn(const std::string& email) {
    auto account_info = identity_test_env()->MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSync);
    EXPECT_TRUE(identity_test_env()->identity_manager()->HasPrimaryAccount(
        signin::ConsentLevel::kSync));
    return account_info.account_id;
  }

  IdentityAPI* id_api() {
    return IdentityAPI::GetFactoryInstance()->Get(browser()->profile());
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  network::TestURLLoaderFactory test_url_loader_factory_;

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;

  base::CallbackListSubscription create_services_subscription_;
};

class IdentityGetAccountsFunctionTest : public IdentityTestWithSignin {
 public:
  IdentityGetAccountsFunctionTest() = default;

 protected:
  testing::AssertionResult ExpectGetAccounts(
      const std::vector<std::string>& gaia_ids) {
    scoped_refptr<IdentityGetAccountsFunction> func(
        new IdentityGetAccountsFunction);
    func->set_extension(
        ExtensionBuilder("Test").SetID(kExtensionId).Build().get());
    if (!utils::RunFunction(func.get(), std::string("[]"), browser()->profile(),
                            api_test_utils::FunctionMode::kNone)) {
      return GenerateFailureResult(gaia_ids, nullptr)
             << "getAccounts did not return a result.";
    }
    const base::Value::List* callback_arguments_list =
        func->GetResultListForTest();
    if (!callback_arguments_list)
      return GenerateFailureResult(gaia_ids, nullptr) << "NULL result";

    if (callback_arguments_list->size() != 1u) {
      return GenerateFailureResult(gaia_ids, nullptr)
             << "Expected 1 argument but got "
             << callback_arguments_list->size();
    }

    if (!(*callback_arguments_list)[0].is_list())
      GenerateFailureResult(gaia_ids, nullptr) << "Result was not an array";
    const base::Value::List& results = (*callback_arguments_list)[0].GetList();

    std::vector<std::string> result_ids;
    for (const base::Value& item : results) {
      std::optional<api::identity::AccountInfo> info =
          api::identity::AccountInfo::FromValue(item);
      if (info) {
        result_ids.push_back(info->id);
      } else {
        return GenerateFailureResult(gaia_ids, &results);
      }
    }

    if (result_ids != gaia_ids) {
      return GenerateFailureResult(gaia_ids, &results);
    }

    return testing::AssertionResult(true);
  }

  testing::AssertionResult GenerateFailureResult(
      const ::std::vector<std::string>& gaia_ids,
      const base::Value::List* results) {
    testing::Message msg("Expected: ");
    for (const std::string& gaia_id : gaia_ids) {
      msg << gaia_id << " ";
    }
    msg << "Actual: ";
    if (!results) {
      msg << "NULL";
    } else {
      for (const auto& result : *results) {
        std::optional<api::identity::AccountInfo> info =
            api::identity::AccountInfo::FromValue(result);
        if (info) {
          msg << info->id << " ";
        } else {
          msg << result << "<-" << result.type() << " ";
        }
      }
    }

    return testing::AssertionFailure(msg);
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
};

IN_PROC_BROWSER_TEST_F(IdentityGetAccountsFunctionTest, AllAccountsOn) {
  EXPECT_FALSE(id_api()->AreExtensionsRestrictedToPrimaryAccount());
}

IN_PROC_BROWSER_TEST_F(IdentityGetAccountsFunctionTest, NoneSignedIn) {
  EXPECT_TRUE(ExpectGetAccounts({}));
}

IN_PROC_BROWSER_TEST_F(IdentityGetAccountsFunctionTest, NoPrimaryAccount) {
  identity_test_env()->MakeAccountAvailable("secondary@example.com");
  EXPECT_TRUE(ExpectGetAccounts({}));
}

IN_PROC_BROWSER_TEST_F(IdentityGetAccountsFunctionTest,
                       PrimaryAccountHasInvalidRefreshToken) {
  CoreAccountId primary_account_id = SignIn("primary@example.com");
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();
  EXPECT_TRUE(ExpectGetAccounts({"gaia_id_for_primary_example.com"}));
}

IN_PROC_BROWSER_TEST_F(IdentityGetAccountsFunctionTest,
                       PrimaryAccountSignedIn) {
  SignIn("primary@example.com");
  EXPECT_TRUE(ExpectGetAccounts({"gaia_id_for_primary_example.com"}));
}

IN_PROC_BROWSER_TEST_F(IdentityGetAccountsFunctionTest, TwoAccountsSignedIn) {
  SignIn("primary@example.com");
  identity_test_env()->MakeAccountAvailable("secondary@example.com");
  if (!id_api()->AreExtensionsRestrictedToPrimaryAccount()) {
    EXPECT_TRUE(ExpectGetAccounts({"gaia_id_for_primary_example.com",
                                   "gaia_id_for_secondary_example.com"}));
  } else {
    EXPECT_TRUE(ExpectGetAccounts({"gaia_id_for_primary_example.com"}));
  }
}

IN_PROC_BROWSER_TEST_F(IdentityGetAccountsFunctionTest, SignedInOnTheWeb) {
  identity_test_env()->MakeAccountAvailable("secondary@example.com");
  EXPECT_TRUE(ExpectGetAccounts({}));

  SignIn("primary@example.com");
  EXPECT_TRUE(ExpectGetAccounts({"gaia_id_for_primary_example.com",
                                 "gaia_id_for_secondary_example.com"}));
}

class IdentityGetProfileUserInfoFunctionTest : public IdentityTestWithSignin {
 protected:
  std::optional<api::identity::ProfileUserInfo> RunGetProfileUserInfo() {
    scoped_refptr<IdentityGetProfileUserInfoFunction> func(
        new IdentityGetProfileUserInfoFunction);
    func->set_extension(
        ExtensionBuilder("Test").SetID(kExtensionId).Build().get());
    std::optional<base::Value> value = utils::RunFunctionAndReturnSingleResult(
        func.get(), "[]", browser()->profile());
    return api::identity::ProfileUserInfo::FromValue(*value);
  }

  std::optional<api::identity::ProfileUserInfo>
  RunGetProfileUserInfoWithEmail() {
    scoped_refptr<IdentityGetProfileUserInfoFunction> func(
        new IdentityGetProfileUserInfoFunction);
    func->set_extension(CreateExtensionWithEmailPermission());
    std::optional<base::Value> value = utils::RunFunctionAndReturnSingleResult(
        func.get(), "[]", browser()->profile());
    return api::identity::ProfileUserInfo::FromValue(*value);
  }

  scoped_refptr<const Extension> CreateExtensionWithEmailPermission() {
    return ExtensionBuilder("Test").AddAPIPermission("identity.email").Build();
  }
};

IN_PROC_BROWSER_TEST_F(IdentityGetProfileUserInfoFunctionTest, NotSignedIn) {
  std::optional<api::identity::ProfileUserInfo> info =
      RunGetProfileUserInfoWithEmail();
  EXPECT_TRUE(info->email.empty());
  EXPECT_TRUE(info->id.empty());
}

IN_PROC_BROWSER_TEST_F(IdentityGetProfileUserInfoFunctionTest, SignedIn) {
  SignIn("president@example.com");
  std::optional<api::identity::ProfileUserInfo> info =
      RunGetProfileUserInfoWithEmail();
  EXPECT_EQ("president@example.com", info->email);
  EXPECT_EQ("gaia_id_for_president_example.com", info->id);
}

IN_PROC_BROWSER_TEST_F(IdentityGetProfileUserInfoFunctionTest,
                       SignedInUnconsented) {
  identity_test_env()->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  std::optional<api::identity::ProfileUserInfo> info =
      RunGetProfileUserInfoWithEmail();
  EXPECT_TRUE(info->email.empty());
  EXPECT_TRUE(info->id.empty());
}

IN_PROC_BROWSER_TEST_F(IdentityGetProfileUserInfoFunctionTest,
                       NotSignedInNoEmail) {
  std::optional<api::identity::ProfileUserInfo> info = RunGetProfileUserInfo();
  EXPECT_TRUE(info->email.empty());
  EXPECT_TRUE(info->id.empty());
}

IN_PROC_BROWSER_TEST_F(IdentityGetProfileUserInfoFunctionTest,
                       SignedInNoEmail) {
  SignIn("president@example.com");
  std::optional<api::identity::ProfileUserInfo> info = RunGetProfileUserInfo();
  EXPECT_TRUE(info->email.empty());
  EXPECT_TRUE(info->id.empty());
}

class IdentityGetProfileUserInfoFunctionTestWithAccountStatusParam
    : public IdentityGetProfileUserInfoFunctionTest,
      public ::testing::WithParamInterface<std::string> {
 protected:
  std::optional<api::identity::ProfileUserInfo>
  RunGetProfileUserInfoWithAccountStatus() {
    scoped_refptr<IdentityGetProfileUserInfoFunction> func(
        new IdentityGetProfileUserInfoFunction);
    func->set_extension(CreateExtensionWithEmailPermission());
    std::string args = base::StringPrintf(R"([{"accountStatus": "%s"}])",
                                          account_status().c_str());
    std::optional<base::Value> value = utils::RunFunctionAndReturnSingleResult(
        func.get(), args, browser()->profile());
    return api::identity::ProfileUserInfo::FromValue(*value);
  }

  std::string account_status() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    IdentityGetProfileUserInfoFunctionTestWithAccountStatusParam,
    ::testing::Values("SYNC", "ANY"));

IN_PROC_BROWSER_TEST_P(
    IdentityGetProfileUserInfoFunctionTestWithAccountStatusParam,
    NotSignedIn) {
  std::optional<api::identity::ProfileUserInfo> info =
      RunGetProfileUserInfoWithAccountStatus();
  EXPECT_TRUE(info->email.empty());
  EXPECT_TRUE(info->id.empty());
}

IN_PROC_BROWSER_TEST_P(
    IdentityGetProfileUserInfoFunctionTestWithAccountStatusParam,
    SignedIn) {
  SignIn("test@example.com");
  std::optional<api::identity::ProfileUserInfo> info =
      RunGetProfileUserInfoWithAccountStatus();
  EXPECT_EQ("test@example.com", info->email);
  EXPECT_EQ("gaia_id_for_test_example.com", info->id);
}

IN_PROC_BROWSER_TEST_P(
    IdentityGetProfileUserInfoFunctionTestWithAccountStatusParam,
    SignedInUnconsented) {
  identity_test_env()->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  std::optional<api::identity::ProfileUserInfo> info =
      RunGetProfileUserInfoWithAccountStatus();
  // The unconsented (Sync off) primary account is returned conditionally,
  // depending on the accountStatus parameter.
  if (account_status() == "ANY") {
    EXPECT_EQ("test@example.com", info->email);
    EXPECT_EQ("gaia_id_for_test_example.com", info->id);
  } else {
    // accountStatus is SYNC or unspecified.
    EXPECT_TRUE(info->email.empty());
    EXPECT_TRUE(info->id.empty());
  }
}

class GetAuthTokenFunctionTest
    : public IdentityTestWithSignin,
      public signin::IdentityManager::DiagnosticsObserver {
 public:
  GetAuthTokenFunctionTest() { SetUserGestureEnabled(true); }

  ~GetAuthTokenFunctionTest() override = default;

  std::string IssueLoginAccessTokenForAccount(const CoreAccountId& account_id) {
    std::string access_token = "access_token-" + account_id.ToString();
    identity_test_env()
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            account_id, access_token, base::Time::Now() + base::Seconds(3600));
    return access_token;
  }

 protected:
  enum OAuth2Fields { NONE = 0, CLIENT_ID = 1, SCOPES = 2, AS_COMPONENT = 4 };

  void SetUpOnMainThread() override {
    IdentityTestWithSignin::SetUpOnMainThread();
    identity_test_env()->identity_manager()->AddDiagnosticsObserver(this);
    signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);
  }

  void TearDownOnMainThread() override {
    if (!identity_test_env_profile_adaptor_) {
      // In some tests, we have released the profile early and removed the
      // observer, so do nothing
      return;
    }
    identity_test_env()->identity_manager()->RemoveDiagnosticsObserver(this);
    IdentityTestWithSignin::TearDownOnMainThread();
  }

  // Helper to create an extension with specific OAuth2Info fields set.
  // |fields_to_set| should be computed by using fields of Oauth2Fields enum.
  const Extension* CreateExtension(int fields_to_set) {
    const Extension* ext = nullptr;
    base::FilePath manifest_path =
        test_data_dir_.AppendASCII("api_test/identity/oauth2");
    base::FilePath component_manifest_path =
        test_data_dir_.AppendASCII("api_test/identity/component_oauth2");
    if ((fields_to_set & AS_COMPONENT) == 0) {
      ext = LoadExtension(manifest_path);
    } else {
      ext = LoadExtensionAsComponent(component_manifest_path);
    }

    if (!ext) {
      ADD_FAILURE() << "Cannot create extension";
      return nullptr;
    }

    OAuth2Info& oauth2_info =
        const_cast<OAuth2Info&>(OAuth2ManifestHandler::GetOAuth2Info(*ext));
    if ((fields_to_set & CLIENT_ID) != 0)
      oauth2_info.client_id = "client1";
    if ((fields_to_set & SCOPES) != 0) {
      oauth2_info.scopes.push_back("scope1");
      oauth2_info.scopes.push_back("scope2");
    }

    extension_id_ = ext->id();
    oauth_scopes_ = std::set<std::string>(oauth2_info.scopes.begin(),
                                          oauth2_info.scopes.end());
    return ext;
  }

  CoreAccountInfo GetPrimaryAccountInfo() {
    return identity_test_env()->identity_manager()->GetPrimaryAccountInfo(
        signin::ConsentLevel::kSignin);
  }

  CoreAccountId GetPrimaryAccountId() {
    return identity_test_env()->identity_manager()->GetPrimaryAccountId(
        signin::ConsentLevel::kSignin);
  }

  IdentityTokenCacheValue CreateToken(const std::string& token,
                                      base::TimeDelta time_to_live) {
    return IdentityTokenCacheValue::CreateToken(token, oauth_scopes_,
                                                time_to_live);
  }

  // Sets a cached token for the primary account.
  void SetCachedToken(const IdentityTokenCacheValue& token_data) {
    SetCachedTokenForAccount(GetPrimaryAccountInfo(), token_data);
  }

  void SetCachedTokenForAccount(const CoreAccountInfo account_info,
                                const IdentityTokenCacheValue& token_data) {
    ExtensionTokenKey key(extension_id_, account_info, oauth_scopes_);
    id_api()->token_cache()->SetToken(key, token_data);
  }

  void SetCachedGaiaId(const std::string& gaia_id) {
    id_api()->SetGaiaIdForExtension(extension_id_, gaia_id);
  }

  const IdentityTokenCacheValue& GetCachedToken(
      const CoreAccountInfo& account_info,
      const std::set<std::string>& scopes) {
    ExtensionTokenKey key(
        extension_id_,
        account_info.IsEmpty() ? GetPrimaryAccountInfo() : account_info,
        scopes);
    return id_api()->token_cache()->GetToken(key);
  }

  const IdentityTokenCacheValue& GetCachedToken(
      const CoreAccountInfo& account_info) {
    return GetCachedToken(account_info, oauth_scopes_);
  }

  std::optional<std::string> GetCachedGaiaId() {
    return id_api()->GetGaiaIdForExtension(extension_id_);
  }

  void QueueRequestStart(IdentityMintRequestQueue::MintType type,
                         IdentityMintRequestQueue::Request* request) {
    ExtensionTokenKey key(extension_id_, GetPrimaryAccountInfo(),
                          oauth_scopes_);
    id_api()->mint_queue()->RequestStart(type, key, request);
  }

  void QueueRequestComplete(IdentityMintRequestQueue::MintType type,
                            IdentityMintRequestQueue::Request* request) {
    ExtensionTokenKey key(extension_id_, GetPrimaryAccountInfo(),
                          oauth_scopes_);
    id_api()->mint_queue()->RequestComplete(type, key, request);
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

  base::OnceClosure on_access_token_requested_;

  void RunGetAuthTokenFunction(ExtensionFunction* function,
                               const std::string& args,
                               Browser* browser,
                               std::string* access_token,
                               std::set<std::string>* granted_scopes) {
    std::optional<base::Value> result_value =
        utils::RunFunctionAndReturnSingleResult(function, args,
                                                browser->profile());
    ASSERT_TRUE(result_value);
    std::optional<api::identity::GetAuthTokenResult> result =
        api::identity::GetAuthTokenResult::FromValue(*result_value);
    ASSERT_TRUE(result);

    EXPECT_TRUE(result->token);
    *access_token = *result->token;
    EXPECT_TRUE(result->granted_scopes);
    std::set<std::string> granted_scopes_map(result->granted_scopes->begin(),
                                             result->granted_scopes->end());
    *granted_scopes = std::move(granted_scopes_map);
  }

  void WaitForGetAuthTokenResults(
      ExtensionFunction* function,
      std::string* access_token,
      std::set<std::string>* granted_scopes,
      AsyncFunctionRunner* function_runner = nullptr) {
    base::Value result_value;
    if (function_runner == nullptr) {
      WaitForOneResult(function, &result_value);
    } else {
      function_runner->WaitForOneResult(function, &result_value);
    }
    std::optional<api::identity::GetAuthTokenResult> result =
        api::identity::GetAuthTokenResult::FromValue(result_value);
    ASSERT_TRUE(result);

    ASSERT_TRUE(result->token);
    *access_token = *result->token;
    ASSERT_TRUE(result->granted_scopes);
    std::set<std::string> granted_scopes_map(result->granted_scopes->begin(),
                                             result->granted_scopes->end());
    *granted_scopes = std::move(granted_scopes_map);
  }

  void SetUserGestureEnabled(bool enabled) {
    if (enabled) {
      if (!user_gesture_) {
        user_gesture_ =
            std::make_unique<ExtensionFunction::ScopedUserGestureForTests>();
      }
      return;
    }
    user_gesture_.reset();
  }

 private:
  // signin::IdentityManager::DiagnosticsObserver:
  void OnAccessTokenRequested(const CoreAccountId& account_id,
                              const std::string& consumer_id,
                              const signin::ScopeSet& scopes) override {
    if (on_access_token_requested_.is_null())
      return;
    std::move(on_access_token_requested_).Run();
  }

  base::test::ScopedFeatureList feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
  base::HistogramTester histogram_tester_;
  ExtensionId extension_id_;
  std::set<std::string> oauth_scopes_;
  std::unique_ptr<ExtensionFunction::ScopedUserGestureForTests> user_gesture_;
};

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, NoClientId) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(SCOPES));
  std::string error = utils::RunFunctionAndReturnError(func.get(), "[{}]",
                                                       browser()->profile());
  EXPECT_EQ(std::string(errors::kInvalidClientId), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kInvalidClientId, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, NoScopes) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID));
  std::string error = utils::RunFunctionAndReturnError(func.get(), "[{}]",
                                                       browser()->profile());
  EXPECT_EQ(std::string(errors::kInvalidScopes), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kEmptyScopes, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, NonInteractiveNotSignedIn) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  std::string error = utils::RunFunctionAndReturnError(func.get(), "[{}]",
                                                       browser()->profile());
  EXPECT_EQ(std::string(errors::kUserNotSignedIn), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kUserNotSignedIn, 1);
}

// The signin flow is simply not used on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveNotSignedInShowSigninOnlyOnce) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_login_ui_result(false);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser()->profile());
  EXPECT_EQ(std::string(errors::kUserNotSignedIn), error);
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kSignInFailed, 1);
}

// Signin is always allowed on Lacros.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       PRE_InteractiveNotSignedAndSigninNotAllowed) {
  // kSigninAllowed cannot be set after the profile creation. Use
  // kSigninAllowedOnNextStartup instead.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSigninAllowedOnNextStartup, false);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveNotSignedAndSigninNotAllowed) {
  ASSERT_FALSE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_login_ui_result(false);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser()->profile());
  EXPECT_EQ(std::string(errors::kBrowserSigninNotAllowed), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kBrowserSigninNotAllowed, 1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, NonInteractiveMintFailure) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_FAILURE);
  std::string error = utils::RunFunctionAndReturnError(func.get(), "[{}]",
                                                       browser()->profile());
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kMintTokenAuthFailure, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NonInteractiveLoginAccessTokenFailure) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_login_access_token_result(false);
  std::string error = utils::RunFunctionAndReturnError(func.get(), "[{}]",
                                                       browser()->profile());
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kGetAccessTokenAuthFailure, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NonInteractiveRemoteConsentSuccess) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());
  func->push_mint_token_result(TestOAuth2MintTokenFlow::REMOTE_CONSENT_SUCCESS);
  std::string error = utils::RunFunctionAndReturnError(func.get(), "[{}]",
                                                       browser()->profile());
  EXPECT_EQ(std::string(errors::kNoGrant), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());

  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_REMOTE_CONSENT,
            GetCachedToken(CoreAccountInfo()).status());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kGaiaConsentInteractionRequired, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NonInteractiveMintBadCredentials) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->push_mint_token_result(
      TestOAuth2MintTokenFlow::MINT_TOKEN_BAD_CREDENTIALS);
  std::string error = utils::RunFunctionAndReturnError(func.get(), "[{}]",
                                                       browser()->profile());
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kMintTokenAuthFailure, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NonInteractiveMintServiceError) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->push_mint_token_result(
      TestOAuth2MintTokenFlow::MINT_TOKEN_SERVICE_ERROR);
  std::string error = utils::RunFunctionAndReturnError(func.get(), "[{}]",
                                                       browser()->profile());
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kMintTokenAuthFailure, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveMintServiceErrorAccountValid) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->push_mint_token_result(
      TestOAuth2MintTokenFlow::MINT_TOKEN_SERVICE_ERROR);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser()->profile());
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));

  // The login UI should not have been shown, as the user's primary account is
  // in a valid state.
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kMintTokenAuthFailure, 1);
}

// The signin flow is simply not used on Ash.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveMintServiceErrorShowSigninOnlyOnce) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_login_ui_result(true);
  func->push_mint_token_result(
      TestOAuth2MintTokenFlow::MINT_TOKEN_SERVICE_ERROR);

  // The function should complete with an error, showing the signin UI only
  // once for the initial signin.
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser()->profile());
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kMintTokenAuthFailure, 1);
}
#endif

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, NonInteractiveSuccess) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  std::string access_token;
  std::set<std::string> granted_scopes;
  RunGetAuthTokenFunction(func.get(), "[{}]", browser(), &access_token,
                          &granted_scopes);
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            GetCachedToken(CoreAccountInfo()).status());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, InteractiveLoginCanceled) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_login_ui_result(false);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser()->profile());
  EXPECT_EQ(std::string(errors::kUserNotSignedIn), error);
// Ash does not support the interactive login flow, so the login UI will never
// be shown on that platform.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(func->login_ui_shown());
#endif
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kSignInFailed, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveMintBadCredentialsAccountValid) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->push_mint_token_result(
      TestOAuth2MintTokenFlow::MINT_TOKEN_BAD_CREDENTIALS);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser()->profile());
  // The login UI should not be shown as the account is in a valid state.
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kMintTokenAuthFailure, 1);
}

// The interactive login flow is always short-circuited out with failure on
// Ash, so the tests of the interactive login flow being successful are not
// relevant on that platform.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveLoginSuccessMintFailure) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_login_ui_result(true);
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_FAILURE);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser()->profile());
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kMintTokenAuthFailure, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveLoginSuccessMintBadCredentials) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_login_ui_result(true);
  func->push_mint_token_result(
      TestOAuth2MintTokenFlow::MINT_TOKEN_BAD_CREDENTIALS);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser()->profile());
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kMintTokenAuthFailure, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveLoginSuccessLoginAccessTokenFailure) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_login_ui_result(true);
  func->set_login_access_token_result(false);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser()->profile());
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kGetAccessTokenAuthFailure, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveLoginSuccessMintSuccess) {
  // TODO(courage): verify that account_id in token service requests
  // is correct once manual token minting for tests is implemented.
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_login_ui_result(true);
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  std::string access_token;
  std::set<std::string> granted_scopes;
  RunGetAuthTokenFunction(func.get(), "[{\"interactive\": true}]", browser(),
                          &access_token, &granted_scopes);
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, SignedInWebOnlyAcceptPrompt) {
  identity_test_env()->MakeAccountAvailable("account@gmail.com",
                                            {.set_cookie = true});
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  ASSERT_FALSE(identity_test_env()
                   ->identity_manager()
                   ->GetAccountsWithRefreshTokens()
                   .empty());

  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{},
      "ChromeSigninChoiceForExtensionsPrompt");

  RunFunctionAsync(func.get(), "[{\"interactive\": true}]");
  views::Widget* confirmation_prompt = widget_waiter.WaitIfNeededAndGet();
  views::DialogDelegate* dialog_delegate =
      confirmation_prompt->widget_delegate()->AsDialogDelegate();
  ASSERT_TRUE(dialog_delegate);
  dialog_delegate->AcceptDialog();

  std::string access_token;
  std::set<std::string> granted_scopes;
  WaitForGetAuthTokenResults(func.get(), &access_token, &granted_scopes);
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);

  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, SignedInWebOnlyDeclinePrompt) {
  identity_test_env()->MakeAccountAvailable("account@gmail.com",
                                            {.set_cookie = true});
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  ASSERT_FALSE(identity_test_env()
                   ->identity_manager()
                   ->GetAccountsWithRefreshTokens()
                   .empty());

  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{},
      "ChromeSigninChoiceForExtensionsPrompt");

  RunFunctionAsync(func.get(), "[{\"interactive\": true}]");
  views::Widget* confirmation_prompt = widget_waiter.WaitIfNeededAndGet();
  views::DialogDelegate* dialog_delegate =
      confirmation_prompt->widget_delegate()->AsDialogDelegate();
  ASSERT_TRUE(dialog_delegate);
  dialog_delegate->CancelDialog();

  EXPECT_EQ(std::string(errors::kUserNotSignedIn), WaitForError(func.get()));

  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kSignInFailed, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       SignedInWebOnlyAcceptPromptMultipleFunctions) {
  identity_test_env()->MakeAccountAvailable("account@gmail.com",
                                            {.set_cookie = true});
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  ASSERT_FALSE(identity_test_env()
                   ->identity_manager()
                   ->GetAccountsWithRefreshTokens()
                   .empty());

  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func1(new FakeGetAuthTokenFunction());
  func1->set_extension(extension.get());
  func1->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  scoped_refptr<FakeGetAuthTokenFunction> func2(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension2(
      CreateExtension(CLIENT_ID | SCOPES));
  func2->set_extension(extension2.get());
  func2->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  AsyncFunctionRunner func1_runner;
  func1_runner.RunFunctionAsync(func1.get(), "[{\"interactive\": true}]",
                                browser()->profile());

  AsyncFunctionRunner func2_runner;
  func2_runner.RunFunctionAsync(func2.get(), "[{\"interactive\": true}]",
                                browser()->profile());

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{},
      "ChromeSigninChoiceForExtensionsPrompt");

  views::Widget* confirmation_prompt = widget_waiter.WaitIfNeededAndGet();
  views::DialogDelegate* dialog_delegate =
      confirmation_prompt->widget_delegate()->AsDialogDelegate();
  ASSERT_TRUE(dialog_delegate);
  dialog_delegate->AcceptDialog();

  std::string access_token;
  std::set<std::string> granted_scopes;
  WaitForGetAuthTokenResults(func1.get(), &access_token, &granted_scopes,
                             &func1_runner);
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_EQ(func1->GetExtensionTokenKeyForTest()->scopes, granted_scopes);

  EXPECT_FALSE(func1->login_ui_shown());
  EXPECT_FALSE(func1->scope_ui_shown());

  WaitForGetAuthTokenResults(func2.get(), &access_token, &granted_scopes,
                             &func2_runner);
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_EQ(func2->GetExtensionTokenKeyForTest()->scopes, granted_scopes);

  EXPECT_FALSE(func2->login_ui_shown());
  EXPECT_FALSE(func2->scope_ui_shown());

  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      2);
}
#endif

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveLoginSuccessApprovalAborted) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_login_ui_result(true);
  func->push_mint_token_result(TestOAuth2MintTokenFlow::REMOTE_CONSENT_SUCCESS);
  func->set_scope_ui_failure(GaiaRemoteConsentFlow::WINDOW_CLOSED);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser()->profile());
  EXPECT_EQ(std::string(errors::kUserRejected), error);
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_TRUE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kRemoteConsentFlowRejected, 1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

class GetAuthTokenFunctionInteractivityTest
    : public GetAuthTokenFunctionTest,
      public testing::WithParamInterface<
          IdentityGetAuthTokenFunction::InteractivityStatus> {
 public:
  GetAuthTokenFunctionInteractivityTest() = default;

 private:
  void SetUpOnMainThread() override {
    // Mock the user activity.
    switch (GetParam()) {
      case IdentityGetAuthTokenFunction::InteractivityStatus::
          kAllowedWithGesture:
        SetUserGestureEnabled(true);
        break;
      case IdentityGetAuthTokenFunction::InteractivityStatus::
          kAllowedWithActivity:
        idle_state_ = std::make_unique<ui::ScopedSetIdleState>(
            ui::IdleState::IDLE_STATE_ACTIVE);
        SetUserGestureEnabled(false);
        ASSERT_EQ(
            ui::CalculateIdleState(kGetAuthTokenInactivityTime.InSeconds()),
            ui::IDLE_STATE_ACTIVE);
        break;
      case IdentityGetAuthTokenFunction::InteractivityStatus::kNotRequested:
      case IdentityGetAuthTokenFunction::InteractivityStatus::kDisallowedIdle:
        SetUserGestureEnabled(false);
        idle_state_ = std::make_unique<ui::ScopedSetIdleState>(
            ui::IdleState::IDLE_STATE_LOCKED);
        ASSERT_NE(
            ui::CalculateIdleState(kGetAuthTokenInactivityTime.InSeconds()),
            ui::IDLE_STATE_ACTIVE);
        break;
      case IdentityGetAuthTokenFunction::InteractivityStatus::
          kDisallowedSigninDisallowed:
        NOTREACHED_IN_MIGRATION();
        break;
    }

    GetAuthTokenFunctionTest::SetUpOnMainThread();
  }

  std::unique_ptr<ui::ScopedSetIdleState> idle_state_;
};

// The interactive login flow is always short-circuited out with failure on
// Ash, so the tests of the interactive login flow being successful are not
// relevant on that platform.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(GetAuthTokenFunctionInteractivityTest,
                       SigninInteractivityTest) {
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  ASSERT_EQ(func->user_gesture(),
            GetParam() == IdentityGetAuthTokenFunction::InteractivityStatus::
                              kAllowedWithGesture);

  func->set_extension(extension.get());
  func->set_login_ui_result(true);
  func->push_mint_token_result(TestOAuth2MintTokenFlow::REMOTE_CONSENT_SUCCESS);
  func->set_remote_consent_gaia_id("gaia_id_for_primary_example.com");
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  const std::string function_args =
      GetParam() ==
              IdentityGetAuthTokenFunction::InteractivityStatus::kNotRequested
          ? "[{}]"
          : "[{\"interactive\": true}]";
  IdentityGetAuthTokenError::State expected_error_state =
      IdentityGetAuthTokenError::State::kNone;
  if (GetParam() ==
          IdentityGetAuthTokenFunction::InteractivityStatus::kDisallowedIdle ||
      GetParam() ==
          IdentityGetAuthTokenFunction::InteractivityStatus::kNotRequested) {
    // Interactivity is not allowed, return an error.
    std::string error = utils::RunFunctionAndReturnError(
        func.get(), function_args, browser()->profile());
    std::string expected_error;
    if (GetParam() ==
        IdentityGetAuthTokenFunction::InteractivityStatus::kDisallowedIdle) {
      expected_error = errors::kGetAuthTokenInteractivityDeniedError;
      expected_error_state =
          IdentityGetAuthTokenError::State::kInteractivityDenied;
    } else {
      expected_error = errors::kUserNotSignedIn;
      expected_error_state = IdentityGetAuthTokenError::State::kUserNotSignedIn;
    }
    EXPECT_EQ(expected_error, error);
    EXPECT_FALSE(func->login_ui_shown());
    EXPECT_FALSE(func->scope_ui_shown());
  } else {
    std::string access_token;
    std::set<std::string> granted_scopes;
    RunGetAuthTokenFunction(func.get(), function_args, browser(), &access_token,
                            &granted_scopes);
    EXPECT_EQ(std::string(kAccessToken), access_token);
    EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);
    EXPECT_TRUE(func->login_ui_shown());
    EXPECT_TRUE(func->scope_ui_shown());
  }
  histogram_tester()->ExpectUniqueSample(kGetAuthTokenResultHistogramName,
                                         expected_error_state, 1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_P(GetAuthTokenFunctionInteractivityTest,
                       ConsentInteractivityTest) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  ASSERT_EQ(func->user_gesture(),
            GetParam() == IdentityGetAuthTokenFunction::InteractivityStatus::
                              kAllowedWithGesture);

  func->set_extension(extension.get());
  func->push_mint_token_result(TestOAuth2MintTokenFlow::REMOTE_CONSENT_SUCCESS);
  func->set_remote_consent_gaia_id("gaia_id_for_primary_example.com");
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  const std::string function_args =
      GetParam() ==
              IdentityGetAuthTokenFunction::InteractivityStatus::kNotRequested
          ? "[{}]"
          : "[{\"interactive\": true}]";
  IdentityGetAuthTokenError::State expected_error_state =
      IdentityGetAuthTokenError::State::kNone;
  if (GetParam() ==
          IdentityGetAuthTokenFunction::InteractivityStatus::kDisallowedIdle ||
      GetParam() ==
          IdentityGetAuthTokenFunction::InteractivityStatus::kNotRequested) {
    // Interactivity is not allowed, return an error.
    std::string error = utils::RunFunctionAndReturnError(
        func.get(), function_args, browser()->profile());
    std::string expected_error;
    if (GetParam() ==
        IdentityGetAuthTokenFunction::InteractivityStatus::kDisallowedIdle) {
      expected_error = errors::kGetAuthTokenInteractivityDeniedError;
      expected_error_state =
          IdentityGetAuthTokenError::State::kInteractivityDenied;
    } else {
      expected_error = errors::kNoGrant;
      expected_error_state =
          IdentityGetAuthTokenError::State::kGaiaConsentInteractionRequired;
    }
    EXPECT_EQ(expected_error, error);
    EXPECT_FALSE(func->scope_ui_shown());
  } else {
    std::string access_token;
    std::set<std::string> granted_scopes;
    RunGetAuthTokenFunction(func.get(), function_args, browser(), &access_token,
                            &granted_scopes);
    EXPECT_EQ(std::string(kAccessToken), access_token);
    EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);
    EXPECT_TRUE(func->scope_ui_shown());
    EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
              GetCachedToken(CoreAccountInfo()).status());
  }
  EXPECT_FALSE(func->login_ui_shown());
  histogram_tester()->ExpectUniqueSample(kGetAuthTokenResultHistogramName,
                                         expected_error_state, 1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GetAuthTokenFunctionInteractivityTest,
    testing::Values(
        IdentityGetAuthTokenFunction::InteractivityStatus::kNotRequested,
        IdentityGetAuthTokenFunction::InteractivityStatus::kDisallowedIdle,
        IdentityGetAuthTokenFunction::InteractivityStatus::kAllowedWithGesture,
        IdentityGetAuthTokenFunction::InteractivityStatus::
            kAllowedWithActivity));

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, InteractiveApprovalAborted) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->push_mint_token_result(TestOAuth2MintTokenFlow::REMOTE_CONSENT_SUCCESS);
  func->set_scope_ui_failure(GaiaRemoteConsentFlow::WINDOW_CLOSED);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser()->profile());
  EXPECT_EQ(std::string(errors::kUserRejected), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_TRUE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kRemoteConsentFlowRejected, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveApprovalLoadFailed) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->push_mint_token_result(TestOAuth2MintTokenFlow::REMOTE_CONSENT_SUCCESS);
  func->set_scope_ui_failure(GaiaRemoteConsentFlow::LOAD_FAILED);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser()->profile());
  EXPECT_EQ(std::string(errors::kPageLoadFailure), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_TRUE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kRemoteConsentPageLoadFailure, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveApprovalInvalidConsentResult) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->push_mint_token_result(TestOAuth2MintTokenFlow::REMOTE_CONSENT_SUCCESS);
  func->set_scope_ui_failure(GaiaRemoteConsentFlow::INVALID_CONSENT_RESULT);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser()->profile());
  EXPECT_TRUE(base::StartsWith(error, errors::kInvalidConsentResult,
                               base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_TRUE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kInvalidConsentResult, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, InteractiveApprovalNoGrant) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->push_mint_token_result(TestOAuth2MintTokenFlow::REMOTE_CONSENT_SUCCESS);
  func->set_scope_ui_failure(GaiaRemoteConsentFlow::NO_GRANT);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser()->profile());
  EXPECT_TRUE(base::StartsWith(error, errors::kNoGrant,
                               base::CompareCase::INSENSITIVE_ASCII));

  // The login UI should not be shown as the account is in a valid state.
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_TRUE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kNoGrant, 1);
}

// The signin flow is simply not used on Ash.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveApprovalNoGrantShowSigninUIOnlyOnce) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_login_ui_result(true);
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->push_mint_token_result(TestOAuth2MintTokenFlow::REMOTE_CONSENT_SUCCESS);
  func->set_scope_ui_failure(GaiaRemoteConsentFlow::NO_GRANT);

  // The function should complete with an error, showing the signin UI only
  // once for the initial signin.
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser()->profile());
  EXPECT_TRUE(base::StartsWith(error, errors::kNoGrant,
                               base::CompareCase::INSENSITIVE_ASCII));

  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_TRUE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kNoGrant, 1);
}
#endif

#if !BUILDFLAG(IS_MAC)
// Test was originally written for http://crbug.com/753014 and subsequently
// modified to use the remote consent flow.
//
// On macOS, closing all browsers does not shut down the browser process.
// TODO(http://crbug.com/756462): Figure out how to shut down the browser
// process on macOS and enable this test on macOS as well.
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveSigninFailedDuringBrowserProcessShutDown) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->push_mint_token_result(TestOAuth2MintTokenFlow::REMOTE_CONSENT_SUCCESS);
  func->set_scope_ui_failure(GaiaRemoteConsentFlow::LOAD_FAILED);
  func->set_login_ui_result(false);

  // Closing all browsers ensures that the browser process is shutting down.
  CloseAllBrowsers();

  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser()->profile());
  // Check that the remote consent dialog is shown to ensure that the remote
  // consent flow fails.
  EXPECT_TRUE(func->scope_ui_shown());

  // The login screen should not be shown when the browser process is shutting
  // down.
  EXPECT_FALSE(func->login_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kRemoteConsentPageLoadFailure, 1);
}
#endif  // !BUILDFLAG(IS_MAC)

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveSigninFailedDuringProfileShutDown) {
  SignIn("primary@example.com");
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::BROWSER, KeepAliveRestartOption::DISABLED);
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->push_mint_token_result(TestOAuth2MintTokenFlow::REMOTE_CONSENT_SUCCESS);
  // Have GetAuthTokenFunction make the request for the access token to ensure
  // that the function doesn't immediately succeed.
  func->set_auto_login_access_token(false);

  RunFunctionAsync(func.get(), "[{\"interactive\": true}]");
  identity_test_env()->identity_manager()->RemoveDiagnosticsObserver(this);
  identity_test_env_profile_adaptor_.reset();
  CloseBrowserSynchronously(browser());
  EXPECT_FALSE(func->scope_ui_shown());

  // The login screen should not be shown when the profile is shutting
  // down.
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_EQ(std::string(errors::kBrowserContextShutDown),
            WaitForError(func.get()));
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(keep_alive));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, NoninteractiveQueue) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());

  // Create a fake request to block the queue.
  MockQueuedMintRequest queued_request;
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE;

  EXPECT_CALL(queued_request, StartMintToken(type)).Times(1);
  QueueRequestStart(type, &queued_request);

  // The real request will start processing, but wait in the queue behind
  // the blocker.
  RunFunctionAsync(func.get(), "[{}]");
  // Verify that we have fetched the login token at this point.
  testing::Mock::VerifyAndClearExpectations(func.get());

  // The flow will be created after the first queued request clears.
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  QueueRequestComplete(type, &queued_request);

  std::string access_token;
  std::set<std::string> granted_scopes;
  WaitForGetAuthTokenResults(func.get(), &access_token, &granted_scopes);
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);

  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, InteractiveQueue) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());

  // Create a fake request to block the queue.
  MockQueuedMintRequest queued_request;
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE;

  EXPECT_CALL(queued_request, StartMintToken(type)).Times(1);
  QueueRequestStart(type, &queued_request);

  // The real request will start processing, but wait in the queue behind
  // the blocker.
  func->push_mint_token_result(TestOAuth2MintTokenFlow::REMOTE_CONSENT_SUCCESS);
  func->set_remote_consent_gaia_id("gaia_id_for_primary_example.com");
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);
  RunFunctionAsync(func.get(), "[{\"interactive\": true}]");
  // Verify that we have fetched the login token and run the first flow.
  testing::Mock::VerifyAndClearExpectations(func.get());
  EXPECT_FALSE(func->scope_ui_shown());

  // The UI will be displayed and a token retrieved after the first
  // queued request clears.
  QueueRequestComplete(type, &queued_request);

  std::string access_token;
  std::set<std::string> granted_scopes;
  WaitForGetAuthTokenResults(func.get(), &access_token, &granted_scopes);
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);

  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_TRUE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, InteractiveQueueShutdown) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());

  // Create a fake request to block the queue.
  MockQueuedMintRequest queued_request;
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE;

  EXPECT_CALL(queued_request, StartMintToken(type)).Times(1);
  QueueRequestStart(type, &queued_request);

  // The real request will start processing, but wait in the queue behind
  // the blocker.
  func->push_mint_token_result(TestOAuth2MintTokenFlow::REMOTE_CONSENT_SUCCESS);
  RunFunctionAsync(func.get(), "[{\"interactive\": true}]");
  // Verify that we have fetched the login token and run the first flow.
  testing::Mock::VerifyAndClearExpectations(func.get());
  EXPECT_FALSE(func->scope_ui_shown());

  // After the request is canceled, the function will complete.
  func->OnIdentityAPIShutdown();
  EXPECT_EQ(std::string(errors::kBrowserContextShutDown),
            WaitForError(func.get()));
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());

  QueueRequestComplete(type, &queued_request);
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kBrowserContextShutDown, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, NoninteractiveShutdown) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());

  func->push_mint_token_flow(std::make_unique<TestHangOAuth2MintTokenFlow>());
  RunFunctionAsync(func.get(), "[{\"interactive\": false}]");

  // After the request is canceled, the function will complete.
  func->OnIdentityAPIShutdown();
  EXPECT_EQ(std::string(errors::kBrowserContextShutDown),
            WaitForError(func.get()));
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kBrowserContextShutDown, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveQueuedNoninteractiveFails) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());

  // Create a fake request to block the interactive queue.
  MockQueuedMintRequest queued_request;
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE;

  EXPECT_CALL(queued_request, StartMintToken(type)).Times(1);
  QueueRequestStart(type, &queued_request);

  // Non-interactive requests fail without hitting GAIA, because a
  // consent UI is known to be up.
  std::string error = utils::RunFunctionAndReturnError(func.get(), "[{}]",
                                                       browser()->profile());
  EXPECT_EQ(std::string(errors::kNoGrant), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());

  QueueRequestComplete(type, &queued_request);
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kGaiaConsentInteractionAlreadyRunning,
      1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, NonInteractiveCacheHit) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());

  // Pre-populate the cache with a token.
  IdentityTokenCacheValue token =
      CreateToken(kAccessToken, base::Seconds(3600));
  SetCachedToken(token);

  // Get a token. Should not require a GAIA request.
  std::string access_token;
  std::set<std::string> granted_scopes;
  RunGetAuthTokenFunction(func.get(), "[{}]", browser(), &access_token,
                          &granted_scopes);
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NonInteractiveCacheHitSecondary) {
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      "email@example.com", signin::ConsentLevel::kSignin);

  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());

  // Pre-populate the cache with a token.
  IdentityTokenCacheValue token =
      CreateToken(kAccessToken, base::Seconds(3600));
  SetCachedTokenForAccount(account_info, token);

  std::string access_token;
  std::set<std::string> granted_scopes;
  RunGetAuthTokenFunction(func.get(), "[{}]", browser(), &access_token,
                          &granted_scopes);
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);

  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NonInteractiveRemoteConsentCacheHit) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());

  // Pre-populate the cache with the remote consent resolution data.
  RemoteConsentResolutionData resolution_data;
  IdentityTokenCacheValue token =
      IdentityTokenCacheValue::CreateRemoteConsent(resolution_data);
  SetCachedToken(token);

  // Should return an error without a GAIA request.
  std::string error = utils::RunFunctionAndReturnError(func.get(), "[{}]",
                                                       browser()->profile());
  EXPECT_EQ(std::string(errors::kNoGrant), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kGaiaConsentInteractionRequired, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NonInteractiveRemoteConsentApprovedCacheHit) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());

  // Pre-populate the cache with the remote consent approved result.
  IdentityTokenCacheValue token =
      IdentityTokenCacheValue::CreateRemoteConsentApproved(std::string());
  SetCachedToken(token);

  // Get a token. Should not prompt for scopes.
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);
  std::string access_token;
  std::set<std::string> granted_scopes;
  RunGetAuthTokenFunction(func.get(), "[{}]", browser(), &access_token,
                          &granted_scopes);
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, InteractiveCacheHit) {
  SignIn("primary@example.com");
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());

  // Create a fake request to block the queue.
  MockQueuedMintRequest queued_request;
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE;

  EXPECT_CALL(queued_request, StartMintToken(type)).Times(1);
  QueueRequestStart(type, &queued_request);

  // The real request will start processing, but wait in the queue behind
  // the blocker.
  func->push_mint_token_result(TestOAuth2MintTokenFlow::REMOTE_CONSENT_SUCCESS);
  RunFunctionAsync(func.get(), "[{\"interactive\": true}]");
  base::RunLoop().RunUntilIdle();

  // Populate the cache with a token while the request is blocked.
  IdentityTokenCacheValue token =
      CreateToken(kAccessToken, base::Seconds(3600));
  SetCachedToken(token);

  // When we wake up the request, it returns the cached token without
  // displaying a UI, or hitting GAIA.
  QueueRequestComplete(type, &queued_request);

  std::string access_token;
  std::set<std::string> granted_scopes;
  WaitForGetAuthTokenResults(func.get(), &access_token, &granted_scopes);
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);

  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

// The interactive login UI is never shown on Ash, so tests of the interactive
// login flow being successful are not relevant on that platform.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, LoginInvalidatesTokenCache) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());

  // Pre-populate the cache with a token.
  IdentityTokenCacheValue token =
      CreateToken(kAccessToken, base::Seconds(3600));
  SetCachedToken(token);

  // Because the user is not signed in, the token will be removed,
  // and we'll hit GAIA for new tokens.
  func->set_login_ui_result(true);
  func->push_mint_token_result(TestOAuth2MintTokenFlow::REMOTE_CONSENT_SUCCESS);
  func->set_remote_consent_gaia_id("gaia_id_for_primary_example.com");
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  std::string access_token;
  std::set<std::string> granted_scopes;
  RunGetAuthTokenFunction(func.get(), "[{\"interactive\": true}]", browser(),
                          &access_token, &granted_scopes);
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_TRUE(func->scope_ui_shown());

  ExtensionTokenKey key(extension->id(), CoreAccountInfo(), granted_scopes);
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
            id_api()->token_cache()->GetToken(key).status());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}
#endif

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, ComponentWithChromeClientId) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->ignore_did_respond_for_testing();
  scoped_refptr<const Extension> extension(
      CreateExtension(SCOPES | AS_COMPONENT));
  ASSERT_TRUE(extension.get());
  func->set_extension(extension.get());
  const OAuth2Info& oauth2_info =
      OAuth2ManifestHandler::GetOAuth2Info(*extension);
  EXPECT_FALSE(oauth2_info.client_id);
  EXPECT_FALSE(func->GetOAuth2ClientId().empty());
  EXPECT_NE("client1", func->GetOAuth2ClientId());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, ComponentWithNormalClientId) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->ignore_did_respond_for_testing();
  scoped_refptr<const Extension> extension(
      CreateExtension(CLIENT_ID | SCOPES | AS_COMPONENT));
  func->set_extension(extension.get());
  EXPECT_EQ("client1", func->GetOAuth2ClientId());
}

// Ensure that IdentityAPI shutdown triggers an active function call to return
// with an error.
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, IdentityAPIShutdown) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  // Have GetAuthTokenFunction actually make the request for the access token to
  // ensure that the function doesn't immediately succeed.
  func->set_auto_login_access_token(false);
  RunFunctionAsync(func.get(), "[{}]");

  id_api()->Shutdown();
  EXPECT_EQ(std::string(errors::kBrowserContextShutDown),
            WaitForError(func.get()));
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kBrowserContextShutDown, 1);
}

// Ensure that when there are multiple active function calls, IdentityAPI
// shutdown triggers them all to return with errors.
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       IdentityAPIShutdownWithMultipleActiveTokenRequests) {
  // Set up two extension functions, having them actually make the request for
  // the access token to ensure that they don't immediately succeed.
  scoped_refptr<FakeGetAuthTokenFunction> func1(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension1(
      CreateExtension(CLIENT_ID | SCOPES));
  func1->set_extension(extension1.get());
  func1->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);
  func1->set_auto_login_access_token(false);

  scoped_refptr<FakeGetAuthTokenFunction> func2(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension2(
      CreateExtension(CLIENT_ID | SCOPES));
  func2->set_extension(extension2.get());
  func2->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);
  func2->set_auto_login_access_token(false);

  // Run both functions. Note that it's necessary to use AsyncFunctionRunner
  // directly here rather than the AsyncExtensionBrowserTest instance methods
  // that wrap it, as each AsyncFunctionRunner instance sets itself as the
  // delegate of exactly one function.
  AsyncFunctionRunner func1_runner;
  func1_runner.RunFunctionAsync(func1.get(), "[{}]", browser()->profile());

  AsyncFunctionRunner func2_runner;
  func2_runner.RunFunctionAsync(func2.get(), "[{}]", browser()->profile());

  // Shut down IdentityAPI and ensure that both functions complete with an
  // error.
  id_api()->Shutdown();
  EXPECT_EQ(std::string(errors::kBrowserContextShutDown),
            func1_runner.WaitForError(func1.get()));
  EXPECT_EQ(std::string(errors::kBrowserContextShutDown),
            func2_runner.WaitForError(func2.get()));
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, ManuallyIssueToken) {
  CoreAccountId primary_account_id = SignIn("primary@example.com");

  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  // Have GetAuthTokenFunction actually make the request for the access token.
  func->set_auto_login_access_token(false);

  base::RunLoop run_loop;
  on_access_token_requested_ = run_loop.QuitClosure();
  RunFunctionAsync(func.get(), "[{}]");
  run_loop.Run();

  std::string primary_account_access_token =
      IssueLoginAccessTokenForAccount(primary_account_id);

  std::string access_token;
  std::set<std::string> granted_scopes;
  WaitForGetAuthTokenResults(func.get(), &access_token, &granted_scopes);
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);

  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            GetCachedToken(CoreAccountInfo()).status());
  EXPECT_THAT(func->login_access_tokens(),
              testing::ElementsAre(primary_account_access_token));
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, ManuallyIssueTokenFailure) {
  CoreAccountId primary_account_id = SignIn("primary@example.com");

  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  // Have GetAuthTokenFunction actually make the request for the access token.
  func->set_auto_login_access_token(false);

  base::RunLoop run_loop;
  on_access_token_requested_ = run_loop.QuitClosure();
  RunFunctionAsync(func.get(), "[{}]");
  run_loop.Run();

  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      primary_account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));

  EXPECT_EQ(
      std::string(errors::kAuthFailure) +
          GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE)
              .ToString(),
      WaitForError(func.get()));
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kGetAccessTokenAuthFailure, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       MultiDefaultUserManuallyIssueToken) {
  CoreAccountId primary_account_id = SignIn("primary@example.com");
  identity_test_env()->MakeAccountAvailable("secondary@example.com");

  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());
  func->set_auto_login_access_token(false);
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  base::RunLoop run_loop;
  on_access_token_requested_ = run_loop.QuitClosure();
  RunFunctionAsync(func.get(), "[{}]");
  run_loop.Run();

  std::string primary_account_access_token =
      IssueLoginAccessTokenForAccount(primary_account_id);

  std::string access_token;
  std::set<std::string> granted_scopes;
  WaitForGetAuthTokenResults(func.get(), &access_token, &granted_scopes);
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);

  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            GetCachedToken(CoreAccountInfo()).status());
  EXPECT_THAT(func->login_access_tokens(),
              testing::ElementsAre(primary_account_access_token));
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       MultiPrimaryUserManuallyIssueToken) {
  CoreAccountId primary_account_id = SignIn("primary@example.com");
  identity_test_env()->MakeAccountAvailable("secondary@example.com");

  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());
  func->set_auto_login_access_token(false);
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  base::RunLoop run_loop;
  on_access_token_requested_ = run_loop.QuitClosure();
  RunFunctionAsync(
      func.get(),
      "[{\"account\": { \"id\": \"gaia_id_for_primary_example.com\" } }]");
  run_loop.Run();

  std::string primary_account_access_token =
      IssueLoginAccessTokenForAccount(primary_account_id);

  std::string access_token;
  std::set<std::string> granted_scopes;
  WaitForGetAuthTokenResults(func.get(), &access_token, &granted_scopes);
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);

  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            GetCachedToken(CoreAccountInfo()).status());
  EXPECT_THAT(func->login_access_tokens(),
              testing::ElementsAre(primary_account_access_token));
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       MultiSecondaryUserManuallyIssueToken) {
  SignIn("primary@example.com");
  CoreAccountInfo secondary_account =
      identity_test_env()->MakeAccountAvailable("secondary@example.com");

  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());
  func->set_auto_login_access_token(false);
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  const char kFunctionParams[] =
      "[{\"account\": { \"id\": \"gaia_id_for_secondary_example.com\" } }]";

  if (id_api()->AreExtensionsRestrictedToPrimaryAccount()) {
    // Fail if extensions are restricted to the primary account.
    std::string error = utils::RunFunctionAndReturnError(
        func.get(), kFunctionParams, browser()->profile());
    EXPECT_EQ(std::string(errors::kUserNonPrimary), error);
    EXPECT_FALSE(func->login_ui_shown());
    EXPECT_FALSE(func->scope_ui_shown());
    histogram_tester()->ExpectUniqueSample(
        kGetAuthTokenResultHistogramName,
        IdentityGetAuthTokenError::State::kUserNonPrimary, 1);
    return;
  }

  base::RunLoop run_loop;
  on_access_token_requested_ = run_loop.QuitClosure();
  RunFunctionAsync(func.get(), kFunctionParams);
  run_loop.Run();

  std::string secondary_account_access_token =
      IssueLoginAccessTokenForAccount(secondary_account.account_id);

  std::string access_token;
  std::set<std::string> granted_scopes;
  WaitForGetAuthTokenResults(func.get(), &access_token, &granted_scopes);
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);

  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            GetCachedToken(secondary_account).status());
  EXPECT_THAT(func->login_access_tokens(),
              testing::ElementsAre(secondary_account_access_token));
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       MultiUnknownUserGetTokenFromTokenServiceFailure) {
  SignIn("primary@example.com");
  identity_test_env()->MakeAccountAvailable("secondary@example.com");

  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());
  func->set_auto_login_access_token(false);

  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"account\": { \"id\": \"unknown@example.com\" } }]",
      browser()->profile());
  std::string expected_error;
  if (id_api()->AreExtensionsRestrictedToPrimaryAccount()) {
    EXPECT_EQ(errors::kUserNonPrimary, error);
    histogram_tester()->ExpectUniqueSample(
        kGetAuthTokenResultHistogramName,
        IdentityGetAuthTokenError::State::kUserNonPrimary, 1);
  } else {
    EXPECT_EQ(errors::kUserNotSignedIn, error);
    histogram_tester()->ExpectUniqueSample(
        kGetAuthTokenResultHistogramName,
        IdentityGetAuthTokenError::State::kUserNotSignedIn, 1);
  }
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       MultiSecondaryNonInteractiveMintFailure) {
  // This test is only relevant if extensions see all accounts.
  if (id_api()->AreExtensionsRestrictedToPrimaryAccount())
    return;

  SignIn("primary@example.com");
  identity_test_env()->MakeAccountAvailable("secondary@example.com");

  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_FAILURE);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(),
      "[{\"account\": { \"id\": \"gaia_id_for_secondary_example.com\" } }]",
      browser()->profile());
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kMintTokenAuthFailure, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       MultiSecondaryNonInteractiveLoginAccessTokenFailure) {
  // This test is only relevant if extensions see all accounts.
  if (id_api()->AreExtensionsRestrictedToPrimaryAccount())
    return;

  SignIn("primary@example.com");
  identity_test_env()->MakeAccountAvailable("secondary@example.com");

  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_login_access_token_result(false);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(),
      "[{\"account\": { \"id\": \"gaia_id_for_secondary_example.com\" } }]",
      browser()->profile());
  EXPECT_TRUE(base::StartsWith(error, errors::kAuthFailure,
                               base::CompareCase::INSENSITIVE_ASCII));
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kGetAccessTokenAuthFailure, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       MultiSecondaryInteractiveApprovalAborted) {
  // This test is only relevant if extensions see all accounts.
  if (id_api()->AreExtensionsRestrictedToPrimaryAccount())
    return;

  SignIn("primary@example.com");
  identity_test_env()->MakeAccountAvailable("secondary@example.com");

  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  func->push_mint_token_result(TestOAuth2MintTokenFlow::REMOTE_CONSENT_SUCCESS);
  func->set_scope_ui_failure(GaiaRemoteConsentFlow::WINDOW_CLOSED);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(),
      "[{\"account\": { \"id\": \"gaia_id_for_secondary_example.com\" }, "
      "\"interactive\": true}]",
      browser()->profile());
  EXPECT_EQ(std::string(errors::kUserRejected), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_TRUE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kRemoteConsentFlowRejected, 1);
}

// Tests that Chrome remembers user's choice of an account at the end of the
// remote consent flow. Chrome should reuse this account in the next
// getAuthToken() call for the same extension.
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       MultiSecondaryInteractiveRemoteConsent) {
  CoreAccountId primary_account_id = SignIn("primary@example.com");
  AccountInfo secondary_account =
      identity_test_env()->MakeAccountAvailable("secondary@example.com");
  const extensions::Extension* extension = CreateExtension(CLIENT_ID | SCOPES);

  {
    scoped_refptr<FakeGetAuthTokenFunction> func(
        new FakeGetAuthTokenFunction());
    func->set_extension(extension);
    func->push_mint_token_result(
        TestOAuth2MintTokenFlow::REMOTE_CONSENT_SUCCESS);
    func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);
    func->set_remote_consent_gaia_id(secondary_account.gaia);
    // Have GetAuthTokenFunction actually make the request for the access token.
    func->set_auto_login_access_token(false);

    base::RunLoop run_loop;
    on_access_token_requested_ = run_loop.QuitClosure();
    RunFunctionAsync(func.get(), "[{\"interactive\": true}]");
    run_loop.Run();

    // The first request will be for the primary account and the second one for
    // the account that has been returned in result of the remote consent.
    std::string primary_account_access_token =
        IssueLoginAccessTokenForAccount(primary_account_id);

    if (id_api()->AreExtensionsRestrictedToPrimaryAccount()) {
      EXPECT_EQ(std::string(errors::kUserNonPrimary), WaitForError(func.get()));
      histogram_tester()->ExpectUniqueSample(
          kGetAuthTokenResultHistogramName,
          IdentityGetAuthTokenError::State::kRemoteConsentUserNonPrimary, 1);
      histogram_tester()->ExpectUniqueSample(
          kGetAuthTokenResultAfterConsentApprovedHistogramName,
          IdentityGetAuthTokenError::State::kRemoteConsentUserNonPrimary, 1);
      return;
    }

    std::string secondary_account_access_token =
        IssueLoginAccessTokenForAccount(secondary_account.account_id);

    std::string access_token;
    std::set<std::string> granted_scopes;
    WaitForGetAuthTokenResults(func.get(), &access_token, &granted_scopes);
    EXPECT_EQ(std::string(kAccessToken), access_token);
    EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);

    EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
              GetCachedToken(secondary_account).status());
    EXPECT_EQ(secondary_account.gaia,
              id_api()->GetGaiaIdForExtension(extension->id()));
    EXPECT_THAT(func->login_access_tokens(),
                testing::ElementsAre(primary_account_access_token,
                                     secondary_account_access_token));
    histogram_tester()->ExpectUniqueSample(
        kGetAuthTokenResultHistogramName,
        IdentityGetAuthTokenError::State::kNone, 1);
    histogram_tester()->ExpectUniqueSample(
        kGetAuthTokenResultAfterConsentApprovedHistogramName,
        IdentityGetAuthTokenError::State::kNone, 1);
  }

  {
    // Check that the next function call returns a token for the same account
    // from the cache.
    scoped_refptr<FakeGetAuthTokenFunction> func(
        new FakeGetAuthTokenFunction());
    func->set_extension(extension);

    std::string access_token;
    std::set<std::string> granted_scopes;
    RunGetAuthTokenFunction(func.get(), "[{}]", browser(), &access_token,
                            &granted_scopes);
    EXPECT_EQ(std::string(kAccessToken), access_token);
    EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);
    EXPECT_FALSE(func->login_ui_shown());
    EXPECT_FALSE(func->scope_ui_shown());
    histogram_tester()->ExpectUniqueSample(
        kGetAuthTokenResultHistogramName,
        IdentityGetAuthTokenError::State::kNone, 2);
    histogram_tester()->ExpectUniqueSample(
        kGetAuthTokenResultAfterConsentApprovedHistogramName,
        IdentityGetAuthTokenError::State::kNone, 1);
  }
}

// Tests two concurrent remote consent flows. Both of them should succeed.
// The second flow starts while the first one is blocked on an interactive mint
// token flow. This is a regression test for https://crbug.com/1091423.
IN_PROC_BROWSER_TEST_F(
    GetAuthTokenFunctionTest,
    RemoteConsentMultipleActiveRequests_BlockedOnInteractive) {
  SignIn("primary@example.com");
  CoreAccountInfo account = GetPrimaryAccountInfo();
  const extensions::Extension* extension = CreateExtension(CLIENT_ID | SCOPES);

  scoped_refptr<FakeGetAuthTokenFunction> func1(new FakeGetAuthTokenFunction());
  func1->set_extension(extension);
  func1->push_mint_token_result(
      TestOAuth2MintTokenFlow::REMOTE_CONSENT_SUCCESS);
  func1->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);
  func1->set_remote_consent_gaia_id(account.gaia);
  base::RunLoop scope_ui_shown_loop;
  func1->set_scope_ui_async(scope_ui_shown_loop.QuitClosure());

  scoped_refptr<FakeGetAuthTokenFunction> func2(new FakeGetAuthTokenFunction());
  func2->set_extension(extension);
  func2->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);
  func2->set_remote_consent_gaia_id(account.gaia);

  AsyncFunctionRunner func1_runner;
  func1_runner.RunFunctionAsync(func1.get(), "[{\"interactive\": true}]",
                                browser()->profile());

  AsyncFunctionRunner func2_runner;
  func2_runner.RunFunctionAsync(func2.get(), "[{\"interactive\": true}]",
                                browser()->profile());

  // Allows func2 to put a task in the queue.
  base::RunLoop().RunUntilIdle();

  scope_ui_shown_loop.Run();
  func1->CompleteRemoteConsentDialog();

  std::string access_token1;
  std::set<std::string> granted_scopes1;
  WaitForGetAuthTokenResults(func1.get(), &access_token1, &granted_scopes1,
                             &func1_runner);
  EXPECT_EQ(std::string(kAccessToken), access_token1);
  EXPECT_EQ(func1->GetExtensionTokenKeyForTest()->scopes, granted_scopes1);

  std::string access_token2;
  std::set<std::string> granted_scopes2;
  WaitForGetAuthTokenResults(func2.get(), &access_token2, &granted_scopes2,
                             &func2_runner);
  EXPECT_EQ(std::string(kAccessToken), access_token2);
  EXPECT_EQ(func2->GetExtensionTokenKeyForTest()->scopes, granted_scopes2);

  // Only one consent ui should be shown.
  int total_scope_ui_shown = func1->scope_ui_shown() + func2->scope_ui_shown();
  EXPECT_EQ(1, total_scope_ui_shown);

  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            GetCachedToken(account).status());

  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      2);
}

// Tests two concurrent remote consent flows. Both of them should succeed.
// The second flow starts while the first one is blocked on a non-interactive
// mint token flow. This is a regression test for https://crbug.com/1091423.
IN_PROC_BROWSER_TEST_F(
    GetAuthTokenFunctionTest,
    RemoteConsentMultipleActiveRequests_BlockedOnNoninteractive) {
  SignIn("primary@example.com");
  CoreAccountInfo account = GetPrimaryAccountInfo();
  const extensions::Extension* extension = CreateExtension(CLIENT_ID | SCOPES);

  scoped_refptr<FakeGetAuthTokenFunction> func1(new FakeGetAuthTokenFunction());
  func1->set_extension(extension);
  func1->push_mint_token_result(
      TestOAuth2MintTokenFlow::REMOTE_CONSENT_SUCCESS);
  func1->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);
  func1->set_remote_consent_gaia_id(account.gaia);
  func1->set_auto_login_access_token(false);

  scoped_refptr<FakeGetAuthTokenFunction> func2(new FakeGetAuthTokenFunction());
  func2->set_extension(extension);
  func2->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);
  func2->set_remote_consent_gaia_id(account.gaia);
  base::RunLoop scope_ui_shown_loop;
  func2->set_scope_ui_async(scope_ui_shown_loop.QuitClosure());

  base::RunLoop access_token_run_loop;
  on_access_token_requested_ = access_token_run_loop.QuitClosure();
  AsyncFunctionRunner func1_runner;
  func1_runner.RunFunctionAsync(func1.get(), "[{\"interactive\": true}]",
                                browser()->profile());

  AsyncFunctionRunner func2_runner;
  func2_runner.RunFunctionAsync(func2.get(), "[{\"interactive\": true}]",
                                browser()->profile());

  // Allows func2 to put a task in the queue.
  base::RunLoop().RunUntilIdle();

  access_token_run_loop.Run();
  // Let subsequent requests pass automatically.
  func1->set_auto_login_access_token(true);
  IssueLoginAccessTokenForAccount(account.account_id);

  scope_ui_shown_loop.Run();
  func2->CompleteRemoteConsentDialog();

  std::string access_token1;
  std::set<std::string> granted_scopes1;
  WaitForGetAuthTokenResults(func1.get(), &access_token1, &granted_scopes1,
                             &func1_runner);
  EXPECT_EQ(std::string(kAccessToken), access_token1);
  EXPECT_EQ(func1->GetExtensionTokenKeyForTest()->scopes, granted_scopes1);

  std::string access_token2;
  std::set<std::string> granted_scopes2;
  WaitForGetAuthTokenResults(func2.get(), &access_token2, &granted_scopes2,
                             &func2_runner);
  EXPECT_EQ(std::string(kAccessToken), access_token2);
  EXPECT_EQ(func2->GetExtensionTokenKeyForTest()->scopes, granted_scopes2);

  // Only one consent ui should be shown.
  int total_scope_ui_shown = func1->scope_ui_shown() + func2->scope_ui_shown();
  EXPECT_EQ(1, total_scope_ui_shown);

  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            GetCachedToken(account).status());

  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      2);
}

// The signin flow is simply not used on Ash.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       MultiSecondaryInteractiveInvalidToken) {
  // Setup a secondary account with no valid refresh token, and try to get a
  // auth token for it.
  SignIn("primary@example.com");
  AccountInfo secondary_account =
      identity_test_env()->MakeAccountAvailable("secondary@example.com");
  identity_test_env()->SetInvalidRefreshTokenForAccount(
      secondary_account.account_id);

  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(extension.get());
  func->set_login_ui_result(true);
  func->push_mint_token_result(TestOAuth2MintTokenFlow::REMOTE_CONSENT_SUCCESS);
  func->set_remote_consent_gaia_id(secondary_account.account_id.ToString());
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  const char kFunctionParams[] =
      "[{\"account\": { \"id\": \"gaia_id_for_secondary@example.com\" }, "
      "\"interactive\": true}]";

  if (id_api()->AreExtensionsRestrictedToPrimaryAccount()) {
    // Fail if extensions are restricted to the primary account.
    std::string error = utils::RunFunctionAndReturnError(
        func.get(), kFunctionParams, browser()->profile());
    EXPECT_EQ(std::string(errors::kUserNonPrimary), error);
    EXPECT_FALSE(func->login_ui_shown());
    EXPECT_FALSE(func->scope_ui_shown());
    histogram_tester()->ExpectUniqueSample(
        kGetAuthTokenResultHistogramName,
        IdentityGetAuthTokenError::State::kUserNonPrimary, 1);
  } else {
    // Extensions can show the login UI for secondary accounts, and get the auth
    // token.
    std::string access_token;
    std::set<std::string> granted_scopes;
    RunGetAuthTokenFunction(func.get(), kFunctionParams, browser(),
                            &access_token, &granted_scopes);
    EXPECT_EQ(std::string(kAccessToken), access_token);
    EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);
    EXPECT_TRUE(func->login_ui_shown());
    EXPECT_TRUE(func->scope_ui_shown());
    histogram_tester()->ExpectUniqueSample(
        kGetAuthTokenResultHistogramName,
        IdentityGetAuthTokenError::State::kNone, 1);
  }
}
#endif

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, ScopesDefault) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  std::string access_token;
  std::set<std::string> granted_scopes;
  RunGetAuthTokenFunction(func.get(), "[{}]", browser(), &access_token,
                          &granted_scopes);
  EXPECT_EQ(std::string(kAccessToken), access_token);

  const ExtensionTokenKey* token_key = func->GetExtensionTokenKeyForTest();
  EXPECT_EQ(token_key->scopes, granted_scopes);
  EXPECT_EQ(2ul, token_key->scopes.size());
  EXPECT_TRUE(base::Contains(token_key->scopes, "scope1"));
  EXPECT_TRUE(base::Contains(token_key->scopes, "scope2"));
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, ScopesEmpty) {
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());

  std::string error(utils::RunFunctionAndReturnError(
      func.get(), "[{\"scopes\": []}]", browser()->profile()));

  EXPECT_EQ(errors::kInvalidScopes, error);
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kEmptyScopes, 1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, ScopesEmail) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());

  std::set<std::string> scopes = {"email"};
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS,
                               scopes);
  std::string access_token;
  std::set<std::string> granted_scopes;
  RunGetAuthTokenFunction(func.get(), "[{\"scopes\": [\"email\"]}]", browser(),
                          &access_token, &granted_scopes);
  EXPECT_EQ(std::string(kAccessToken), access_token);

  const ExtensionTokenKey* token_key = func->GetExtensionTokenKeyForTest();
  EXPECT_EQ(token_key->scopes, granted_scopes);
  EXPECT_EQ(scopes, token_key->scopes);
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, ScopesEmailFooBar) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());

  std::set<std::string> scopes = {"email", "foo", "bar"};
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS,
                               scopes);
  std::string access_token;
  std::set<std::string> granted_scopes;
  RunGetAuthTokenFunction(func.get(),
                          "[{\"scopes\": [\"email\", \"foo\", \"bar\"]}]",
                          browser(), &access_token, &granted_scopes);
  EXPECT_EQ(std::string(kAccessToken), access_token);

  const ExtensionTokenKey* token_key = func->GetExtensionTokenKeyForTest();
  EXPECT_EQ(token_key->scopes, granted_scopes);
  EXPECT_EQ(scopes, token_key->scopes);
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

// Ensure that the returned scopes from the function is the cached scopes and
// not the requested scopes.
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, SubsetMatchCacheHit) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());

  std::set<std::string> scopes = {"email", "foo", "bar"};
  IdentityTokenCacheValue token = IdentityTokenCacheValue::CreateToken(
      kAccessToken, scopes, base::Seconds(3600));
  SetCachedToken(token);

  std::string access_token;
  std::set<std::string> granted_scopes;
  RunGetAuthTokenFunction(func.get(), "[{\"scopes\": [\"email\", \"foo\"]}]",
                          browser(), &access_token, &granted_scopes);
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_EQ(scopes, granted_scopes);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());

  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

// Ensure that the newly cached token uses the granted scopes and not the
// requested scopes.
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, SubsetMatchCachePopulate) {
  SignIn("primary@example.com");
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());

  std::set<std::string> scopes = {"foo", "bar"};
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS,
                               scopes);
  std::string access_token;
  std::set<std::string> granted_scopes;
  RunGetAuthTokenFunction(func.get(), "[{\"scopes\": [\"email\", \"foo\"]}]",
                          browser(), &access_token, &granted_scopes);

  const IdentityTokenCacheValue& token =
      GetCachedToken(CoreAccountInfo(), scopes);
  EXPECT_EQ(std::string(kAccessToken), token.token());
  EXPECT_EQ(scopes, token.granted_scopes());
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN, token.status());

  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

// Ensure that the scopes returned by the function reflects the granted scopes
// and not the requested scopes.
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest, GranularPermissionsResponse) {
  SignIn("primary@example.com");
  auto func = base::MakeRefCounted<FakeGetAuthTokenFunction>();
  auto extension = base::WrapRefCounted(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());

  std::set<std::string> scopes = {"email", "foobar"};
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS,
                               scopes);
  std::string access_token;
  std::set<std::string> granted_scopes;
  RunGetAuthTokenFunction(func.get(),
                          "[{\"enableGranularPermissions\": true,"
                          "\"scopes\": [\"email\", \"bar\"]}]",
                          browser(), &access_token, &granted_scopes);
  EXPECT_EQ(kAccessToken, access_token);
  EXPECT_EQ(scopes, granted_scopes);

  EXPECT_TRUE(func->enable_granular_permissions());
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

#if BUILDFLAG(IS_CHROMEOS)
enum class DeviceLocalAccountSessionType { kPublic, kAppKiosk, kWebKiosk };

#if BUILDFLAG(IS_CHROMEOS_ASH)
class GetAuthTokenFunctionDeviceLocalAccountTestPlatformHelper {
 public:
  const AccountId kFakeAccountId = AccountId::FromUserEmail("test@test");

  explicit GetAuthTokenFunctionDeviceLocalAccountTestPlatformHelper(
      DeviceLocalAccountSessionType session_type)
      : session_type_(session_type) {}

  void SetUpOnMainThread() {
    ash::LoginState::Get()->SetLoggedInState(
        ash::LoginState::LoggedInState::LOGGED_IN_ACTIVE,
        session_type_ == DeviceLocalAccountSessionType::kPublic
            ? ash::LoginState::LoggedInUserType::LOGGED_IN_USER_PUBLIC_ACCOUNT
            : ash::LoginState::LoggedInUserType::LOGGED_IN_USER_KIOSK);
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    user_manager::User* user = nullptr;
    switch (session_type_) {
      case DeviceLocalAccountSessionType::kPublic:
        user = user_manager->AddPublicAccountUser(kFakeAccountId);
        break;
      case DeviceLocalAccountSessionType::kAppKiosk:
        user = user_manager->AddKioskAppUser(kFakeAccountId);
        break;
      case DeviceLocalAccountSessionType::kWebKiosk:
        user = user_manager->AddWebKioskAppUser(kFakeAccountId);
        break;
    }
    ASSERT_TRUE(user);
    user_manager->UserLoggedIn(kFakeAccountId, user->username_hash(),
                               /*browser_restart=*/false, /*is_child=*/false);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
  }

  void TearDownOnMainThread() {
    auto* fake_manager = static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
    // Explicitly removing the user is required; otherwise ProfileHelper keeps
    // a dangling pointer to the User.
    // TODO(b/208629291): Consider removing all users from ProfileHelper in the
    // destructor of `ash::FakeChromeUserManager`.
    fake_manager->RemoveUserFromList(kFakeAccountId);
    scoped_user_manager_.reset();
  }

 private:
  const DeviceLocalAccountSessionType session_type_;

  // Set up fake install attributes to make the device appeared as
  // enterprise-managed.
  ash::ScopedStubInstallAttributes test_install_attributes_{
      ash::StubInstallAttributes::CreateCloudManaged("example.com", "fake-id")};

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class GetAuthTokenFunctionDeviceLocalAccountTestPlatformHelper {
 public:
  explicit GetAuthTokenFunctionDeviceLocalAccountTestPlatformHelper(
      DeviceLocalAccountSessionType session_type) {
    crosapi::mojom::SessionType init_params_session_type =
        crosapi::mojom::SessionType::kUnknown;
    switch (session_type) {
      case DeviceLocalAccountSessionType::kPublic:
        init_params_session_type = crosapi::mojom::SessionType::kPublicSession;
        break;
      case DeviceLocalAccountSessionType::kAppKiosk:
        init_params_session_type =
            crosapi::mojom::SessionType::kAppKioskSession;
        break;
      case DeviceLocalAccountSessionType::kWebKiosk:
        init_params_session_type =
            crosapi::mojom::SessionType::kWebKioskSession;
        break;
    }
    crosapi::mojom::BrowserInitParamsPtr init_params =
        crosapi::mojom::BrowserInitParams::New();
    init_params->session_type = init_params_session_type;
    init_params->is_device_enterprised_managed = true;
    init_params->device_settings = crosapi::mojom::DeviceSettings::New();
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
  }

  void SetUpOnMainThread() {}
  void TearDownOnMainThread() {}
};
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

class GetAuthTokenFunctionDeviceLocalAccountTest
    : public GetAuthTokenFunctionTest {
 public:
  explicit GetAuthTokenFunctionDeviceLocalAccountTest(
      DeviceLocalAccountSessionType session_type)
      : platform_helper_(session_type) {}

  void SetUpOnMainThread() override {
    platform_helper_.SetUpOnMainThread();
    GetAuthTokenFunctionTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    GetAuthTokenFunctionTest::TearDownOnMainThread();
    platform_helper_.TearDownOnMainThread();
  }

 protected:
  void RunExtensionAndVerifyNoError(bool is_extension_allowlisted) {
    scoped_refptr<FakeGetAuthTokenFunction> func(
        new FakeGetAuthTokenFunction());
    std::string extension_id = is_extension_allowlisted
                                   ? "ljacajndfccfgnfohlgkdphmbnpkjflk"
                                   : "test-id";
    func->set_extension(CreateTestExtension(extension_id));
    func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

    std::string access_token;
    std::set<std::string> granted_scopes;
    RunGetAuthTokenFunction(func.get(), "[{}]", browser(), &access_token,
                            &granted_scopes);
    EXPECT_EQ(std::string(kAccessToken), access_token);
    EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);
    histogram_tester()->ExpectUniqueSample(
        kGetAuthTokenResultHistogramName,
        IdentityGetAuthTokenError::State::kNone, 1);
  }

  scoped_refptr<const Extension> CreateTestExtension(const std::string& id) {
    return ExtensionBuilder("Test")
        .SetManifestKey(
            "oauth2", base::Value::Dict()
                          .Set("client_id", "clientId")
                          .Set("scopes", base::Value::List().Append("scope1")))
        .SetID(id)
        .Build();
  }

  GetAuthTokenFunctionDeviceLocalAccountTestPlatformHelper platform_helper_;
};

class GetAuthTokenFunctionPublicSessionTest
    : public GetAuthTokenFunctionDeviceLocalAccountTest {
 protected:
  GetAuthTokenFunctionPublicSessionTest()
      : GetAuthTokenFunctionDeviceLocalAccountTest(
            DeviceLocalAccountSessionType::kPublic) {}
};

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionPublicSessionTest, NonAllowlisted) {
  // GetAuthToken() should return UserNotSignedIn in public sessions for
  // non-allowlisted extensions.
  scoped_refptr<FakeGetAuthTokenFunction> func(new FakeGetAuthTokenFunction());
  func->set_extension(CreateTestExtension("test-id"));
  std::string error =
      utils::RunFunctionAndReturnError(func.get(), "[]", browser()->profile());
  EXPECT_EQ(std::string(errors::kUserNotSignedIn), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName,
      IdentityGetAuthTokenError::State::kNotAllowlistedInPublicSession, 1);
}

class GetAuthTokenFunctionChromeKioskTest
    : public GetAuthTokenFunctionDeviceLocalAccountTest {
 protected:
  GetAuthTokenFunctionChromeKioskTest()
      : GetAuthTokenFunctionDeviceLocalAccountTest(
            DeviceLocalAccountSessionType::kAppKiosk) {}
};

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionChromeKioskTest, NonAllowlisted) {
  // GetAuthToken() should return a token for non-allowlisted extensions in the
  // Chrome Kiosk session.
  RunExtensionAndVerifyNoError(/*is_extension_allowlisted=*/false);
}

class GetAuthTokenFunctionWebKioskTest
    : public GetAuthTokenFunctionDeviceLocalAccountTest {
 protected:
  GetAuthTokenFunctionWebKioskTest()
      : GetAuthTokenFunctionDeviceLocalAccountTest(
            DeviceLocalAccountSessionType::kWebKiosk) {}
};

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionWebKioskTest, NonAllowlisted) {
  // GetAuthToken() should return a token for non-allowlisted extensions in the
  // web Kiosk session.
  RunExtensionAndVerifyNoError(/*is_extension_allowlisted=*/false);
}

#endif  // BUILDFLAG(IS_CHROMEOS)

// There are two parameters, which are stored in a std::pair, for these tests.
//
// std::string: the GetAuthToken arguments
// bool: the expected value of GetAuthToken's enable_granular_permissions
class GetAuthTokenFunctionEnableGranularPermissionsTest
    : public GetAuthTokenFunctionTest,
      public testing::WithParamInterface<std::pair<std::string, bool>> {};

// Provided with the arguments for GetAuthToken, ensures that GetAuthToken's
// enable_granular_permissions is some expected value when the
// 'ReturnScopesInGetAuthToken' feature flag is enabled.
IN_PROC_BROWSER_TEST_P(GetAuthTokenFunctionEnableGranularPermissionsTest,
                       EnableGranularPermissions) {
  const std::string& args = GetParam().first;
  bool expected_enable_granular_permissions = GetParam().second;

  SignIn("primary@example.com");
  auto func = base::MakeRefCounted<FakeGetAuthTokenFunction>();
  auto extension = base::WrapRefCounted(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension.get());
  func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

  std::string access_token;
  std::set<std::string> granted_scopes;
  RunGetAuthTokenFunction(func.get(), "[{" + args + "}]", browser(),
                          &access_token, &granted_scopes);
  EXPECT_EQ(kAccessToken, access_token);
  EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);

  EXPECT_EQ(expected_enable_granular_permissions,
            func->enable_granular_permissions());
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->scope_ui_shown());
  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GetAuthTokenFunctionEnableGranularPermissionsTest,
    testing::Values(std::make_pair("\"enableGranularPermissions\": true", true),
                    std::make_pair("\"enableGranularPermissions\": false",
                                   false),
                    std::make_pair("", false)));

class RemoveCachedAuthTokenFunctionTest : public ExtensionBrowserTest {
 protected:
  bool InvalidateDefaultToken() {
    scoped_refptr<IdentityRemoveCachedAuthTokenFunction> func(
        new IdentityRemoveCachedAuthTokenFunction);
    func->set_extension(
        ExtensionBuilder("Test").SetID(kExtensionId).Build().get());
    return utils::RunFunction(
        func.get(), std::string("[{\"token\": \"") + kAccessToken + "\"}]",
        browser()->profile(), api_test_utils::FunctionMode::kNone);
  }

  IdentityAPI* id_api() {
    return IdentityAPI::GetFactoryInstance()->Get(browser()->profile());
  }

  IdentityTokenCacheValue CreateToken(const std::string& token,
                                      base::TimeDelta time_to_live) {
    return IdentityTokenCacheValue::CreateToken(
        token, std::set<std::string>({"foo"}), time_to_live);
  }

  void SetCachedToken(const IdentityTokenCacheValue& token_data) {
    CoreAccountInfo account_info;
    account_info.account_id = CoreAccountId::FromGaiaId("test_gaia");
    account_info.gaia = "test_gaia";
    account_info.email = "test@example.com";
    ExtensionTokenKey key(kExtensionId, account_info,
                          std::set<std::string>({"foo"}));
    id_api()->token_cache()->SetToken(key, token_data);
  }

  const IdentityTokenCacheValue& GetCachedToken() {
    CoreAccountInfo account_info;
    account_info.account_id = CoreAccountId::FromGaiaId("test_gaia");
    account_info.gaia = "test_gaia";
    account_info.email = "test@example.com";
    ExtensionTokenKey key(kExtensionId, account_info,
                          std::set<std::string>({"foo"}));
    return id_api()->token_cache()->GetToken(key);
  }
};

class GetAuthTokenFunctionSelectedUserIdTest : public GetAuthTokenFunctionTest {
 public:
  // Executes a new function and checks that the selected_user_id is the
  // expected value. The interactive and scopes field are predefined.
  // The account id specified by the extension is optional.
  void RunNewFunctionAndExpectSelectedUserId(
      const scoped_refptr<const extensions::Extension>& extension,
      const std::string& expected_selected_user_id,
      const std::optional<std::string> requested_account = std::nullopt) {
    auto func = base::MakeRefCounted<FakeGetAuthTokenFunction>();
    func->set_extension(extension);
    RunFunctionAndExpectSelectedUserId(func, expected_selected_user_id,
                                       requested_account);
  }

  void RunFunctionAndExpectSelectedUserId(
      const scoped_refptr<FakeGetAuthTokenFunction>& func,
      const std::string& expected_selected_user_id,
      const std::optional<std::string> requested_account = std::nullopt) {
    // Stops the function right before selected_user_id would be used.
    MockQueuedMintRequest queued_request;
    IdentityMintRequestQueue::MintType type =
        IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE;
    EXPECT_CALL(queued_request, StartMintToken(type)).Times(1);
    QueueRequestStart(type, &queued_request);

    func->push_mint_token_result(TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS);

    std::string requested_account_arg =
        requested_account.has_value()
            ? ", \"account\": {\"id\": \"" + requested_account.value() + "\"}"
            : "";
    RunFunctionAsync(func.get(),
                     "[{\"interactive\": true" + requested_account_arg + "}]");
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(expected_selected_user_id, func->GetSelectedUserId());

    // Resume the function
    QueueRequestComplete(type, &queued_request);

    // Complete function and do some basic checks.
    std::string access_token;
    std::set<std::string> granted_scopes;
    WaitForGetAuthTokenResults(func.get(), &access_token, &granted_scopes);
    EXPECT_EQ(kAccessToken, access_token);
    EXPECT_EQ(func->GetExtensionTokenKeyForTest()->scopes, granted_scopes);
  }
};

// Tests that Chrome uses the correct selected user id value when a gaia id was
// cached and only the primary account is signed in.
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionSelectedUserIdTest, SingleAccount) {
  auto extension = base::WrapRefCounted(CreateExtension(CLIENT_ID | SCOPES));
  SignIn("primary@example.com");
  CoreAccountInfo primary_account = GetPrimaryAccountInfo();

  SetCachedGaiaId(primary_account.gaia);
  RunNewFunctionAndExpectSelectedUserId(extension, primary_account.gaia);

  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

// Tests that Chrome uses the correct selected user id value when a gaia id was
// cached for a secondary account.
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionSelectedUserIdTest,
                       MultipleAccounts) {
  // This test requires the use of a secondary account. If extensions are
  // restricted to primary account only, this test wouldn't make too much sense.
  if (id_api()->AreExtensionsRestrictedToPrimaryAccount())
    return;

  auto extension = base::WrapRefCounted(CreateExtension(CLIENT_ID | SCOPES));
  SignIn("primary@example.com");
  AccountInfo secondary_account =
      identity_test_env()->MakeAccountAvailable("secondary@example.com");

  SetCachedGaiaId(secondary_account.gaia);
  RunNewFunctionAndExpectSelectedUserId(extension, secondary_account.gaia);

  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

// Tests that Chrome uses the correct selected user id value when a gaia id was
// cached but the extension specifies an account id for a different available
// account.
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionSelectedUserIdTest,
                       RequestedAccountAvailable) {
  // This test requires the use of a secondary account. If extensions are
  // restricted to primary account only, this test wouldn't make too much sense.
  if (id_api()->AreExtensionsRestrictedToPrimaryAccount())
    return;

  auto extension = base::WrapRefCounted(CreateExtension(CLIENT_ID | SCOPES));
  SignIn("primary@example.com");
  CoreAccountInfo primary_account = GetPrimaryAccountInfo();
  AccountInfo secondary_account =
      identity_test_env()->MakeAccountAvailable("secondary@example.com");

  SetCachedGaiaId(primary_account.gaia);
  // Run a new function with an account id specified in the arguments.
  RunNewFunctionAndExpectSelectedUserId(extension, secondary_account.gaia,
                                        secondary_account.gaia);

  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

// The signin flow is not used on Ash.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Tests that Chrome does not have any selected user id value if the account
// specified by the extension is not available.
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionSelectedUserIdTest,
                       RequestedAccountUnavailable) {
  // This test requires the use of a secondary account. If extensions are
  // restricted to primary account only, this test wouldn't make too much sense.
  if (id_api()->AreExtensionsRestrictedToPrimaryAccount())
    return;

  auto extension = base::WrapRefCounted(CreateExtension(CLIENT_ID | SCOPES));
  SignIn("primary@example.com");

  // Run a new function with an account id specified. Since this account is not
  // signed in, the login screen will be shown.
  auto func = base::MakeRefCounted<FakeGetAuthTokenFunction>();
  func->set_extension(extension);
  func->set_login_ui_result(true);
  RunFunctionAndExpectSelectedUserId(func, "",
                                     "gaia_id_for_unavailable_example.com");
  // The login ui still showed but another account was logged in instead.
  EXPECT_TRUE(func->login_ui_shown());

  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}

// Tests that Chrome uses the correct selected user id value after logging into
// the account requested by the extension.
IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionSelectedUserIdTest,
                       RequestedAccountLogin) {
  // This test requires the use of a secondary account. If extensions are
  // restricted to primary account only, this test wouldn't make too much sense.
  if (id_api()->AreExtensionsRestrictedToPrimaryAccount())
    return;

  auto extension = base::WrapRefCounted(CreateExtension(CLIENT_ID | SCOPES));
  SignIn("primary@example.com");

  // Run a new function with an account id specified. Since this account is not
  // signed in, the login screen will be shown.
  auto func = base::MakeRefCounted<FakeGetAuthTokenFunction>();
  func->set_extension(extension);
  func->set_login_ui_result(true);
  RunFunctionAndExpectSelectedUserId(func, "gaia_id_for_secondary_example.com",
                                     "gaia_id_for_secondary_example.com");
  EXPECT_TRUE(func->login_ui_shown());

  histogram_tester()->ExpectUniqueSample(
      kGetAuthTokenResultHistogramName, IdentityGetAuthTokenError::State::kNone,
      1);
}
#endif

IN_PROC_BROWSER_TEST_F(RemoveCachedAuthTokenFunctionTest, NotFound) {
  EXPECT_TRUE(InvalidateDefaultToken());
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
            GetCachedToken().status());
}

IN_PROC_BROWSER_TEST_F(RemoveCachedAuthTokenFunctionTest, RemoteConsent) {
  RemoteConsentResolutionData resolution_data;
  IdentityTokenCacheValue advice =
      IdentityTokenCacheValue::CreateRemoteConsent(resolution_data);
  SetCachedToken(advice);
  EXPECT_TRUE(InvalidateDefaultToken());
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_REMOTE_CONSENT,
            GetCachedToken().status());
}

IN_PROC_BROWSER_TEST_F(RemoveCachedAuthTokenFunctionTest, NonMatchingToken) {
  IdentityTokenCacheValue token =
      CreateToken("non_matching_token", base::Seconds(3600));
  SetCachedToken(token);
  EXPECT_TRUE(InvalidateDefaultToken());
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            GetCachedToken().status());
  EXPECT_EQ("non_matching_token", GetCachedToken().token());
}

IN_PROC_BROWSER_TEST_F(RemoveCachedAuthTokenFunctionTest, MatchingToken) {
  IdentityTokenCacheValue token =
      CreateToken(kAccessToken, base::Seconds(3600));
  SetCachedToken(token);
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            GetCachedToken().status());
  EXPECT_TRUE(InvalidateDefaultToken());
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
            GetCachedToken().status());
}

class LaunchWebAuthFlowFunctionTest : public AsyncExtensionBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    AsyncExtensionBrowserTest::SetUpCommandLine(command_line);
    // Reduce performance test variance by disabling background networking.
    command_line->AppendSwitch(switches::kDisableBackgroundNetworking);
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTest, InteractionRequired) {
  std::unique_ptr<net::EmbeddedTestServer> https_server = LaunchHttpsServer();
  GURL auth_url(https_server->GetURL("/interaction_required.html"));

  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function =
      CreateLaunchWebAuthFlowFunction();

  std::string args =
      "[{\"interactive\": false, \"url\": \"" + auth_url.spec() + "\"}]";
  std::string error = utils::RunFunctionAndReturnError(function.get(), args,
                                                       browser()->profile());

  EXPECT_EQ(std::string(errors::kInteractionRequired), error);
  histogram_tester()->ExpectUniqueSample(
      kLaunchWebAuthFlowResultHistogramName,
      IdentityLaunchWebAuthFlowFunction::Error::kInteractionRequired, 1);
}

// Checks that, by default, when a page fully loads in silent mode and doesn't
// redirect, `launchWebAuthFlow()` terminates with an error.
IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTest,
                       InteractionRequiredAfterLoad) {
  std::unique_ptr<net::EmbeddedTestServer> https_server = LaunchHttpsServer();
  GURL auth_url(https_server->GetURL("/redirect_after_load.html"));

  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function =
      CreateLaunchWebAuthFlowFunction();

  std::string args = base::StringPrintf(
      R"([{"interactive": false, "url": "%s"}])", auth_url.spec().c_str());
  std::string error = utils::RunFunctionAndReturnError(function.get(), args,
                                                       browser()->profile());

  EXPECT_EQ(errors::kInteractionRequired, error);
}

// Checks that when a page fully loads in silent mode and doesn't redirect,
// `launchWebAuthFlow()` terminates with an error after a specific timeout.
IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTest,
                       InteractionRequiredWithTimeout) {
  std::unique_ptr<net::EmbeddedTestServer> https_server = LaunchHttpsServer();
  GURL auth_url(https_server->GetURL("/interaction_required.html"));

  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function =
      CreateLaunchWebAuthFlowFunction();

  std::string args = base::StringPrintf(
      R"([{
        "interactive": false,
        "url": "%s",
        "abortOnLoadForNonInteractive": false,
        "timeoutMsForNonInteractive": 5000
      }])",
      auth_url.spec().c_str());
  std::string error = utils::RunFunctionAndReturnError(function.get(), args,
                                                       browser()->profile());

  // The function is expected to return `errors::kInteractionRequired` as the
  // page is expected to be loaded within the allotted time, but a race is still
  // possible where page load takes more than 5s, in which case
  // `errors::kPageLoadTimedOut` is returned. Accept both errors here because we
  // don't have a reliable way of sequencing page load before timeout in these
  // tests.
  EXPECT_TRUE(error == errors::kInteractionRequired ||
              error == errors::kPageLoadTimedOut);
}

// Checks that when a page fails to fully load before timeout in silent mode,
// `launchWebAuthFlow()` terminates with an error after a specific timeout.
IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTest, LoadTimedOut) {
  std::unique_ptr<net::EmbeddedTestServer> https_server = LaunchHttpsServer();
  GURL auth_url(https_server->GetURL("/interaction_required.html"));

  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function =
      CreateLaunchWebAuthFlowFunction();

  std::string args = base::StringPrintf(
      R"([{
        "interactive": false,
        "url": "%s",
        "abortOnLoadForNonInteractive": false,
        "timeoutMsForNonInteractive": 10
      }])",
      auth_url.spec().c_str());
  std::string error = utils::RunFunctionAndReturnError(function.get(), args,
                                                       browser()->profile());

  EXPECT_EQ(errors::kPageLoadTimedOut, error);
}

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTest, LoadFailed) {
  std::unique_ptr<net::EmbeddedTestServer> https_server = LaunchHttpsServer();
  GURL auth_url(https_server->GetURL("/five_hundred.html"));

  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function =
      CreateLaunchWebAuthFlowFunction();

  std::string args =
      "[{\"interactive\": true, \"url\": \"" + auth_url.spec() + "\"}]";
  std::string error = utils::RunFunctionAndReturnError(function.get(), args,
                                                       browser()->profile());

  EXPECT_EQ(std::string(errors::kPageLoadFailure), error);
  histogram_tester()->ExpectUniqueSample(
      kLaunchWebAuthFlowResultHistogramName,
      IdentityLaunchWebAuthFlowFunction::Error::kPageLoadFailure, 1);
}

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTest, NonInteractiveSuccess) {
  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function =
      CreateLaunchWebAuthFlowFunction();

  function->InitFinalRedirectURLDomainsForTest("abcdefghij");
  std::optional<base::Value> value = utils::RunFunctionAndReturnSingleResult(
      function.get(),
      "[{\"interactive\": false,"
      "\"url\": \"https://abcdefghij.chromiumapp.org/callback#test\"}]",
      browser()->profile());

  EXPECT_TRUE(value->is_string());
  EXPECT_EQ(std::string("https://abcdefghij.chromiumapp.org/callback#test"),
            value->GetString());
  histogram_tester()->ExpectUniqueSample(
      kLaunchWebAuthFlowResultHistogramName,
      IdentityLaunchWebAuthFlowFunction::Error::kNone, 1);
}

// Checks that when a page fully loads in silent mode and then performs a
// JavaScript redirect `launchWebAuthFlow()` finishes successfully.
IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTest,
                       NonInteractiveSuccessAfterLoad) {
  std::unique_ptr<net::EmbeddedTestServer> https_server = LaunchHttpsServer();
  GURL auth_url(https_server->GetURL("/redirect_after_load.html"));

  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function =
      CreateLaunchWebAuthFlowFunction();

  function->InitFinalRedirectURLDomainsForTest("abcdefghij");
  std::string args = base::StringPrintf(
      R"([{
        "interactive": false,
        "url": "%s",
        "abortOnLoadForNonInteractive": false,
        "timeoutMsForNonInteractive": 20000
      }])",
      auth_url.spec().c_str());
  std::optional<base::Value> value = utils::RunFunctionAndReturnSingleResult(
      function.get(), args, browser()->profile());

  EXPECT_TRUE(value->is_string());
  EXPECT_EQ("https://abcdefghij.chromiumapp.org/callback#test",
            value->GetString());
}

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTest,
                       InteractiveFirstNavigationSuccess) {
  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function =
      CreateLaunchWebAuthFlowFunction();

  function->InitFinalRedirectURLDomainsForTest("abcdefghij");
  std::optional<base::Value> value = utils::RunFunctionAndReturnSingleResult(
      function.get(),
      "[{\"interactive\": true,"
      "\"url\": \"https://abcdefghij.chromiumapp.org/callback#test\"}]",
      browser()->profile());

  EXPECT_TRUE(value->is_string());
  EXPECT_EQ(std::string("https://abcdefghij.chromiumapp.org/callback#test"),
            value->GetString());
  histogram_tester()->ExpectUniqueSample(
      kLaunchWebAuthFlowResultHistogramName,
      IdentityLaunchWebAuthFlowFunction::Error::kNone, 1);
}

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTest,
                       InteractiveSecondNavigationSuccess) {
  std::unique_ptr<net::EmbeddedTestServer> https_server = LaunchHttpsServer();
  GURL auth_url(https_server->GetURL("/redirect_to_chromiumapp.html"));

  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function =
      CreateLaunchWebAuthFlowFunction();

  function->InitFinalRedirectURLDomainsForTest("abcdefghij");
  std::string args =
      "[{\"interactive\": true, \"url\": \"" + auth_url.spec() + "\"}]";
  std::optional<base::Value> value = utils::RunFunctionAndReturnSingleResult(
      function.get(), args, browser()->profile());

  EXPECT_TRUE(value->is_string());
  EXPECT_EQ(std::string("https://abcdefghij.chromiumapp.org/callback#test"),
            value->GetString());
  histogram_tester()->ExpectUniqueSample(
      kLaunchWebAuthFlowResultHistogramName,
      IdentityLaunchWebAuthFlowFunction::Error::kNone, 1);
}

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTest, UserCloseWindow) {
  std::unique_ptr<net::EmbeddedTestServer> https_server = LaunchHttpsServer();
  GURL auth_url(https_server->GetURL("/interaction_required.html"));

  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function =
      CreateLaunchWebAuthFlowFunction();

  content::TestNavigationObserver url_obvserver(auth_url);
  url_obvserver.StartWatchingNewWebContents();

  std::string args =
      "[{\"interactive\": true, \"url\": \"" + auth_url.spec() + "\"}]";
  RunFunctionAsync(function.get(), args);

  url_obvserver.Wait();

  Browser* popup_browser = chrome::FindBrowserWithTab(
      function->GetWebAuthFlowForTesting()->web_contents());
  TabStripModel* tabs = popup_browser->tab_strip_model();
  EXPECT_NE(browser(), popup_browser);
  ASSERT_EQ(tabs->GetActiveWebContents()->GetURL(), auth_url);
  // Close the opened auth web contents.
  tabs->CloseWebContentsAt(tabs->active_index(), 0);

  EXPECT_EQ(std::string(errors::kUserRejected), WaitForError(function.get()));
  histogram_tester()->ExpectUniqueSample(
      kLaunchWebAuthFlowResultHistogramName,
      IdentityLaunchWebAuthFlowFunction::Error::kUserRejected, 1);
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTest, ProfileShutDown) {
  std::unique_ptr<net::EmbeddedTestServer> https_server =
      std::make_unique<net::EmbeddedTestServer>(
          net::EmbeddedTestServer::TYPE_HTTPS);
  net::test_server::RegisterDefaultHandlers(https_server.get());
  EXPECT_TRUE(https_server->Start());
  // We want to interrupt the flow before `auth_url` gets loaded. To ensure that
  // an URL doesn't load prematurely, use a default test URL that never returns
  // a response.
  GURL auth_url(https_server->GetURL("/hung"));
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::BROWSER, KeepAliveRestartOption::DISABLED);
  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function =
      CreateLaunchWebAuthFlowFunction();
  std::string args =
      "[{\"interactive\": true, \"url\": \"" + auth_url.spec() + "\"}]";
  RunFunctionAsync(function.get(), args);
  CloseBrowserSynchronously(browser());

  // Because the navigation to auth_url is still ongoing when profile shutdown
  // starts, it will be canceled before proceeding with shutdown, and hence the
  // error message below will reflect a canceled navigation.
  EXPECT_EQ(std::string(errors::kPageLoadFailure),
            WaitForError(function.get()));
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(keep_alive));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Regression test for http://b/290733700.
IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTest,
                       SchemeOtherThanHttpOrHttpsNotAllowed) {
  // Only http and https schemes are allowed.
  GURL invalid_auth_url("chrome-untrusted://some_chrome_url");
  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function =
      CreateLaunchWebAuthFlowFunction();

  std::string args =
      "[{\"interactive\": true, \"url\": \"" + invalid_auth_url.spec() + "\"}]";
  RunFunctionAsync(function.get(), args);

  EXPECT_EQ(std::string(errors::kInvalidURLScheme),
            WaitForError(function.get()));
  histogram_tester()->ExpectUniqueSample(
      kLaunchWebAuthFlowResultHistogramName,
      IdentityLaunchWebAuthFlowFunction::Error::kInvalidURLScheme, 1);
}

class LaunchWebAuthFlowFunctionTestWithBrowserTab
    : public LaunchWebAuthFlowFunctionTest {
 protected:
  void RunFunctionAndWaitForNavigation(
      IdentityLaunchWebAuthFlowFunction* function,
      const GURL& url,
      const std::string& args) {
    content::TestNavigationObserver url_observer(url);
    url_observer.StartWatchingNewWebContents();
    RunFunctionAsync(function, args);
    url_observer.Wait();
  }
};

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTestWithBrowserTab,
                       PageNavigateFromInitURLToFinalURL) {
  std::unique_ptr<net::EmbeddedTestServer> https_server = LaunchHttpsServer();
  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function =
      CreateLaunchWebAuthFlowFunction();

  const std::string extension_id("abcdefghij");
  function->InitFinalRedirectURLDomainsForTest(extension_id);

  const GURL auth_url(https_server->GetURL("/consent_page.html"));
  const GURL final_url("https://" + extension_id + ".chromiumapp.org/");

  const std::string args =
      "[{\"interactive\": true, \"url\": \"" + auth_url.spec() + "\"}]";
  RunFunctionAndWaitForNavigation(function.get(), auth_url, args);

  SimulateUrlRedirect(extension_id,
                      function->GetWebAuthFlowForTesting()->web_contents());

  base::Value output;
  WaitForOneResult(function.get(), &output);
  EXPECT_FALSE(function->GetWebAuthFlowForTesting());
  EXPECT_EQ(GURL(output.GetString()).Resolve("/"), final_url);
  histogram_tester()->ExpectUniqueSample(
      kLaunchWebAuthFlowResultHistogramName,
      IdentityLaunchWebAuthFlowFunction::Error::kNone, 1);
}

class TestDelegate : public LaunchWebAuthFlowDelegate {
 public:
  // LaunchWebAuthFlowDelegate:
  void GetOptionalWindowBounds(
      Profile* profile,
      const std::string& extension_id,
      base::OnceCallback<void(std::optional<gfx::Rect>)> callback) override {
    std::move(callback).Run(kTestBounds);
  }

  static constexpr gfx::Rect kTestBounds = gfx::Rect(23, 27, 400, 400);
};

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTestWithBrowserTab,
                       PopupBoundsComeFromDelegate) {
  std::unique_ptr<net::EmbeddedTestServer> https_server = LaunchHttpsServer();
  GURL auth_url(https_server->GetURL("/interaction_required.html"));

  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function =
      CreateLaunchWebAuthFlowFunction();
  function->SetLaunchWebAuthFlowDelegateForTesting(
      std::make_unique<TestDelegate>());

  ui_test_utils::BrowserChangeObserver browser_opened(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);

  std::string args =
      "[{\"interactive\": true, \"url\": \"" + auth_url.spec() + "\"}]";
  RunFunctionAsync(function.get(), args);

  Browser* popup_browser = browser_opened.Wait();

  gfx::Rect bounds = popup_browser->window()->GetBounds();
  EXPECT_EQ(bounds.x(), TestDelegate::kTestBounds.x());
  EXPECT_EQ(bounds.y(), TestDelegate::kTestBounds.y());
  // The final width and height can contain platform-specific offsets for the
  // window title bar, which we don't want to assert exactly here.
  EXPECT_GE(bounds.width(), TestDelegate::kTestBounds.width());
  EXPECT_GE(bounds.height(), TestDelegate::kTestBounds.height());
}

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTestWithBrowserTab,
                       PageNavigateFromInitURLToCustomFinalURL) {
  std::unique_ptr<net::EmbeddedTestServer> https_server = LaunchHttpsServer();
  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function =
      CreateLaunchWebAuthFlowFunction();

  const GURL auth_url(https_server->GetURL("/consent_page.html"));
  const GURL final_url("example://example.com/");

  const std::string args =
      "[{\"interactive\": true, \"url\": \"" + auth_url.spec() + "\"}]";

  browser()->profile()->GetPrefs()->SetDict(
      extensions::pref_names::kOAuthRedirectUrls,
      base::Value::Dict().Set(function->extension()->id(),
                              base::Value::List().Append(final_url.spec())));
  RunFunctionAndWaitForNavigation(function.get(), auth_url, args);

  SimulateCustomUrlRedirect(
      final_url.spec() + "#some_information",
      function->GetWebAuthFlowForTesting()->web_contents());

  base::Value output;
  WaitForOneResult(function.get(), &output);
  EXPECT_FALSE(function->GetWebAuthFlowForTesting());
  EXPECT_EQ(GURL(output.GetString()).Resolve("/"), final_url);
  histogram_tester()->ExpectUniqueSample(
      kLaunchWebAuthFlowResultHistogramName,
      IdentityLaunchWebAuthFlowFunction::Error::kNone, 1);
}

// TODO(crbug.com/40259192): This test should be adapted after the
// implementation of the bug. Multiple TODOs in the test to fix.
IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTestWithBrowserTab,
                       SimilarExtensionAndArgsShouldGenerateSameFlow) {
  std::unique_ptr<net::EmbeddedTestServer> https_server = LaunchHttpsServer();
  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function1 =
      CreateLaunchWebAuthFlowFunction();
  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function2 =
      CreateLaunchWebAuthFlowFunction();

  const std::string extension_id("final_url");
  function1->InitFinalRedirectURLDomainsForTest(extension_id);
  function2->InitFinalRedirectURLDomainsForTest(extension_id);

  const GURL auth_url(https_server->GetURL("/consent_page.html"));
  const GURL final_url("https://" + extension_id + ".chromiumapp.org/");

  // Same args used in both functions.
  const std::string args =
      "[{\"interactive\": true, \"url\": \"" + auth_url.spec() + "\"}]";

  // Activate function1.
  RunFunctionAndWaitForNavigation(function1.get(), auth_url, args);
  // Activate function2.
  RunFunctionAndWaitForNavigation(function2.get(), auth_url, args);

  content::WebContents* consent_web_contents1 =
      function1->GetWebAuthFlowForTesting()->web_contents();
  content::WebContents* consent_web_contents2 =
      function2->GetWebAuthFlowForTesting()->web_contents();
  // TODO(crbug.com/40259192): These two should be equal, EXPECT_EQ.
  EXPECT_NE(consent_web_contents1, consent_web_contents2);

  // `SimulateUrlRedirect()` on first action should not affect the second
  // function.
  SimulateUrlRedirect(extension_id, consent_web_contents1);

  base::Value output1;
  WaitForOneResult(function1.get(), &output1);
  EXPECT_FALSE(function1->GetWebAuthFlowForTesting());
  // TODO(crbug.com/40259192): This should be EXPECT_FALSE.
  EXPECT_TRUE(function2->GetWebAuthFlowForTesting());
  EXPECT_TRUE(output1.GetString().find(final_url.spec()) != std::string::npos);
  EXPECT_TRUE(output1.GetString().find("#access_token="));

  // TODO(crbug.com/40259192): 2 samples should be recorded instead of 1.
  histogram_tester()->ExpectUniqueSample(
      kLaunchWebAuthFlowResultHistogramName,
      IdentityLaunchWebAuthFlowFunction::Error::kNone, 1);
}

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTestWithBrowserTab,
                       DifferentExtensionsShouldGenerateDifferentFlows) {
  std::unique_ptr<net::EmbeddedTestServer> https_server = LaunchHttpsServer();
  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function1 =
      CreateLaunchWebAuthFlowFunction();
  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function2 =
      CreateLaunchWebAuthFlowFunction();

  const std::string extension_id1("extension1");
  function1->InitFinalRedirectURLDomainsForTest(extension_id1);
  const std::string extension_id2("extension2");
  function2->InitFinalRedirectURLDomainsForTest(extension_id2);

  const GURL auth_url(https_server->GetURL("/consent_page.html"));
  // Different final_urls.
  const GURL final_url1("https://" + extension_id1 + ".chromiumapp.org/");
  const GURL final_url2("https://" + extension_id2 + ".chromiumapp.org/");

  // Same args used in both functions.
  const std::string args =
      "[{\"interactive\": true, \"url\": \"" + auth_url.spec() + "\"}]";

  RunFunctionAndWaitForNavigation(function1.get(), auth_url, args);
  RunFunctionAndWaitForNavigation(function2.get(), auth_url, args);

  content::WebContents* consent_web_contents1 =
      function1->GetWebAuthFlowForTesting()->web_contents();
  content::WebContents* consent_web_contents2 =
      function2->GetWebAuthFlowForTesting()->web_contents();
  EXPECT_NE(consent_web_contents1, consent_web_contents2);

  const std::string& current_consent_url2 =
      consent_web_contents2->GetURL().spec();

  // SimulateConsent on first action should not affect the second function.
  SimulateUrlRedirect(extension_id1, consent_web_contents1);

  base::Value output1;
  WaitForOneResult(function1.get(), &output1);
  // `function2` state should remain.
  EXPECT_EQ(current_consent_url2, consent_web_contents2->GetURL().spec());
  EXPECT_FALSE(function1->GetWebAuthFlowForTesting());
  EXPECT_TRUE(output1.GetString().find(final_url1.spec()) != std::string::npos);

  SimulateUrlRedirect(extension_id2, consent_web_contents2);

  base::Value output2;
  WaitForOneResult(function2.get(), &output2);
  EXPECT_FALSE(function2->GetWebAuthFlowForTesting());
  EXPECT_TRUE(output2.GetString().find(final_url2.spec()) != std::string::npos);

  histogram_tester()->ExpectUniqueSample(
      kLaunchWebAuthFlowResultHistogramName,
      IdentityLaunchWebAuthFlowFunction::Error::kNone, 2);
}

// TODO(crbug.com/40259192): This test should be adapted after the
// implementation of the bug.
IN_PROC_BROWSER_TEST_F(
    LaunchWebAuthFlowFunctionTestWithBrowserTab,
    ExtensionWithDifferentArgsShouldGenerateDifferentFlowsInAQueue) {
  std::unique_ptr<net::EmbeddedTestServer> https_server = LaunchHttpsServer();
  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function1 =
      CreateLaunchWebAuthFlowFunction();
  scoped_refptr<IdentityLaunchWebAuthFlowFunction> function2 =
      CreateLaunchWebAuthFlowFunction();

  const std::string extension_id("extension");
  function1->InitFinalRedirectURLDomainsForTest(extension_id);

  const GURL auth_url1(https_server->GetURL("/consent_page.html"));
  const GURL auth_url2(https_server->GetURL("/interaction_required.html"));
  const GURL final_url("https://" + extension_id + ".chromiumapp.org/");

  const std::string args1 =
      "[{\"interactive\": true, \"url\": \"" + auth_url1.spec() + "\"}]";
  const std::string args2 =
      "[{\"interactive\": true, \"url\": \"" + auth_url2.spec() + "\"}]";

  RunFunctionAndWaitForNavigation(function1.get(), auth_url1, args1);
  RunFunctionAndWaitForNavigation(function2.get(), auth_url2, args2);

  content::WebContents* consent_web_contents1 =
      function1->GetWebAuthFlowForTesting()->web_contents();
  content::WebContents* consent_web_contents2 =
      function2->GetWebAuthFlowForTesting()->web_contents();
  // TODO(crbug.com/40259192): `function2->GetWebAuthFlowForTesting()` should be
  // null after the changes since it would be in a queue.
  EXPECT_NE(consent_web_contents1, consent_web_contents2);

  const std::string& current_consent_url2 =
      consent_web_contents2->GetURL().spec();

  // SimulateConsent on first action should not affect the second function.
  SimulateUrlRedirect(extension_id, consent_web_contents1);

  base::Value output1;
  WaitForOneResult(function1.get(), &output1);
  // `function2` state should remain.
  EXPECT_EQ(current_consent_url2, consent_web_contents2->GetURL().spec());
  EXPECT_FALSE(function1->GetWebAuthFlowForTesting());
  EXPECT_TRUE(output1.GetString().find(final_url.spec()) != std::string::npos);

  // TODO(crbug.com/40259192): function2 should now run, check for that once the
  // queue is implemented.

  histogram_tester()->ExpectUniqueSample(
      kLaunchWebAuthFlowResultHistogramName,
      IdentityLaunchWebAuthFlowFunction::Error::kNone, 1);
}

class ClearAllCachedAuthTokensFunctionTest : public AsyncExtensionBrowserTest {
 public:
  void SetUpOnMainThread() override {
    AsyncExtensionBrowserTest::SetUpOnMainThread();
    base::FilePath manifest_path =
        test_data_dir_.AppendASCII("api_test/identity/oauth2");
    extension_ = LoadExtension(manifest_path);
  }

  void TearDownOnMainThread() override {
    // We must clear the extension_ raw_ptr before browser shutdown is
    // initiated. Otherwise, it will become dangling after extensions are
    // unloaded during shutdown.
    extension_ = nullptr;
    AsyncExtensionBrowserTest::TearDownOnMainThread();
  }

  const Extension* extension() { return extension_; }

  bool RunClearAllCachedAuthTokensFunction() {
    auto function =
        base::MakeRefCounted<IdentityClearAllCachedAuthTokensFunction>();
    function->set_extension(extension_.get());
    return utils::RunFunction(function.get(), "[]", browser()->profile(),
                              api_test_utils::FunctionMode::kNone);
  }

  IdentityAPI* id_api() {
    return IdentityAPI::GetFactoryInstance()->Get(browser()->profile());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<const Extension> extension_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(ClearAllCachedAuthTokensFunctionTest,
                       EraseCachedGaiaId) {
  id_api()->SetGaiaIdForExtension(extension()->id(), "test_gaia");
  EXPECT_EQ("test_gaia", id_api()->GetGaiaIdForExtension(extension()->id()));
  ASSERT_TRUE(RunClearAllCachedAuthTokensFunction());
  EXPECT_FALSE(id_api()->GetGaiaIdForExtension(extension()->id()).has_value());
}

IN_PROC_BROWSER_TEST_F(ClearAllCachedAuthTokensFunctionTest,
                       EraseCachedTokens) {
  ExtensionTokenKey token_key(extension()->id(), CoreAccountInfo(), {"foo"});
  id_api()->token_cache()->SetToken(
      token_key, IdentityTokenCacheValue::CreateToken("access_token", {"foo"},
                                                      base::Seconds(3600)));
  EXPECT_NE(IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
            id_api()->token_cache()->GetToken(token_key).status());
  ASSERT_TRUE(RunClearAllCachedAuthTokensFunction());
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
            id_api()->token_cache()->GetToken(token_key).status());
}

class OnSignInChangedEventTest : public IdentityTestWithSignin {
 protected:
  void SetUpOnMainThread() override {
    // TODO(blundell): Ideally we would test fully end-to-end by injecting a
    // JavaScript extension listener and having that listener do the
    // verification, but it's not clear how to set that up.
    id_api()->set_on_signin_changed_callback_for_testing(
        base::BindRepeating(&OnSignInChangedEventTest::OnSignInEventChanged,
                            base::Unretained(this)));

    IdentityTestWithSignin::SetUpOnMainThread();
  }

  IdentityAPI* id_api() {
    return IdentityAPI::GetFactoryInstance()->Get(browser()->profile());
  }

  // Adds an event that is expected to fire. Events are unordered, i.e., when an
  // event fires it will be checked against all of the expected events that have
  // been added. This is because the order of multiple events firing due to the
  // same underlying state change is undefined in the
  // chrome.identity.onSignInEventChanged() API.
  void AddExpectedEvent(base::Value::List args) {
    expected_events_.insert(
        std::make_unique<Event>(events::IDENTITY_ON_SIGN_IN_CHANGED,
                                api::identity::OnSignInChanged::kEventName,
                                std::move(args), browser()->profile()));
  }

  bool HasExpectedEvent() { return !expected_events_.empty(); }

 private:
  void OnSignInEventChanged(Event* event) {
    ASSERT_TRUE(HasExpectedEvent());

    // Search for |event| in the set of expected events.
    bool found_event = false;
    const auto& event_args = event->event_args;
    for (const auto& expected_event : expected_events_) {
      EXPECT_EQ(expected_event->histogram_value, event->histogram_value);
      EXPECT_EQ(expected_event->event_name, event->event_name);

      const auto& expected_event_args = expected_event->event_args;
      if (event_args != expected_event_args)
        continue;

      expected_events_.erase(expected_event);
      found_event = true;
      break;
    }

    if (!found_event) {
      EXPECT_TRUE(false) << "Received bad event:";

      LOG(INFO) << "Was expecting events with these args:";

      for (const auto& expected_event : expected_events_) {
        LOG(INFO) << expected_event->event_args;
      }

      LOG(INFO) << "But received event with different args:";
      LOG(INFO) << event_args;
    }
  }

  std::set<std::unique_ptr<Event>> expected_events_;
};

// Test that an event is fired when the primary account signs in.
IN_PROC_BROWSER_TEST_F(OnSignInChangedEventTest, FireOnPrimaryAccountSignIn) {
  api::identity::AccountInfo account_info;
  account_info.id = "gaia_id_for_primary_example.com";
  AddExpectedEvent(api::identity::OnSignInChanged::Create(account_info, true));

  // Sign in and verify that the callback fires.
  SignIn("primary@example.com");

  EXPECT_FALSE(HasExpectedEvent());
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Test that an event is fired when the primary account signs out. Only
// applicable in non-DICE mode, as when DICE is enabled clearing the primary
// account does not result in its refresh token being removed and hence does
// not trigger an event to fire.
IN_PROC_BROWSER_TEST_F(OnSignInChangedEventTest, FireOnPrimaryAccountSignOut) {
  if (AccountConsistencyModeManager::IsDiceEnabledForProfile(profile()))
    return;

  api::identity::AccountInfo account_info;
  account_info.id = "gaia_id_for_primary_example.com";
  AddExpectedEvent(api::identity::OnSignInChanged::Create(account_info, true));

  SignIn("primary@example.com");

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Clear primary account is not allowed in Lacros main profile.
  // This test overrides |UserSignoutSetting| to test Lacros secondary profile.
  ChromeSigninClientFactory::GetForProfile(profile())
      ->set_is_clear_primary_account_allowed_for_testing(
          SigninClient::SignoutDecision::ALLOW);
#endif
  AddExpectedEvent(api::identity::OnSignInChanged::Create(account_info, false));

  // Sign out and verify that the callback fires.
  identity_test_env()->ClearPrimaryAccount();

  EXPECT_FALSE(HasExpectedEvent());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Test that an event is fired when the primary account has a refresh token
// invalidated.
IN_PROC_BROWSER_TEST_F(OnSignInChangedEventTest,
                       FireOnPrimaryAccountRefreshTokenInvalidated) {
  api::identity::AccountInfo account_info;
  account_info.id = "gaia_id_for_primary_example.com";
  AddExpectedEvent(api::identity::OnSignInChanged::Create(account_info, true));

  CoreAccountId primary_account_id = SignIn("primary@example.com");

  AddExpectedEvent(api::identity::OnSignInChanged::Create(account_info, true));

  // Revoke the refresh token and verify that the callback fires.
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();

  EXPECT_FALSE(HasExpectedEvent());
}

// Test that an event is fired when the primary account has a refresh token
// newly available.
IN_PROC_BROWSER_TEST_F(OnSignInChangedEventTest,
                       FireOnPrimaryAccountRefreshTokenAvailable) {
  api::identity::AccountInfo account_info;
  account_info.id = "gaia_id_for_primary_example.com";
  AddExpectedEvent(api::identity::OnSignInChanged::Create(account_info, true));

  CoreAccountId primary_account_id = SignIn("primary@example.com");

  AddExpectedEvent(api::identity::OnSignInChanged::Create(account_info, true));
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();

  account_info.id = "gaia_id_for_primary_example.com";
  AddExpectedEvent(api::identity::OnSignInChanged::Create(account_info, true));

  // Make the primary account available again and check that the callback fires.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();
  EXPECT_FALSE(HasExpectedEvent());
}

// Test that an event is fired for changes to a secondary account.
IN_PROC_BROWSER_TEST_F(OnSignInChangedEventTest, FireForSecondaryAccount) {
  api::identity::AccountInfo account_info;
  account_info.id = "gaia_id_for_primary_example.com";
  AddExpectedEvent(api::identity::OnSignInChanged::Create(account_info, true));
  SignIn("primary@example.com");

  account_info.id = "gaia_id_for_secondary_example.com";
  AddExpectedEvent(api::identity::OnSignInChanged::Create(account_info, true));

  // Make a secondary account available again and check that the callback fires.
  CoreAccountId secondary_account_id =
      identity_test_env()
          ->MakeAccountAvailable("secondary@example.com")
          .account_id;
  EXPECT_FALSE(HasExpectedEvent());

  // Revoke the secondary account's refresh token and check that the callback
  // fires.
  AddExpectedEvent(api::identity::OnSignInChanged::Create(account_info, false));

  identity_test_env()->RemoveRefreshTokenForAccount(secondary_account_id);
  EXPECT_FALSE(HasExpectedEvent());
}

// Tests the chrome.identity API implemented by custom JS bindings.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ChromeIdentityJsBindings) {
  ASSERT_TRUE(RunExtensionTest("identity/js_bindings")) << message_;
}

}  // namespace extensions

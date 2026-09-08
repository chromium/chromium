// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_uploader_chrome.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "build/chromeos_buildflags.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_uploader.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(PLATFORM_CFM)
#include "base/check_deref.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#endif  // BUILDFLAG(PLATFORM_CFM)

namespace feedback {

namespace {

constexpr char kAuthenticationErrorLogMessage[] =
    "Feedback report will be sent without authentication.";

void QueueSingleReport(base::WeakPtr<feedback::FeedbackUploader> uploader,
                       scoped_refptr<FeedbackReport> report) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&FeedbackUploaderChrome::RequeueReport,
                                std::move(uploader), std::move(report)));
}

// Helper function to create an URLLoaderFactory for the FeedbackUploader from
// the BrowserContext storage partition. As creating the storage partition can
// be expensive, this is delayed so that it does not happen during startup.
scoped_refptr<network::SharedURLLoaderFactory>
CreateURLLoaderFactoryForBrowserContext(content::BrowserContext* context) {
  return context->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

}  // namespace

#if BUILDFLAG(PLATFORM_CFM)

class FeedbackUploaderChrome::ActiveAccountAccessTokenFetcher
    : OAuth2AccessTokenManager::Consumer {
 public:
  using ActiveAccountAccessTokenCallback = base::OnceCallback<void(
      base::expected<std::string /* access_token */, GoogleServiceAuthError>)>;

  ActiveAccountAccessTokenFetcher(
      DeviceOAuth2TokenService* device_token_service,
      ActiveAccountAccessTokenCallback callback)
      : OAuth2AccessTokenManager::Consumer(kAccessTokenConsumer),
        callback_(std::move(callback)) {
    CHECK(device_token_service);

    const OAuth2AccessTokenManager::ScopeSet scopes{
        GaiaConstants::kSupportContentOAuth2Scope};

    access_token_request_ =
        device_token_service->StartAccessTokenRequest(scopes, this);
  }

  ActiveAccountAccessTokenFetcher(const ActiveAccountAccessTokenFetcher&) =
      delete;
  ActiveAccountAccessTokenFetcher& operator=(
      const ActiveAccountAccessTokenFetcher&) = delete;

  ~ActiveAccountAccessTokenFetcher() override = default;

 private:
  static constexpr char kAccessTokenConsumer[] = "feedback_uploader";

  // OAuth2AccessTokenManager::Consumer:
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override {
    HandleTokenRequestCompletion(request, token_response.access_token);
  }
  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override {
    HandleTokenRequestCompletion(request, base::unexpected(error));
  }

  void HandleTokenRequestCompletion(
      const OAuth2AccessTokenManager::Request* request,
      base::expected<std::string, GoogleServiceAuthError> access_token) {
    access_token_request_.reset();
    std::move(callback_).Run(std::move(access_token));
  }

  ActiveAccountAccessTokenCallback callback_;
  std::unique_ptr<OAuth2AccessTokenManager::Request> access_token_request_;
};

#endif  // BUILDFLAG(PLATFORM_CFM)

FeedbackUploaderChrome::FeedbackUploaderChrome(content::BrowserContext* context)
    // The FeedbackUploaderChrome lifetime is bound to that of BrowserContext
    // by the KeyedServiceFactory infrastructure. The FeedbackUploaderChrome
    // will be destroyed before the BrowserContext, thus base::Unretained()
    // usage is safe.
    : FeedbackUploader(/*is_off_the_record=*/false,
                       context->GetPath(),
                       base::BindOnce(&CreateURLLoaderFactoryForBrowserContext,
                                      base::Unretained(context))),
      context_(context) {
  DCHECK(!context_->IsOffTheRecord());

  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&FeedbackReport::LoadReportsAndQueue,
                     feedback_reports_path(),
                     base::BindRepeating(&QueueSingleReport,
                                         weak_ptr_factory_.GetWeakPtr())));
}

FeedbackUploaderChrome::~FeedbackUploaderChrome() = default;

base::WeakPtr<FeedbackUploader> FeedbackUploaderChrome::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FeedbackUploaderChrome::PrimaryAccountAccessTokenAvailable(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK(primary_account_token_fetcher_);
  primary_account_token_fetcher_.reset();
  AccessTokenAvailable(error, access_token_info.token);
}

#if BUILDFLAG(PLATFORM_CFM)
void FeedbackUploaderChrome::ActiveAccountAccessTokenAvailable(
    base::expected<std::string, GoogleServiceAuthError> access_token) {
  DCHECK(active_account_token_fetcher_);
  active_account_token_fetcher_.reset();
  AccessTokenAvailable(
      access_token.error_or(GoogleServiceAuthError::AuthErrorNone()),
      access_token.value_or(std::string()));
}
#endif  // BUILDFLAG(PLATFORM_CFM)

void FeedbackUploaderChrome::AccessTokenAvailable(GoogleServiceAuthError error,
                                                  std::string token) {
  if (error.state() == GoogleServiceAuthError::NONE) {
    DCHECK(!token.empty());
    access_token_ = std::move(token);
  } else {
    LOG(ERROR) << "Failed to get the access token. "
               << kAuthenticationErrorLogMessage;
  }

  FeedbackUploader::StartDispatchingReport();
}

void FeedbackUploaderChrome::StartDispatchingReport() {
  if (delegate_)
    delegate_->OnStartDispatchingReport();

  access_token_.clear();

  // TODO(crbug.com/40579328): Instead of getting the IdentityManager from the
  // profile, we should pass the IdentityManager to FeedbackUploaderChrome's
  // ctor.
  Profile* profile = Profile::FromBrowserContext(context_);
  DCHECK(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  // Sync consent is not required to send feedback because the feedback dialog
  // has its own privacy notice.
  if (identity_manager &&
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    primary_account_token_fetcher_ =
        std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
            signin::OAuthConsumerId::kFeedbackUploader, identity_manager,
            base::BindOnce(
                &FeedbackUploaderChrome::PrimaryAccountAccessTokenAvailable,
                base::Unretained(this)),
            signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
            signin::ConsentLevel::kSignin);
    return;
  }

#if BUILDFLAG(PLATFORM_CFM)
  // CFM Devices may need to acquire the auth token for their robot account
  // before they submit feedback.

  DeviceOAuth2TokenService* device_token_service =
      DeviceOAuth2TokenServiceFactory::Get();
  CHECK(device_token_service);

  const bool is_meet_device =
      policy::EnrollmentRequisitionManager::IsMeetDevice(
          CHECK_DEREF(g_browser_process->local_state()));
  const bool has_robot_account =
      !device_token_service->GetRobotAccountId().empty();

  if (is_meet_device && has_robot_account) {
    active_account_token_fetcher_ =
        std::make_unique<ActiveAccountAccessTokenFetcher>(
            device_token_service,
            base::BindOnce(
                &FeedbackUploaderChrome::ActiveAccountAccessTokenAvailable,
                base::Unretained(this)));
    return;
  }
#endif  // BUILDFLAG(PLATFORM_CFM)

  LOG(ERROR) << "Failed to request oauth access token. "
             << kAuthenticationErrorLogMessage;
  FeedbackUploader::StartDispatchingReport();
}

void FeedbackUploaderChrome::AppendExtraHeadersToUploadRequest(
    network::ResourceRequest* resource_request) {
  if (access_token_.empty())
    return;

  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf("Bearer %s", access_token_.c_str()));
}

}  // namespace feedback

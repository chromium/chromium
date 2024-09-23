// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_uploader_chrome.h"

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "build/chromeos_buildflags.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/feedback/feedback_report.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/network/public/cpp/resource_request.h"

#if BUILDFLAG(PLATFORM_CFM)
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/device_identity/device_identity_provider.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#endif  // BUILDFLAG(PLATFORM_CFM)

namespace feedback {

namespace {

constexpr char kAuthenticationErrorLogMessage[] =
    "Feedback report will be sent without authentication.";

constexpr char kConsumer[] = "feedback_uploader_chrome";

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
    GoogleServiceAuthError error,
    std::string token) {
  DCHECK(active_account_token_fetcher_);
  active_account_token_fetcher_.reset();
  AccessTokenAvailable(error, token);
}
#endif  // BUILDFLAG(PLATFORM_CFM)

void FeedbackUploaderChrome::AccessTokenAvailable(GoogleServiceAuthError error,
                                                  std::string token) {
  if (error.state() == GoogleServiceAuthError::NONE) {
    DCHECK(!token.empty());
    access_token_ = token;
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
    signin::ScopeSet scopes;
    scopes.insert(GaiaConstants::kSupportContentOAuth2Scope);
    primary_account_token_fetcher_ =
        std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
            kConsumer, identity_manager, scopes,
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
  DeviceOAuth2TokenService* deviceTokenService =
      DeviceOAuth2TokenServiceFactory::Get();
  DCHECK(deviceTokenService);
  auto device_identity_provider =
      std::make_unique<DeviceIdentityProvider>(deviceTokenService);

  // Flag indicating that a device was intended to be used as a CFM.
  bool isMeetDevice =
      policy::EnrollmentRequisitionManager::IsRemoraRequisition();
  if (isMeetDevice && !device_identity_provider->GetActiveAccountId().empty()) {
    OAuth2AccessTokenManager::ScopeSet scopes;
    scopes.insert(GaiaConstants::kSupportContentOAuth2Scope);
    active_account_token_fetcher_ = device_identity_provider->FetchAccessToken(
        kConsumer, scopes,
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

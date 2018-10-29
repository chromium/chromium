// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_uploader_chrome.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "net/url_request/url_fetcher.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_access_token_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace feedback {

namespace {

constexpr char kAuthenticationErrorLogMessage[] =
    "Feedback report will be sent without authentication.";

}  // namespace

FeedbackUploaderChrome::FeedbackUploaderChrome(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    content::BrowserContext* context,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : FeedbackUploader(url_loader_factory, context, task_runner) {}

FeedbackUploaderChrome::~FeedbackUploaderChrome() = default;

void FeedbackUploaderChrome::AccessTokenAvailable(
    GoogleServiceAuthError error,
    identity::AccessTokenInfo access_token_info) {
  DCHECK(token_fetcher_);
  token_fetcher_.reset();
  if (error.state() == GoogleServiceAuthError::NONE) {
    DCHECK(!access_token_info.token.empty());
    access_token_ = access_token_info.token;
  } else {
    LOG(ERROR) << "Failed to get the access token. "
               << kAuthenticationErrorLogMessage;
  }
  FeedbackUploader::StartDispatchingReport();
}

void FeedbackUploaderChrome::StartDispatchingReport() {
  access_token_.clear();

  // TODO(crbug.com/849591): Instead of getting the IdentityManager from the
  // profile, we should pass the IdentityManager to FeedbackUploaderChrome's
  // ctor.
  Profile* profile = Profile::FromBrowserContext(context());
  DCHECK(profile);
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  if (identity_manager && identity_manager->HasPrimaryAccount()) {
    identity::ScopeSet scopes;
    scopes.insert("https://www.googleapis.com/auth/supportcontent");
    token_fetcher_ =
        std::make_unique<identity::PrimaryAccountAccessTokenFetcher>(
            "feedback_uploader_chrome", identity_manager, scopes,
            base::BindOnce(&FeedbackUploaderChrome::AccessTokenAvailable,
                           base::Unretained(this)),
            identity::PrimaryAccountAccessTokenFetcher::Mode::kImmediate);
    return;
  }

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

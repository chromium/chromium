// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_uploader_chrome.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/feedback/feedback_report.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace feedback {

namespace {

constexpr char kAuthenticationErrorLogMessage[] =
    "Feedback report will be sent without authentication.";

void QueueSingleReport(base::WeakPtr<feedback::FeedbackUploader> uploader,
                       scoped_refptr<FeedbackReport> report) {
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&FeedbackUploaderChrome::RequeueReport,
                                std::move(uploader), std::move(report)));
}

}  // namespace

FeedbackUploaderChrome::FeedbackUploaderChrome(
    content::BrowserContext* context,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : FeedbackUploader(context, task_runner) {
  DCHECK(!context->IsOffTheRecord());

  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&FeedbackReport::LoadReportsAndQueue,
                     feedback_reports_path(),
                     base::BindRepeating(&QueueSingleReport, AsWeakPtr())));
}

FeedbackUploaderChrome::~FeedbackUploaderChrome() = default;

void FeedbackUploaderChrome::AccessTokenAvailable(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
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
  if (delegate_)
    delegate_->OnStartDispatchingReport();

  access_token_.clear();

  // TODO(crbug.com/849591): Instead of getting the IdentityManager from the
  // profile, we should pass the IdentityManager to FeedbackUploaderChrome's
  // ctor.
  Profile* profile = Profile::FromBrowserContext(context());
  DCHECK(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  if (identity_manager && identity_manager->HasPrimaryAccount()) {
    identity::ScopeSet scopes;
    scopes.insert("https://www.googleapis.com/auth/supportcontent");
    token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
        "feedback_uploader_chrome", identity_manager, scopes,
        base::BindOnce(&FeedbackUploaderChrome::AccessTokenAvailable,
                       base::Unretained(this)),
        signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate);
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

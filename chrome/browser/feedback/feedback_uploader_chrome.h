// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_FEEDBACK_UPLOADER_CHROME_H_
#define CHROME_BROWSER_FEEDBACK_FEEDBACK_UPLOADER_CHROME_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "components/feedback/feedback_uploader.h"
#include "components/signin/public/identity_manager/access_token_info.h"

#if BUILDFLAG(PLATFORM_CFM)
#include "components/invalidation/public/identity_provider.h"
#endif

namespace content {
class BrowserContext;
}  // namespace content

namespace signin {
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

class GoogleServiceAuthError;

namespace feedback {

class FeedbackUploaderChrome final : public FeedbackUploader {
 public:
  explicit FeedbackUploaderChrome(content::BrowserContext* context);

  FeedbackUploaderChrome(const FeedbackUploaderChrome&) = delete;
  FeedbackUploaderChrome& operator=(const FeedbackUploaderChrome&) = delete;

  ~FeedbackUploaderChrome() override;

  base::WeakPtr<FeedbackUploader> AsWeakPtr() override;

  class Delegate {
   public:
    // Notifies the delegate when we have started dispatching a feedback report.
    virtual void OnStartDispatchingReport() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  void set_feedback_uploader_delegate(Delegate* delegate) {
    delegate_ = delegate;
  }

 private:
  // feedback::FeedbackUploader:
  void StartDispatchingReport() override;
  void AppendExtraHeadersToUploadRequest(
      network::ResourceRequest* resource_request) override;

  void PrimaryAccountAccessTokenAvailable(
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info);

  void AccessTokenAvailable(GoogleServiceAuthError error, std::string token);

#if BUILDFLAG(PLATFORM_CFM)
  void ActiveAccountAccessTokenAvailable(GoogleServiceAuthError error,
                                         std::string token);

  std::unique_ptr<invalidation::ActiveAccountAccessTokenFetcher>
      active_account_token_fetcher_;
#endif  // BUILDFLAG(PLATFORM_CFM)

  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      primary_account_token_fetcher_;

  std::string access_token_;

  raw_ptr<Delegate> delegate_ = nullptr;  // Not owned.

  raw_ptr<content::BrowserContext> context_ = nullptr;

  base::WeakPtrFactory<FeedbackUploaderChrome> weak_ptr_factory_{this};
};

}  // namespace feedback

#endif  // CHROME_BROWSER_FEEDBACK_FEEDBACK_UPLOADER_CHROME_H_

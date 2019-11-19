// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_FEEDBACK_UPLOADER_CHROME_H_
#define CHROME_BROWSER_FEEDBACK_FEEDBACK_UPLOADER_CHROME_H_

#include <string>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "components/feedback/feedback_uploader.h"
#include "components/signin/public/identity_manager/access_token_info.h"

namespace signin {
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

class GoogleServiceAuthError;

namespace feedback {

class FeedbackUploaderChrome : public FeedbackUploader {
 public:
  FeedbackUploaderChrome(
      content::BrowserContext* context,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~FeedbackUploaderChrome() override;

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

  void AccessTokenAvailable(GoogleServiceAuthError error,
                            signin::AccessTokenInfo access_token_info);

  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;

  std::string access_token_;

  Delegate* delegate_ = nullptr;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(FeedbackUploaderChrome);
};

}  // namespace feedback

#endif  // CHROME_BROWSER_FEEDBACK_FEEDBACK_UPLOADER_CHROME_H_

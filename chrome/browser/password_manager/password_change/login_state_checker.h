// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_LOGIN_STATE_CHECKER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_LOGIN_STATE_CHECKER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"

namespace content {
class WebContents;
}

class OptimizationGuideKeyedService;

// Helper class which checks if the user is fully signed in on the main tab
// before starting a password change flow in a background tab.
// The "main tab" refers to the tab the user is viewing when the flow is
// offered.
class LoginStateChecker {
 public:
  using LoginStateResultCallback = base::OnceCallback<void(bool)>;

  LoginStateChecker(content::WebContents* web_contents,
                    LoginStateResultCallback callback);

  LoginStateChecker(
      content::WebContents* web_contents,
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>
          capture_annotated_page_content,
      LoginStateResultCallback callback);

  ~LoginStateChecker();

#if defined(UNIT_TEST)
  void RespondWithLoginStatus(bool is_logged_in) {
    std::move(callback_).Run(is_logged_in);
  }
#endif

 private:
  OptimizationGuideKeyedService* GetOptimizationService();

  // Checks if the user is fully signed in on the site.
  // The result will be passed to the callback.
  void CheckLoginState();

  void OnPageContentReceived(
      std::optional<optimization_guide::AIPageContentResult> content);

  void OnExecutionResponseCallback(
      optimization_guide::OptimizationGuideModelExecutionResult
          execution_result,
      std::unique_ptr<
          optimization_guide::proto::PasswordChangeSubmissionLoggingData>
          logging_data);

  const raw_ptr<content::WebContents> web_contents_;
  // TODO(crbug.com/436537301): Use AnnotatedPageContentCapturer instead.
  base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>
      capture_annotated_page_content_;

  LoginStateResultCallback callback_;
  base::WeakPtrFactory<LoginStateChecker> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_LOGIN_STATE_CHECKER_H_

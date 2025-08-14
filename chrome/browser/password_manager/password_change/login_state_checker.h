// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_LOGIN_STATE_CHECKER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_LOGIN_STATE_CHECKER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/browser/web_contents_observer.h"

class AnnotatedPageContentCapturer;
class OptimizationGuideKeyedService;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace password_manager {
class PasswordManagerClient;
}  // namespace password_manager

// Helper class which checks if the user is fully signed in on the main tab
// before starting a password change flow in a background tab.
// If the initial check fails, it waits for a navigation to occur before
// retrying.
class LoginStateChecker : public content::WebContentsObserver {
 public:
  // Maximum amount of login state checks.
  static constexpr int kMaxLoginChecks = 5;
  using LoginStateResultCallback = base::OnceCallback<void(bool)>;

  LoginStateChecker(content::WebContents* web_contents,
                    password_manager::PasswordManagerClient* client,
                    LoginStateResultCallback callback);

  ~LoginStateChecker() override;

#if defined(UNIT_TEST)
  AnnotatedPageContentCapturer* capturer() { return capturer_.get(); }
  void RespondWithLoginStatus(bool is_logged_in) {
    std::move(callback_).Run(is_logged_in);
  }
#endif

 private:
  OptimizationGuideKeyedService* GetOptimizationService();

  // Checks if the user is fully signed in on the site.
  // The result will be passed to the callback on success, otherwise it will
  // set up a retry on the next navigation.
  void CheckLoginState();

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void OnPageContentReceived(
      std::optional<optimization_guide::AIPageContentResult> content);

  void OnExecutionResponseCallback(
      optimization_guide::OptimizationGuideModelExecutionResult
          execution_result,
      std::unique_ptr<
          optimization_guide::proto::PasswordChangeSubmissionLoggingData>
          logging_data);

  std::unique_ptr<AnnotatedPageContentCapturer> capturer_;

  raw_ptr<password_manager::PasswordManagerClient> client_ = nullptr;

  LoginStateResultCallback callback_;

  // The number of login state checks performed.
  int state_checks_count_ = 0;

  base::WeakPtrFactory<LoginStateChecker> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_LOGIN_STATE_CHECKER_H_

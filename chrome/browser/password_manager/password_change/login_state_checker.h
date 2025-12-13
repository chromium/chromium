// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_LOGIN_STATE_CHECKER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_LOGIN_STATE_CHECKER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/browser/web_contents_observer.h"

class AnnotatedPageContentCapturer;
class ModelQualityLogsUploader;
class OptimizationGuideKeyedService;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace password_manager {
class PasswordManagerClient;
}  // namespace password_manager

enum class LoginCheckResult {
  kLoggedIn = 0,
  kLoggedOut = 1,
  kError = 2,
};

// Helper class which checks if the user is fully signed in on the main tab
// before starting a password change flow in a background tab.
// If the initial check fails, it waits for a navigation to occur before
// retrying.
class LoginStateChecker : public content::WebContentsObserver {
 public:
  // Maximum amount of login state checks.
  static constexpr int kMaxLoginChecks = 5;
  using LoginStateResultCallback =
      base::RepeatingCallback<void(LoginCheckResult)>;

  LoginStateChecker(content::WebContents* web_contents,
                    ModelQualityLogsUploader* logs_uploader,
                    password_manager::PasswordManagerClient* client,
                    LoginStateResultCallback callback);

  ~LoginStateChecker() override;

  bool ReachedAttemptsLimit() const;

  void RetryLoginCheck();

#if defined(UNIT_TEST)
  AnnotatedPageContentCapturer* capturer() { return capturer_.get(); }
  void RespondWithLoginStatus(LoginCheckResult result) {
    result_check_callback_.Run(result);
  }
#endif

 private:
  // To be called when the login checks should be terminated due
  // to max retries or an unexpected state.
  void TerminateLoginChecks();

  // Sets the quality log state based on the last check performed.
  void SetLoginCheckQuality(
      std::unique_ptr<
          optimization_guide::proto::PasswordChangeSubmissionLoggingData>
          logging_data);

  OptimizationGuideKeyedService* GetOptimizationService();

  // Checks if the user is fully signed in on the site.
  // The result will be passed to the callback on success, otherwise it will
  // set up a retry on the next navigation.
  void CheckLoginState(bool ignore_attempts_limit);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void OnPageContentReceived(
      optimization_guide::AIPageContentResultOrError content);

  void OnExecutionResponseCallback(
      optimization_guide::OptimizationGuideModelExecutionResult
          execution_result,
      std::unique_ptr<
          optimization_guide::proto::PasswordChangeSubmissionLoggingData>
          logging_data);

  std::unique_ptr<AnnotatedPageContentCapturer> capturer_;

  // Whether a server request is ongoing.
  const base::Time creation_time_;
  bool is_request_in_flight_ = false;
  std::optional<optimization_guide::AIPageContentResult> cached_page_content_;
  const raw_ref<ModelQualityLogsUploader> logs_uploader_;

  raw_ptr<password_manager::PasswordManagerClient> client_ = nullptr;

  LoginStateResultCallback result_check_callback_;

  // The number of login state checks performed.
  int state_checks_count_ = 0;

  base::WeakPtrFactory<LoginStateChecker> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_LOGIN_STATE_CHECKER_H_

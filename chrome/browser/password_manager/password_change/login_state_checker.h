// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_LOGIN_STATE_CHECKER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_LOGIN_STATE_CHECKER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "content/public/browser/web_contents_observer.h"

class AnnotatedPageContentCapturer;
class OptimizationGuideKeyedService;

namespace optimization_guide::proto {
class PasswordChangeSubmissionLoggingData;
}  // namespace optimization_guide::proto

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace password_manager {
class PasswordManagerClient;
}  // namespace password_manager

struct LoginCheckResult {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(LoginCheckResult)
  enum class Status {
    kLoggedIn = 0,
    kLoggedOut = 1,
    kError = 2,
    kMaxValue = kError,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/password/enums.xml:LoginCheckResult)

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(LoginCheckError)
  enum class LoginCheckError {
    kLoginFailed = 0,
    kForgotPasswordPage = 1,
    kServerError = 2,
    kFailedToParseResponse = 3,
    kTimeout = 4,
    kUnknown = 5,
    kMaxValue = kUnknown,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/password/enums.xml:LoginCheckError)

  LoginCheckResult();
  LoginCheckResult(
      Status status,
      int state_checks_count,
      base::TimeDelta duration,
      std::unique_ptr<
          optimization_guide::proto::PasswordChangeSubmissionLoggingData>
          logging_data,
      std::optional<LoginCheckError> error = std::nullopt);
  ~LoginCheckResult();
  LoginCheckResult(LoginCheckResult&&);
  LoginCheckResult& operator=(LoginCheckResult&&);

  Status status = Status::kError;
  int state_checks_count = 0;
  base::TimeDelta duration;
  std::unique_ptr<
      optimization_guide::proto::PasswordChangeSubmissionLoggingData>
      logging_data;
  std::optional<LoginCheckError> error;
};

// Helper class which checks if the user is fully signed in on the main tab
// before starting a password change flow in a background tab.
// If the initial check fails, it waits for a navigation to occur before
// retrying.
class LoginStateChecker : public content::WebContentsObserver {
 public:
  // Maximum amount of login state checks.
  static constexpr int kMaxLoginChecks = 5;
  static constexpr base::TimeDelta kLoginCheckTimeout = base::Minutes(5);
  using LoginStateResultCallback =
      base::RepeatingCallback<void(LoginCheckResult)>;

  LoginStateChecker(content::WebContents* web_contents,
                    password_manager::PasswordManagerClient* client,
                    optimization_guide::ModelExecutionServiceType service_type,
                    LoginStateResultCallback callback);

  ~LoginStateChecker() override;

  bool ReachedAttemptsLimit() const;

  void RetryLoginCheck();

#if defined(UNIT_TEST)
  AnnotatedPageContentCapturer* capturer() { return capturer_.get(); }
  void RespondWithLoginStatus(
      LoginCheckResult::Status result,
      std::unique_ptr<
          optimization_guide::proto::PasswordChangeSubmissionLoggingData>
          logging_data = nullptr) {
    if (result == LoginCheckResult::Status::kLoggedIn) {
      timer_.Stop();
    }
    result_check_callback_.Run(LoginCheckResult(
        result, state_checks_count_, base::Time::Now() - creation_time_,
        std::move(logging_data)));
  }
#endif

 private:
  // Starts the timeout timer for login checks if Private Inference is enabled.
  void StartTimeoutTimer();

  // To be called when the login checks should be terminated due
  // to max retries or an unexpected state.
  void TerminateLoginChecks(
      LoginCheckResult::LoginCheckError error,
      std::unique_ptr<
          optimization_guide::proto::PasswordChangeSubmissionLoggingData>
          logging_data = nullptr);

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
  const optimization_guide::ModelExecutionServiceType service_type_;

  raw_ptr<password_manager::PasswordManagerClient> client_ = nullptr;

  LoginStateResultCallback result_check_callback_;

  // The number of login state checks performed.
  int state_checks_count_ = 0;

  base::OneShotTimer timer_;

  base::WeakPtrFactory<LoginStateChecker> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_LOGIN_STATE_CHECKER_H_

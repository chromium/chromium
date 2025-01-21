// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_IMPL_H_

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/password_manager/core/browser/password_form.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/accessibility/ax_tree_update.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace password_manager {
class PasswordFormManager;
class PasswordManagerDriver;
}  // namespace password_manager

namespace {
class ParsedPasswordFormWaiter;
}

// This class controls password change process including acceptance of privacy
// notice, opening of a new tab, navigation to the change password url, password
// generation and form submission.
class PasswordChangeDelegateImpl : public PasswordChangeDelegate,
                                   public content::WebContentsObserver {
 public:
  using OpenPasswordChangeTabCallback =
      base::RepeatingCallback<content::WebContents*(const GURL&,
                                                    content::WebContents*)>;

  static constexpr base::TimeDelta kChangePasswordFormWaitingTimeout =
      base::Seconds(10);

  PasswordChangeDelegateImpl(GURL change_password_url,
                             std::u16string username,
                             std::u16string password,
                             content::WebContents* originator,
                             OpenPasswordChangeTabCallback callback);
  ~PasswordChangeDelegateImpl() override;

  PasswordChangeDelegateImpl(const PasswordChangeDelegateImpl&) = delete;
  PasswordChangeDelegateImpl& operator=(const PasswordChangeDelegateImpl&) =
      delete;

  // Initiates password change flow.
  void Init();

  base::WeakPtr<PasswordChangeDelegate> AsWeakPtr() override;

 private:
  // PasswordChangeDelegate Impl
  bool IsPasswordChangeOngoing(content::WebContents* web_contents) override;
  State GetCurrentState() const override;
  void Stop() override;
#if !BUILDFLAG(IS_ANDROID)
  void OpenPasswordChangeTab() override;
#endif
  void SuccessfulSubmissionDetected();
  void OnPasswordFormSubmission(content::WebContents* web_contents) override;
  void ProcessTree(ui::AXTreeUpdate& ax_tree_update);
  void OnPrivacyNoticeAccepted() override;
  void OnExecutionResponseCallback(
      optimization_guide::OptimizationGuideModelExecutionResult
          execution_result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::u16string GetDisplayOrigin() const override;
  const std::u16string& GetUsername() const override;
  const std::u16string& GetGeneratedPassword() const override;

  // content::WebContentsObserver Impl
  void WebContentsDestroyed() override;

  // Opens the tab for password change and start looking for change password
  // form.
  void StartPasswordChange();

  // Updates `current_state_` and notifies `observers_`.
  void UpdateState(State new_state);

  void OnPasswordChangeFormParsed(
      password_manager::PasswordFormManager* form_manager);

  void FillChangePasswordForm(
      password_manager::PasswordForm form,
      base::WeakPtr<password_manager::PasswordManagerDriver> driver);
  void ChangePasswordFormFilled(const autofill::FormData& submitted_form);

  bool IsPrivacyNoticeAcknowledged() const;

  const GURL change_password_url_;
  const std::u16string username_;
  const std::u16string original_password_;
  bool submission_detected_ = false;

  std::u16string generated_password_;

  base::WeakPtr<content::WebContents> originator_;
  OpenPasswordChangeTabCallback open_password_change_tab_callback_;
  base::WeakPtr<content::WebContents> executor_;

  State current_state_ = State::kWaitingForChangePasswordForm;

  // Class which awaits for change password form to appear.
  std::unique_ptr<ParsedPasswordFormWaiter> form_waiter_;

  // Form manager for displayed change password form.
  std::unique_ptr<password_manager::PasswordFormManager> form_manager_;

  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  base::WeakPtrFactory<PasswordChangeDelegateImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_IMPL_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_WAITER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_WAITER_H_

#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "content/public/browser/web_contents_observer.h"

namespace password_manager {
class PasswordFormManager;
class PasswordManagerClient;
}  // namespace password_manager

namespace content {
class WebContents;
}

namespace password_change {

enum class FormDiscardReason {
  kUnknown = 0,
  kNoNewPasswordField = 1,
  kNewPasswordFieldDisabled = 2,
  kUsernameFieldEmptyAndFocusable = 3,
  kFieldToIgnore = 4,
  kNoDriver = 5,
  kFormNotVisible = 6,
  kNotInPrimaryMainFrame = 7,
};

struct DiscardedForm {
  password_manager::PasswordForm form;
  FormDiscardReason reason = FormDiscardReason::kUnknown;

#if defined(UNIT_TEST)
  friend bool operator==(const DiscardedForm&, const DiscardedForm&) = default;
#endif
};

}  // namespace password_change

// Returns whether the field with `renderer_id` in `form_data` is focusable.
bool FieldFocusable(autofill::FieldRendererId renderer_id,
                    const autofill::FormData& form_data);

// Returns whether the field with `renderer_id` in `form_data` is enabled and
// not readonly.
bool FieldEnabled(autofill::FieldRendererId renderer_id,
                  const autofill::FormData& form_data);

// Helper object which waits for change password parsing, invokes callback on
// completion. If `EnableTimeout()` is set on `Builder` and a form isn't found
// within `kChangePasswordFormWaitingTimeout` after WebContents finished loading
// callback is invoked with nullptr `form_manager`.
class ChangePasswordFormWaiter
    : public password_manager::PasswordFormManagerObserver,
      public content::WebContentsObserver {
 public:
  // Timeout for change password form await time after the page is loaded.
  static constexpr base::TimeDelta kChangePasswordFormWaitingTimeout =
      base::Seconds(3);

  // Timeout for local ML model availability before falling back to Init().
  static constexpr base::TimeDelta kLocalMLModelDownloadTimeout =
      base::Seconds(10);

  struct Result {
    explicit Result(
        password_manager::PasswordFormManager* form_manager = nullptr,
        std::vector<password_change::DiscardedForm> discarded_forms = {});
    ~Result();
    Result(const Result&);
    Result& operator=(const Result&);
    Result(Result&&);
    Result& operator=(Result&&);

    raw_ptr<password_manager::PasswordFormManager> form_manager = nullptr;
    std::vector<password_change::DiscardedForm> discarded_forms;

#if defined(UNIT_TEST)
    friend bool operator==(const Result&, const Result&) = default;
#endif
  };

  using PasswordFormFoundCallback = base::OnceCallback<void(Result)>;

  class Builder final {
   public:
    Builder(content::WebContents* web_contents,
            password_manager::PasswordManagerClient* client,
            PasswordFormFoundCallback callback);
    ~Builder();

    Builder& EnableTimeout();
    Builder& SetFieldsToIgnore(
        const std::vector<autofill::FieldGlobalId>& fields_to_ignore);
    Builder& IgnoreHiddenForms();

    std::unique_ptr<ChangePasswordFormWaiter> Build();

   private:
    std::unique_ptr<ChangePasswordFormWaiter> form_waiter_;
  };

  ~ChangePasswordFormWaiter() override;

  const std::vector<password_change::DiscardedForm>& GetDiscardedForms() const {
    return discarded_forms_;
  }

 private:
  friend class Builder;

  ChangePasswordFormWaiter(content::WebContents* web_contents,
                           password_manager::PasswordManagerClient* client,
                           PasswordFormFoundCallback callback);

  void Init();

  // Rechecks all forms in the cache periodically with exponential backoff.
  void RecheckForms();

  // Delays invoking Init() until the model is fully downloaded. Model has a
  // superior performance in classifying change password forms compared to
  // existing password manager capabilities.
  void WaitForLocalMLModelAvailability();

  // password_manager::PasswordFormManagerObserver Impl
  void OnPasswordFormParsed(
      password_manager::PasswordFormManager* form_manager) override;

  //  content::WebContentsObserver
  void DidStartLoading() override;
  void DidStopLoading() override;

  void OnTimeout();
  void OnLocalMLModelDownloadTimeout();
  void NotifyResult(password_manager::PasswordFormManager* form_manager);

  static password_manager::PasswordFormManager* GetCorrespondingFormManager(
      base::WeakPtr<ChangePasswordFormWaiter> waiter,
      autofill::FieldGlobalId field_global_id);

  void OnCheckViewAreaVisibleCallback(autofill::FieldGlobalId field_global_id,
                                      bool is_visible);

  void RecordDiscardedForm(
      const password_manager::PasswordFormManager* form_manager,
      password_change::FormDiscardReason discard_reason);

  const raw_ptr<password_manager::PasswordManagerClient> client_ = nullptr;
  PasswordFormFoundCallback callback_;

  base::TimeDelta forms_recheck_delay_ = base::Seconds(1);
  bool should_timeout_ = false;
  base::OneShotTimer timeout_timer_;
  // If true, this will skip forms with new password field that is not focusable
  // (hidden).
  bool ignore_hidden_forms_ = false;

  // FieldGlobalIds which ChangePasswordFormWaiter should ignore. This helps
  // avoid detecting the same change password form over and over again.
  std::vector<autofill::FieldGlobalId> fields_to_ignore_;

  // Subscription for model updates. Should be called when model has been
  // downloaded and available for use.
  base::CallbackListSubscription model_loaded_subscription_;

  std::vector<password_change::DiscardedForm> discarded_forms_;

  base::WeakPtrFactory<ChangePasswordFormWaiter> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_WAITER_H_

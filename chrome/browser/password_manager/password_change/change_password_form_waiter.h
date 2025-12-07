// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_WAITER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_WAITER_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
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

// Helper object which waits for change password parsing, invokes callback on
// completion. If form isn't found withing
// `kChangePasswordFormWaitingTimeout` after WebContents finished loading
// callback is invoked with nullptr.
class ChangePasswordFormWaiter
    : public password_manager::PasswordFormManagerObserver,
      public content::WebContentsObserver {
 public:
  // Timeout for change password form await time after the page is loaded.
  static constexpr base::TimeDelta kChangePasswordFormWaitingTimeout =
      base::Seconds(3);

  using PasswordFormFoundCallback =
      base::OnceCallback<void(password_manager::PasswordFormManager*)>;

  class Builder final {
   public:
    Builder(content::WebContents* web_contents,
            password_manager::PasswordManagerClient* client,
            PasswordFormFoundCallback callback);
    ~Builder();

    Builder& SetTimeoutCallback(base::OnceClosure timeout_callback);
    Builder& SetFieldsToIgnore(
        const std::vector<autofill::FieldRendererId>& fields_to_ignore);
    Builder& IgnoreHiddenForms();

    std::unique_ptr<ChangePasswordFormWaiter> Build();

   private:
    std::unique_ptr<ChangePasswordFormWaiter> form_waiter_;
  };

  ~ChangePasswordFormWaiter() override;

 private:
  friend class Builder;

  ChangePasswordFormWaiter(content::WebContents* web_contents,
                           password_manager::PasswordManagerClient* client,
                           PasswordFormFoundCallback callback);

  void Init();

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

  static password_manager::PasswordFormManager* GetCorrespondingFormManager(
      base::WeakPtr<ChangePasswordFormWaiter> waiter,
      autofill::FieldRendererId new_password_element_id);

  void OnCheckViewAreaVisibleCallback(
      autofill::FieldRendererId new_password_element_id,
      bool is_visible);

  const raw_ptr<password_manager::PasswordManagerClient> client_ = nullptr;
  PasswordFormFoundCallback callback_;

  base::TimeDelta timeout_ = base::TimeDelta::Max();
  base::OneShotTimer timeout_timer_;
  base::OnceClosure timeout_callback_;
  // If true, this will skip forms with new password field that is not focusable
  // (hidden).
  bool ignore_hidden_forms_ = false;

  // new_password_element_renderer_ids which ChangePasswordFormWaiter should
  // ignore. This helps avoid detecting the same change password form over and
  // over again.
  std::vector<autofill::FieldRendererId> fields_to_ignore_;

  // Subscription for model updates. Should be called when model has been
  // downloaded and available for use.
  base::CallbackListSubscription model_loaded_subscription_;

  base::WeakPtrFactory<ChangePasswordFormWaiter> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_WAITER_H_

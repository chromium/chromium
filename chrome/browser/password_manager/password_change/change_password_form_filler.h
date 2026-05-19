// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_FILLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_FILLER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/password_manager/password_change/change_password_form_filling_submission_helper.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"

namespace content {
class WebContents;
}

namespace password_manager {
class PasswordFormManager;
class PasswordManagerClient;
class PasswordManagerDriver;
struct PasswordForm;
}  // namespace password_manager

class ModelQualityLogsUploader;
class ChangePasswordFormWaiter;

// Helper class which handles the change password form filling process.
// It fills the form, handles the filling callback, and retries if form filling
// failed (using ChangePasswordFormWaiter).
class ChangePasswordFormFiller {
 public:
  using SubmissionError =
      ChangePasswordFormFillingSubmissionHelper::SubmissionError;
  using FillingResult =
      base::expected<std::unique_ptr<password_manager::PasswordFormManager>,
                     SubmissionError>;

  ChangePasswordFormFiller(content::WebContents* web_contents,
                           password_manager::PasswordManagerClient* client,
                           ModelQualityLogsUploader* logs_uploader);
  ~ChangePasswordFormFiller();

  ChangePasswordFormFiller(const ChangePasswordFormFiller&) = delete;
  ChangePasswordFormFiller& operator=(const ChangePasswordFormFiller&) = delete;

  // Fills the change password form.
  void FillForm(password_manager::PasswordFormManager* form_manager,
                const std::u16string& username,
                const std::u16string& login_password,
                const std::u16string& generated_password,
                base::OnceCallback<void(FillingResult)> callback);

#if defined(UNIT_TEST)
  ChangePasswordFormWaiter* form_waiter() { return form_waiter_.get(); }
#endif

 private:
  void TriggerFilling(
      const password_manager::PasswordForm& form,
      base::WeakPtr<password_manager::PasswordManagerDriver> driver);

  void ChangePasswordFormFilled(
      const std::optional<autofill::FormData>& submitted_form);

  void OnFormFillingFailed();

  void OnChangePasswordFormFound(
      password_manager::PasswordFormManager* form_manager);

  const raw_ptr<content::WebContents> web_contents_;
  const raw_ptr<password_manager::PasswordManagerClient> client_;
  raw_ptr<ModelQualityLogsUploader> logs_uploader_;

  std::unique_ptr<password_manager::PasswordFormManager> form_manager_;

  std::u16string username_;
  std::u16string login_password_;
  std::u16string stored_password_;
  std::u16string generated_password_;

  // FieldGlobalIds for the forms which `this` tried to fill.
  // Used to avoid attempting to fill the same form over and over again.
  std::vector<autofill::FieldGlobalId> observed_fields_;

  // Helper object which finds a new PasswordFormManager when filling of an
  // old form failed.
  std::unique_ptr<ChangePasswordFormWaiter> form_waiter_;

  base::OnceCallback<void(FillingResult)> callback_;

  base::WeakPtrFactory<ChangePasswordFormFiller> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_FILLER_H_

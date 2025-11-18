// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_FORM_FILLING_HELPER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_FORM_FILLING_HELPER_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"

namespace content {
class WebContents;
}

namespace autofill {
class AutofillDriver;
}  // namespace autofill

namespace password_manager {
class PasswordManagerDriver;
}

class TypingHelper;

// Helper object which is responsible for filling all the fields in the order of
// fields appearance in the DOM. Upon completion invokes the callback with
// autofill::FormData. In case of an error or failure invokes a callback with
// std::nullopt.
class FormFillingHelper {
 public:
  using FillingTasks =
      std::map<autofill::FieldGlobalId, std::u16string, std::greater<>>;
  using ResultCallback =
      base::OnceCallback<void(const std::optional<autofill::FormData>&)>;

  FormFillingHelper(
      content::WebContents* web_contents,
      base::WeakPtr<password_manager::PasswordManagerDriver> driver,
      FillingTasks filling_tasks,
      ResultCallback callback);
  ~FormFillingHelper();

#if defined(UNIT_TEST)
  TypingHelper* typing_helper() { return typing_helper_.get(); }
  void SimulateFillingResult(std::optional<autofill::FormData> result) {
    std::move(callback_).Run(std::move(result));
  }
#endif

 private:
  void FillField(
      autofill::FieldGlobalId field_id,
      std::u16string value,
      base::OnceCallback<void(actor::mojom::ActionResultCode)> callback);

  void OnTypingResult(actor::mojom::ActionResultCode result);

  void ExtractForm(autofill::FieldGlobalId global_id);

  void OnFormExtracted(autofill::AutofillDriver* host_frame_driver,
                       const std::optional<autofill::FormData>& form);

  void TerminateFormFilling();

  const raw_ptr<content::WebContents> web_contents_ = nullptr;

  base::WeakPtr<password_manager::PasswordManagerDriver> driver_;

  ResultCallback callback_;

  std::unique_ptr<TypingHelper> typing_helper_;

  base::WeakPtrFactory<FormFillingHelper> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_FORM_FILLING_HELPER_H_

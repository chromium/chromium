// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/form_filling_helper.h"

#include "chrome/browser/password_manager/password_change/typing_helper.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_interface.h"

namespace {

using Logger = password_manager::BrowserSavePasswordProgressLogger;

std::unique_ptr<Logger> GetLoggerIfAvailable(
    base::WeakPtr<password_manager::PasswordManagerDriver> driver) {
  if (!driver || !driver->GetPasswordManager()) {
    return nullptr;
  }

  auto* client = driver->GetPasswordManager()->GetClient();
  CHECK(client);

  autofill::LogManager* log_manager = client->GetCurrentLogManager();
  if (log_manager && log_manager->IsLoggingActive()) {
    return std::make_unique<Logger>(log_manager);
  }

  return nullptr;
}

}  // namespace

FormFillingHelper::FormFillingHelper(
    content::WebContents* web_contents,
    base::WeakPtr<password_manager::PasswordManagerDriver> driver,
    FillingTasks filling_tasks,
    ResultCallback callback)
    : web_contents_(web_contents),
      driver_(driver),
      callback_(std::move(callback)) {
  CHECK(!filling_tasks.empty());

  auto callback_chain = base::BindOnce(&FormFillingHelper::ExtractForm,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       filling_tasks.begin()->first);

  for (const auto& [field_id, value] : filling_tasks) {
    auto on_filling_complete =
        base::BindOnce(&FormFillingHelper::OnTypingResult,
                       weak_ptr_factory_.GetWeakPtr())
            .Then(std::move(callback_chain));
    callback_chain = base::BindOnce(
        &FormFillingHelper::FillField, weak_ptr_factory_.GetWeakPtr(), field_id,
        std::move(value), std::move(on_filling_complete));
  }

  // PostTask is required because if the form is filled immediately the fields
  // might be cleared by PasswordAutofillAgent if there were no credentials to
  // fill during SendFillInformationToRenderer call.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback_chain));
}

FormFillingHelper::~FormFillingHelper() = default;

void FormFillingHelper::FillField(
    autofill::FieldGlobalId field_id,
    std::u16string value,
    base::OnceCallback<void(actor::mojom::ActionResultCode)> callback) {
  CHECK(!typing_helper_);
  typing_helper_ = std::make_unique<TypingHelper>(web_contents_.get(),
                                                  field_id.renderer_id.value(),
                                                  value, std::move(callback));
}

void FormFillingHelper::OnTypingResult(actor::mojom::ActionResultCode result) {
  typing_helper_.reset();

  if (auto logger = GetLoggerIfAvailable(driver_)) {
    logger->LogString(
        Logger::STRING_AUTOMATED_PASSWORD_CHANGE_FILLING_ACTION_RESULT,
        base::ToString(result));
  }

  if (result != actor::mojom::ActionResultCode::kOk) {
    TerminateFormFilling();
    return;
  }
}

void FormFillingHelper::ExtractForm(autofill::FieldGlobalId global_id) {
  if (!driver_) {
    std::move(callback_).Run(std::nullopt);
    return;
  }
  autofill::AutofillDriver* autofill_driver = driver_->GetAutofillDriver();
  if (!autofill_driver) {
    std::move(callback_).Run(std::nullopt);
    return;
  }
  autofill_driver->ExtractFormWithField(
      global_id, base::BindOnce(&FormFillingHelper::OnFormExtracted,
                                weak_ptr_factory_.GetWeakPtr()));
}

void FormFillingHelper::OnFormExtracted(
    autofill::AutofillDriver* host_frame_driver,
    const std::optional<autofill::FormData>& form) {
  std::move(callback_).Run(form);
}

void FormFillingHelper::TerminateFormFilling() {
  // InvalidateWeakPtrs to stop the callback chain.
  weak_ptr_factory_.InvalidateWeakPtrs();

  std::move(callback_).Run(std::nullopt);
}

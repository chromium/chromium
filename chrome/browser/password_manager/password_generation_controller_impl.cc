// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_generation_controller_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_accessory_controller.h"
#include "chrome/browser/password_manager/password_generation_dialog_view_interface.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/signatures_util.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

using autofill::mojom::FocusedFieldType;
using autofill::password_generation::PasswordGenerationType;
using password_manager::metrics_util::GenerationDialogChoice;

PasswordGenerationControllerImpl::~PasswordGenerationControllerImpl() = default;

// static
bool PasswordGenerationController::AllowedForWebContents(
    content::WebContents* web_contents) {
  return PasswordAccessoryController::AllowedForWebContents(web_contents);
}

// static
PasswordGenerationController* PasswordGenerationController::GetOrCreate(
    content::WebContents* web_contents) {
  DCHECK(PasswordGenerationController::AllowedForWebContents(web_contents));

  PasswordGenerationControllerImpl::CreateForWebContents(web_contents);
  return PasswordGenerationControllerImpl::FromWebContents(web_contents);
}

// static
PasswordGenerationController* PasswordGenerationController::GetIfExisting(
    content::WebContents* web_contents) {
  return PasswordGenerationControllerImpl::FromWebContents(web_contents);
}

struct PasswordGenerationControllerImpl::GenerationElementData {
  GenerationElementData(
      const autofill::password_generation::PasswordGenerationUIData& ui_data) {
    const std::string kFieldType = "password";

    form = ui_data.password_form;
    form_signature =
        autofill::CalculateFormSignature(ui_data.password_form.form_data);
    field_signature = autofill::CalculateFieldSignatureByNameAndType(
        ui_data.generation_element, kFieldType);
    generation_element_id = ui_data.generation_element_id;
    max_password_length = ui_data.max_length;
  }

  // Form for which password generation is triggered.
  autofill::PasswordForm form;

  // Signature of the form for which password generation is triggered.
  autofill::FormSignature form_signature;

  // Signature of the field for which password generation is triggered.
  autofill::FieldSignature field_signature;

  // Renderer ID of the password field triggering generation.
  uint32_t generation_element_id;

  // Maximum length of the generated password.
  uint32_t max_password_length;
};

base::WeakPtr<password_manager::PasswordManagerDriver>
PasswordGenerationControllerImpl::GetActiveFrameDriver() const {
  return active_frame_driver_;
}

void PasswordGenerationControllerImpl::OnAutomaticGenerationAvailable(
    const password_manager::PasswordManagerDriver* target_frame_driver,
    const autofill::password_generation::PasswordGenerationUIData& ui_data,
    gfx::RectF element_bounds_in_screen_space) {
  if (!IsActiveFrameDriver(target_frame_driver))
    return;
  DCHECK(!dialog_view_);

  active_frame_driver_->GetPasswordManager()
      ->SetGenerationElementAndReasonForForm(
          active_frame_driver_.get(), ui_data.password_form,
          ui_data.generation_element, false /* is_manually_triggered */);

  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillKeyboardAccessory)) {
    active_frame_driver_->GetPasswordAutofillManager()
        ->MaybeShowPasswordSuggestions(element_bounds_in_screen_space,
                                       ui_data.text_direction);
  }

  generation_element_data_ = std::make_unique<GenerationElementData>(ui_data);

  if (!manual_filling_controller_) {
    manual_filling_controller_ =
        ManualFillingController::GetOrCreate(web_contents_);
  }

  DCHECK(manual_filling_controller_);
  manual_filling_controller_->OnAutomaticGenerationStatusChanged(true);
}

void PasswordGenerationControllerImpl::ShowManualGenerationDialog(
    const password_manager::PasswordManagerDriver* target_frame_driver,
    const autofill::password_generation::PasswordGenerationUIData& ui_data) {
  if (!IsActiveFrameDriver(target_frame_driver) ||
      !manual_generation_requested_)
    return;
  generation_element_data_ = std::make_unique<GenerationElementData>(ui_data);
  ShowDialog(PasswordGenerationType::kManual);
}

void PasswordGenerationControllerImpl::FocusedInputChanged(
    autofill::mojom::FocusedFieldType focused_field_type,
    base::WeakPtr<password_manager::PasswordManagerDriver> driver) {
  ResetState();
  if (focused_field_type == FocusedFieldType::kFillablePasswordField)
    active_frame_driver_ = std::move(driver);
}

void PasswordGenerationControllerImpl::OnGenerationRequested(
    PasswordGenerationType type) {
  if (type == PasswordGenerationType::kManual) {
    manual_generation_requested_ = true;
    client_->GeneratePassword();
  } else {
    ShowDialog(PasswordGenerationType::kAutomatic);
  }
}

void PasswordGenerationControllerImpl::GeneratedPasswordAccepted(
    const base::string16& password,
    base::WeakPtr<password_manager::PasswordManagerDriver> driver,
    PasswordGenerationType type) {
  if (!driver)
    return;
  password_manager::metrics_util::LogGenerationDialogChoice(
      GenerationDialogChoice::kAccepted, type);
  driver->GeneratedPasswordAccepted(
      generation_element_data_->form.form_data,
      generation_element_data_->generation_element_id, password);
  ResetState();
}

void PasswordGenerationControllerImpl::GeneratedPasswordRejected(
    PasswordGenerationType type) {
  ResetState();
  password_manager::metrics_util::LogGenerationDialogChoice(
      GenerationDialogChoice::kRejected, type);
}

gfx::NativeWindow PasswordGenerationControllerImpl::top_level_native_window()
    const {
  return web_contents_->GetTopLevelNativeWindow();
}

// static
void PasswordGenerationControllerImpl::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    base::WeakPtr<ManualFillingController> manual_filling_controller,
    CreateDialogFactory create_dialog_factory) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  DCHECK(manual_filling_controller);

  web_contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new PasswordGenerationControllerImpl(
          web_contents, client, std::move(manual_filling_controller),
          create_dialog_factory)));
}

PasswordGenerationControllerImpl::PasswordGenerationControllerImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      client_(ChromePasswordManagerClient::FromWebContents(web_contents)),
      create_dialog_factory_(
          base::BindRepeating(&PasswordGenerationDialogViewInterface::Create)) {
}

PasswordGenerationControllerImpl::PasswordGenerationControllerImpl(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    base::WeakPtr<ManualFillingController> manual_filling_controller,
    CreateDialogFactory create_dialog_factory)
    : web_contents_(web_contents),
      client_(client),
      manual_filling_controller_(std::move(manual_filling_controller)),
      create_dialog_factory_(create_dialog_factory) {}

void PasswordGenerationControllerImpl::ShowDialog(PasswordGenerationType type) {
  if (!active_frame_driver_ || dialog_view_) {
    return;
  }

  // TODO(crbug.com/894756): Add a test helper that sets this up correctly.
  if (!generation_element_data_) {
    /* This can currently happen in integration tests that are iniated from
    the java side. */
    return;
  }

  dialog_view_ = create_dialog_factory_.Run(this);

  uint32_t spec_priority = 0;
  base::string16 password =
      active_frame_driver_->GetPasswordGenerationHelper()->GeneratePassword(
          web_contents_->GetLastCommittedURL().GetOrigin(),
          generation_element_data_->form_signature,
          generation_element_data_->field_signature,
          generation_element_data_->max_password_length, &spec_priority);
  dialog_view_->Show(password, active_frame_driver_, type);
}

bool PasswordGenerationControllerImpl::IsActiveFrameDriver(
    const password_manager::PasswordManagerDriver* driver) const {
  if (!active_frame_driver_)
    return false;
  return active_frame_driver_.get() == driver;
}

void PasswordGenerationControllerImpl::ResetState() {
  if (manual_filling_controller_)
    manual_filling_controller_->OnAutomaticGenerationStatusChanged(false);
  active_frame_driver_.reset();
  generation_element_data_.reset();
  dialog_view_.reset();
  manual_generation_requested_ = false;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PasswordGenerationControllerImpl)

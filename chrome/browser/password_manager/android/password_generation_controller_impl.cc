// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_generation_controller_impl.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/password_manager/android/password_accessory_controller.h"
#include "chrome/browser/password_manager/android/password_generation_dialog_view_interface.h"
#include "chrome/browser/password_manager/android/password_infobar_utils.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/touch_to_fill/password_generation/android/touch_to_fill_password_generation_bridge_impl.h"
#include "chrome/browser/touch_to_fill/password_generation/android/touch_to_fill_password_generation_controller.h"
#include "components/autofill/core/browser/ui/accessory_sheet_enums.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_features.h"

using autofill::mojom::FocusedFieldType;
using autofill::password_generation::PasswordGenerationType;
using password_manager::metrics_util::GenerationDialogChoice;
using ShouldShowAction = ManualFillingController::ShouldShowAction;

PasswordGenerationControllerImpl::~PasswordGenerationControllerImpl() = default;

// static
PasswordGenerationController* PasswordGenerationController::GetOrCreate(
    content::WebContents* web_contents) {
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

    form_data = ui_data.form_data;
    form_signature = autofill::CalculateFormSignature(ui_data.form_data);
    field_signature = autofill::CalculateFieldSignatureByNameAndType(
        ui_data.generation_element, kFieldType);
    generation_element_id = ui_data.generation_element_id;
    max_password_length = ui_data.max_length;
  }

  // Form for which password generation is triggered.
  autofill::FormData form_data;

  // Signature of the form for which password generation is triggered.
  autofill::FormSignature form_signature;

  // Signature of the field for which password generation is triggered.
  autofill::FieldSignature field_signature;

  // Renderer ID of the password field triggering generation.
  autofill::FieldRendererId generation_element_id;

  // Maximum length of the generated password.
  uint32_t max_password_length;
};

base::WeakPtr<password_manager::ContentPasswordManagerDriver>
PasswordGenerationControllerImpl::GetActiveFrameDriver() const {
  return active_frame_driver_;
}

void PasswordGenerationControllerImpl::OnAutomaticGenerationAvailable(
    base::WeakPtr<password_manager::ContentPasswordManagerDriver>
        target_frame_driver,
    const autofill::password_generation::PasswordGenerationUIData& ui_data,
    gfx::RectF element_bounds_in_screen_space) {
  // We can't be sure that the active frame driver would be set in the
  // FocusedInputChanged by now, because there is a race condition. The roots
  // of the OnAutomaticGenerationAvailable and FocusedInputChanged calls are
  // the same in the renderer. So we need to set it here too.
  FocusedInputChanged(autofill::mojom::FocusedFieldType::kFillablePasswordField,
                      std::move(target_frame_driver));

  active_frame_driver_->GetPasswordManager()
      ->SetGenerationElementAndTypeForForm(
          active_frame_driver_.get(), ui_data.form_data.unique_renderer_id,
          ui_data.generation_element_id, PasswordGenerationType::kAutomatic);

  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillKeyboardAccessory)) {
    active_frame_driver_->GetPasswordAutofillManager()
        ->MaybeShowPasswordSuggestions(element_bounds_in_screen_space,
                                       ui_data.text_direction);
  }

  generation_element_data_ = std::make_unique<GenerationElementData>(ui_data);

  if (touch_to_fill_generation_state_ == TouchToFillState::kIsShowing) {
    return;
  }
  if (TryToShowGenerationTouchToFill()) {
    return;
  }

  if (!manual_filling_controller_) {
    manual_filling_controller_ =
        ManualFillingController::GetOrCreate(&GetWebContents());
  }

  DCHECK(manual_filling_controller_);
  manual_filling_controller_->OnAccessoryActionAvailabilityChanged(
      ShouldShowAction(true),
      autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC);
}

void PasswordGenerationControllerImpl::ShowManualGenerationDialog(
    const password_manager::ContentPasswordManagerDriver* target_frame_driver,
    const autofill::password_generation::PasswordGenerationUIData& ui_data) {
  if (!IsActiveFrameDriver(target_frame_driver) ||
      !manual_generation_requested_)
    return;
  generation_element_data_ = std::make_unique<GenerationElementData>(ui_data);
  ShowDialog(PasswordGenerationType::kManual);
}

void PasswordGenerationControllerImpl::FocusedInputChanged(
    autofill::mojom::FocusedFieldType focused_field_type,
    base::WeakPtr<password_manager::ContentPasswordManagerDriver> driver) {
  TRACE_EVENT0("passwords",
               "PasswordGenerationControllerImpl::FocusedInputChanged");
  // It's probably a duplicate notification.
  if (IsActiveFrameDriver(driver.get()) &&
      focused_field_type == FocusedFieldType::kFillablePasswordField) {
    return;
  }
  ResetFocusState();
  if (focused_field_type == FocusedFieldType::kFillablePasswordField)
    active_frame_driver_ = std::move(driver);
}

void PasswordGenerationControllerImpl::OnGenerationRequested(
    PasswordGenerationType type) {
  if (type == PasswordGenerationType::kManual) {
    manual_generation_requested_ = true;
    client_->GeneratePassword(type);
  } else {
    ShowDialog(PasswordGenerationType::kAutomatic);
  }
}

void PasswordGenerationControllerImpl::GeneratedPasswordAccepted(
    const std::u16string& password,
    base::WeakPtr<password_manager::ContentPasswordManagerDriver> driver,
    PasswordGenerationType type) {
  if (!driver)
    return;
  password_manager::metrics_util::LogGenerationDialogChoice(
      GenerationDialogChoice::kAccepted, type);
  driver->GeneratedPasswordAccepted(
      generation_element_data_->form_data,
      generation_element_data_->generation_element_id, password);
  ResetFocusState();
}

void PasswordGenerationControllerImpl::GeneratedPasswordRejected(
    PasswordGenerationType type) {
  ResetFocusState();
  password_manager::metrics_util::LogGenerationDialogChoice(
      GenerationDialogChoice::kRejected, type);
}

gfx::NativeWindow PasswordGenerationControllerImpl::top_level_native_window() {
  return GetWebContents().GetTopLevelNativeWindow();
}

content::WebContents* PasswordGenerationControllerImpl::web_contents() {
  return &GetWebContents();
}

autofill::FieldSignature
PasswordGenerationControllerImpl::get_field_signature_for_testing() {
  return generation_element_data_->field_signature;
}

autofill::FormSignature
PasswordGenerationControllerImpl::get_form_signature_for_testing() {
  return generation_element_data_->form_signature;
}

// static
void PasswordGenerationControllerImpl::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    base::WeakPtr<ManualFillingController> manual_filling_controller,
    CreateDialogFactory create_dialog_factory,
    CreateTouchToFillGenerationControllerFactory
        create_touch_to_fill_generation_controller) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  DCHECK(manual_filling_controller);

  web_contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new PasswordGenerationControllerImpl(
          web_contents, client, std::move(manual_filling_controller),
          create_dialog_factory, create_touch_to_fill_generation_controller)));
}

PasswordGenerationControllerImpl::PasswordGenerationControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsUserData<PasswordGenerationControllerImpl>(
          *web_contents),
      client_(ChromePasswordManagerClient::FromWebContents(web_contents)),
      create_dialog_factory_(
          base::BindRepeating(&PasswordGenerationDialogViewInterface::Create)),
      create_touch_to_fill_generation_controller_(
          base::BindRepeating(&PasswordGenerationControllerImpl::
                                  CreateTouchToFillGenerationController,
                              base::Unretained(this))) {}

PasswordGenerationControllerImpl::PasswordGenerationControllerImpl(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    base::WeakPtr<ManualFillingController> manual_filling_controller,
    CreateDialogFactory create_dialog_factory,
    CreateTouchToFillGenerationControllerFactory
        create_touch_to_fill_generation_controller)
    : content::WebContentsUserData<PasswordGenerationControllerImpl>(
          *web_contents),
      client_(client),
      manual_filling_controller_(std::move(manual_filling_controller)),
      create_dialog_factory_(create_dialog_factory),
      create_touch_to_fill_generation_controller_(
          create_touch_to_fill_generation_controller) {}

std::unique_ptr<TouchToFillPasswordGenerationController>
PasswordGenerationControllerImpl::CreateTouchToFillGenerationController() {
  return std::make_unique<TouchToFillPasswordGenerationController>(
      active_frame_driver_, &GetWebContents(),
      std::make_unique<TouchToFillPasswordGenerationBridgeImpl>(),
      base::BindOnce(&PasswordGenerationControllerImpl::
                         OnTouchToFillForGenerationDismissed,
                     base::Unretained(this)));
}

std::unique_ptr<TouchToFillPasswordGenerationController>
PasswordGenerationControllerImpl::
    CreateTouchToFillGenerationControllerForTesting(
        std::unique_ptr<TouchToFillPasswordGenerationBridge> bridge) {
  return std::make_unique<TouchToFillPasswordGenerationController>(
      active_frame_driver_, &GetWebContents(), std::move(bridge),
      base::BindOnce(&PasswordGenerationControllerImpl::
                         OnTouchToFillForGenerationDismissed,
                     base::Unretained(this)));
}

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

  std::u16string password =
      active_frame_driver_->GetPasswordGenerationHelper()->GeneratePassword(
          GetWebContents().GetLastCommittedURL().DeprecatedGetOriginAsURL(),
          generation_element_data_->form_signature,
          generation_element_data_->field_signature,
          generation_element_data_->max_password_length);
  dialog_view_->Show(password, active_frame_driver_, type);
}

bool PasswordGenerationControllerImpl::TryToShowGenerationTouchToFill() {
  CHECK(touch_to_fill_generation_state_ != TouchToFillState::kIsShowing);

  if (!base::FeatureList::IsEnabled(
          password_manager::features::kPasswordGenerationBottomSheet) ||
      touch_to_fill_generation_state_ == TouchToFillState::kWasShown) {
    return false;
  }

  touch_to_fill_generation_controller_ =
      create_touch_to_fill_generation_controller_.Run();
  std::u16string generated_password =
      active_frame_driver_->GetPasswordGenerationHelper()->GeneratePassword(
          GetWebContents().GetLastCommittedURL().DeprecatedGetOriginAsURL(),
          generation_element_data_->form_signature,
          generation_element_data_->field_signature,
          generation_element_data_->max_password_length);
  std::string account =
      password_manager::GetDisplayableAccountName(&GetWebContents());
  if (!touch_to_fill_generation_controller_->ShowTouchToFill(
          std::move(generated_password), std::move(account))) {
    return false;
  }

  touch_to_fill_generation_state_ = TouchToFillState::kIsShowing;
  return true;
}

void PasswordGenerationControllerImpl::OnTouchToFillForGenerationDismissed() {
  CHECK(touch_to_fill_generation_state_ == TouchToFillState::kIsShowing);
  touch_to_fill_generation_state_ = TouchToFillState::kWasShown;
  touch_to_fill_generation_controller_.reset();
}

bool PasswordGenerationControllerImpl::IsActiveFrameDriver(
    const password_manager::ContentPasswordManagerDriver* driver) const {
  if (!active_frame_driver_)
    return false;
  return active_frame_driver_.get() == driver;
}

void PasswordGenerationControllerImpl::ResetFocusState() {
  if (manual_filling_controller_)
    manual_filling_controller_->OnAccessoryActionAvailabilityChanged(
        ShouldShowAction(false),
        autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC);
  active_frame_driver_.reset();
  generation_element_data_.reset();
  dialog_view_.reset();
  manual_generation_requested_ = false;
  // TODO (crbug.com/1421753): Do we need to hide the bottom sheet here?
}

void PasswordGenerationControllerImpl::HideBottomSheetIfNeeded() {
  if (touch_to_fill_generation_state_ != TouchToFillState::kNone) {
    touch_to_fill_generation_state_ = TouchToFillState::kNone;
    touch_to_fill_generation_controller_.reset();
  }
}

void PasswordGenerationControllerImpl::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (active_frame_driver_ &&
      active_frame_driver_->render_frame_host() == render_frame_host) {
    HideBottomSheetIfNeeded();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PasswordGenerationControllerImpl);

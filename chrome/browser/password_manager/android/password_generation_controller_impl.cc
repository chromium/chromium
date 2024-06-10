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
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_enums.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller.h"
#include "chrome/browser/keyboard_accessory/android/password_accessory_controller.h"
#include "chrome/browser/password_manager/android/password_infobar_utils.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/touch_to_fill_password_generation_bridge_impl.h"
#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/touch_to_fill_password_generation_controller.h"
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
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

using autofill::mojom::FocusedFieldType;
using autofill::password_generation::PasswordGenerationType;
using password_manager::metrics_util::GenerationDialogChoice;
using password_manager::prefs::kPasswordGenerationBottomSheetDismissCount;
using ShouldShowAction = ManualFillingController::ShouldShowAction;
using TouchToFillOutcome =
    password_manager::metrics_util::TouchToFillPasswordGenerationTriggerOutcome;

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

base::WeakPtr<password_manager::ContentPasswordManagerDriver>
PasswordGenerationControllerImpl::GetActiveFrameDriver() const {
  return active_frame_driver_;
}

void PasswordGenerationControllerImpl::OnAutomaticGenerationAvailable(
    base::WeakPtr<password_manager::ContentPasswordManagerDriver>
        target_frame_driver,
    const autofill::password_generation::PasswordGenerationUIData& ui_data,
    bool has_saved_credentials,
    gfx::RectF element_bounds_in_screen_space) {
  // We can't be sure that the active frame driver would be set in the
  // FocusedInputChanged by now, because there is a race condition. The roots
  // of the OnAutomaticGenerationAvailable and FocusedInputChanged calls are
  // the same in the renderer. So we need to set it here too.
  FocusedInputChanged(autofill::mojom::FocusedFieldType::kFillablePasswordField,
                      std::move(target_frame_driver));

  active_frame_driver_->GetPasswordManager()
      ->SetGenerationElementAndTypeForForm(
          active_frame_driver_.get(), ui_data.form_data.renderer_id(),
          ui_data.generation_element_id, PasswordGenerationType::kAutomatic);

  generation_element_data_ =
      std::make_unique<PasswordGenerationElementData>(ui_data);

  if (touch_to_fill_generation_state_ == TouchToFillState::kIsShowing) {
    return;
  }
  if (TryToShowGenerationTouchToFill(has_saved_credentials)) {
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
      !manual_generation_requested_) {
    return;
  }
  generation_element_data_ =
      std::make_unique<PasswordGenerationElementData>(ui_data);
  ShowBottomSheet(PasswordGenerationType::kManual);
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
  if (focused_field_type == FocusedFieldType::kFillablePasswordField) {
    active_frame_driver_ = std::move(driver);
  }
}

void PasswordGenerationControllerImpl::OnGenerationRequested(
    PasswordGenerationType type) {
  if (type == PasswordGenerationType::kManual) {
    manual_generation_requested_ = true;
    client_->GeneratePassword(type);
  } else {
    ShowBottomSheet(PasswordGenerationType::kAutomatic);
  }
  ResetPasswordGenerationDismissBottomSheetCount();
}

void PasswordGenerationControllerImpl::GeneratedPasswordAccepted(
    const std::u16string& password,
    base::WeakPtr<password_manager::ContentPasswordManagerDriver> driver,
    PasswordGenerationType type) {
  if (!driver) {
    return;
  }
  password_manager::metrics_util::LogGenerationDialogChoice(
      GenerationDialogChoice::kAccepted, type);
  driver->GeneratedPasswordAccepted(
      generation_element_data_->form_data,
      generation_element_data_->generation_element_id, password);
  driver->FocusNextFieldAfterPasswords();
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
    CreateTouchToFillGenerationControllerFactory
        create_touch_to_fill_generation_controller) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  DCHECK(manual_filling_controller);

  web_contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new PasswordGenerationControllerImpl(
          web_contents, client, std::move(manual_filling_controller),
          create_touch_to_fill_generation_controller)));
}

PasswordGenerationControllerImpl::PasswordGenerationControllerImpl(
    content::WebContents* web_contents)
    : PasswordGenerationControllerImpl(
          web_contents,
          ChromePasswordManagerClient::FromWebContents(web_contents),
          /*manual_filling_controller=*/nullptr,
          base::BindRepeating(&PasswordGenerationControllerImpl::
                                  CreateTouchToFillGenerationController,
                              base::Unretained(this))) {}

PasswordGenerationControllerImpl::PasswordGenerationControllerImpl(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    base::WeakPtr<ManualFillingController> manual_filling_controller,
    CreateTouchToFillGenerationControllerFactory
        create_touch_to_fill_generation_controller)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PasswordGenerationControllerImpl>(
          *web_contents),
      client_(client),
      manual_filling_controller_(std::move(manual_filling_controller)),
      create_touch_to_fill_generation_controller_(
          create_touch_to_fill_generation_controller) {}

std::unique_ptr<TouchToFillPasswordGenerationController>
PasswordGenerationControllerImpl::CreateTouchToFillGenerationController() {
  return std::make_unique<TouchToFillPasswordGenerationController>(
      active_frame_driver_, &GetWebContents(), *generation_element_data_,
      std::make_unique<TouchToFillPasswordGenerationBridgeImpl>(),
      base::BindOnce(&PasswordGenerationControllerImpl::
                         OnTouchToFillForGenerationDismissed,
                     base::Unretained(this)),
      ManualFillingController::GetOrCreate(&GetWebContents()));
}

std::unique_ptr<TouchToFillPasswordGenerationController>
PasswordGenerationControllerImpl::
    CreateTouchToFillGenerationControllerForTesting(
        std::unique_ptr<TouchToFillPasswordGenerationBridge> bridge,
        base::WeakPtr<ManualFillingController> manual_filling_controller) {
  return std::make_unique<TouchToFillPasswordGenerationController>(
      active_frame_driver_, &GetWebContents(), *generation_element_data_,
      std::move(bridge),
      base::BindOnce(&PasswordGenerationControllerImpl::
                         OnTouchToFillForGenerationDismissed,
                     base::Unretained(this)),
      manual_filling_controller);
}

bool PasswordGenerationControllerImpl::TryToShowGenerationTouchToFill(
    bool has_saved_credentials) {
  CHECK(touch_to_fill_generation_state_ != TouchToFillState::kIsShowing);

  if (has_saved_credentials) {
    password_manager::metrics_util::
        LogTouchToFillPasswordGenerationTriggerOutcome(
            TouchToFillOutcome::kHasSavedCredentials);
    return false;
  }

  bool dismissed_4_times_in_a_row =
      client_->GetPrefs()->GetInteger(
          kPasswordGenerationBottomSheetDismissCount) >=
      TouchToFillPasswordGenerationController::kMaxAllowedNumberOfDismisses;
  if (dismissed_4_times_in_a_row) {
    password_manager::metrics_util::
        LogTouchToFillPasswordGenerationTriggerOutcome(
            TouchToFillOutcome::kDismissed4TimesInARow);
    return false;
  }

  if (touch_to_fill_generation_state_ == TouchToFillState::kWasShown) {
    password_manager::metrics_util::
        LogTouchToFillPasswordGenerationTriggerOutcome(
            TouchToFillOutcome::kShownBefore);
    return false;
  }

  if (ShowBottomSheet(PasswordGenerationType::kTouchToFill)) {
    password_manager::metrics_util::
        LogTouchToFillPasswordGenerationTriggerOutcome(
            TouchToFillOutcome::kShown);
    return true;
  }

  password_manager::metrics_util::
      LogTouchToFillPasswordGenerationTriggerOutcome(
          TouchToFillOutcome::kFailedToDisplay);
  return false;
}

bool PasswordGenerationControllerImpl::ShowBottomSheet(
    PasswordGenerationType type) {
  touch_to_fill_generation_controller_ =
      create_touch_to_fill_generation_controller_.Run();
  Profile* profile =
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
  std::string account = password_manager::GetDisplayableAccountName(
      SyncServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile));
  if (!touch_to_fill_generation_controller_->ShowTouchToFill(
          std::move(account), type, client_->GetPrefs())) {
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
  if (!active_frame_driver_) {
    return false;
  }
  return active_frame_driver_.get() == driver;
}

void PasswordGenerationControllerImpl::ResetFocusState() {
  if (manual_filling_controller_) {
    manual_filling_controller_->OnAccessoryActionAvailabilityChanged(
        ShouldShowAction(false),
        autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC);
  }
  active_frame_driver_.reset();
  generation_element_data_.reset();
  manual_generation_requested_ = false;
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

void PasswordGenerationControllerImpl::WebContentsDestroyed() {
  // Avoid invalid pointers to other `WebContentsUserData`, e.g. `client_`.
  GetWebContents().RemoveUserData(UserDataKey());
  // `this` is now destroyed - do not add code here.
}

void PasswordGenerationControllerImpl::
    ResetPasswordGenerationDismissBottomSheetCount() {
  client_->GetPrefs()->SetInteger(kPasswordGenerationBottomSheetDismissCount,
                                  0);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PasswordGenerationControllerImpl);

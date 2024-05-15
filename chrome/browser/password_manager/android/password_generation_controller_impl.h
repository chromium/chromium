// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_GENERATION_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_GENERATION_CONTROLLER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/android/password_generation_controller.h"
#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/touch_to_fill_password_generation_bridge.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/geometry/rect.h"

class ManualFillingController;
class TouchToFillPasswordGenerationController;
struct PasswordGenerationElementData;

namespace password_manager {
class ContentPasswordManagerDriver;
class PasswordManagerClient;
}  // namespace password_manager

// In its current state, this class is not involved in the editing flow for
// a generated password.
//
// Use either PasswordGenerationController::GetOrCreate or
// PasswordGenerationController::GetIfExisting to obtain instances of this
// class.
class PasswordGenerationControllerImpl
    : public PasswordGenerationController,
      public content::WebContentsObserver,
      public content::WebContentsUserData<PasswordGenerationControllerImpl> {
 public:
  using CreateTouchToFillGenerationControllerFactory = base::RepeatingCallback<
      std::unique_ptr<TouchToFillPasswordGenerationController>()>;

  PasswordGenerationControllerImpl(const PasswordGenerationControllerImpl&) =
      delete;
  PasswordGenerationControllerImpl& operator=(
      const PasswordGenerationControllerImpl&) = delete;

  ~PasswordGenerationControllerImpl() override;

  // PasswordGenerationController:
  base::WeakPtr<password_manager::ContentPasswordManagerDriver>
  GetActiveFrameDriver() const override;
  void FocusedInputChanged(
      autofill::mojom::FocusedFieldType focused_field_type,
      base::WeakPtr<password_manager::ContentPasswordManagerDriver> driver)
      override;
  void OnAutomaticGenerationAvailable(
      base::WeakPtr<password_manager::ContentPasswordManagerDriver>
          target_frame_driver,
      const autofill::password_generation::PasswordGenerationUIData& ui_data,
      bool has_saved_credentials,
      gfx::RectF element_bounds_in_screen_space) override;
  void ShowManualGenerationDialog(
      const password_manager::ContentPasswordManagerDriver* target_frame_driver,
      const autofill::password_generation::PasswordGenerationUIData& ui_data)
      override;
  void OnGenerationRequested(
      autofill::password_generation::PasswordGenerationType type) override;
  void GeneratedPasswordAccepted(
      const std::u16string& password,
      base::WeakPtr<password_manager::ContentPasswordManagerDriver> driver,
      autofill::password_generation::PasswordGenerationType type) override;
  void GeneratedPasswordRejected(
      autofill::password_generation::PasswordGenerationType type) override;
  void HideBottomSheetIfNeeded() override;
  // Creates the |TouchToFillPasswordGenerationController| with mocked bridge
  // for testing.
  std::unique_ptr<TouchToFillPasswordGenerationController>
  CreateTouchToFillGenerationControllerForTesting(
      std::unique_ptr<TouchToFillPasswordGenerationBridge> bridge,
      base::WeakPtr<ManualFillingController> manual_filling_controller)
      override;
  gfx::NativeWindow top_level_native_window() override;
  content::WebContents* web_contents() override;
  autofill::FieldSignature get_field_signature_for_testing() override;
  autofill::FormSignature get_form_signature_for_testing() override;

  // Like |CreateForWebContents|, it creates the controller and attaches it to
  // the given |web_contents|. Additionally, it allows injecting mocks for
  // testing.
  static void CreateForWebContentsForTesting(
      content::WebContents* web_contents,
      password_manager::PasswordManagerClient* client,
      base::WeakPtr<ManualFillingController> manual_filling_controller,
      CreateTouchToFillGenerationControllerFactory
          create_touch_to_fill_generation_controller);

 protected:
  // Callable in tests.
  explicit PasswordGenerationControllerImpl(content::WebContents* web_contents);

 private:
  enum class TouchToFillState {
    kNone,
    kIsShowing,
    kWasShown,
  };

  friend class content::WebContentsUserData<PasswordGenerationControllerImpl>;

  // Constructor that allows to inject a mock or fake dependencies
  PasswordGenerationControllerImpl(
      content::WebContents* web_contents,
      password_manager::PasswordManagerClient* client,
      base::WeakPtr<ManualFillingController> manual_filling_controller,
      CreateTouchToFillGenerationControllerFactory
          create_touch_to_fill_generation_controller);

  // content::WebContentsObserver:
  // Called when the `content::WebContents` render frame is deleted.
  // Ensures that the password generation bottom sheet is hidden when the frame
  // is removed.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

  std::unique_ptr<TouchToFillPasswordGenerationController>
  CreateTouchToFillGenerationController();

  // Checks if the given PasswordManagerDriver is the same as the one
  // belonging to the currently considered active frame for generation.
  // The active frame is the latest focused frame that received a field focus
  // event.
  bool IsActiveFrameDriver(
      const password_manager::ContentPasswordManagerDriver* driver) const;

  // Called to show the generation modal dialog. |manual| - whether the
  // dialog was shown for a manual or automatic generation flow. This is used
  // for metrics.
  void ShowDialog(autofill::password_generation::PasswordGenerationType type);

  bool TryToShowGenerationTouchToFill(bool has_saved_credentials);

  bool ShowBottomSheet(
      autofill::password_generation::PasswordGenerationType type);

  void OnTouchToFillForGenerationDismissed();

  // Resets the current active frame driver, as well as the dialog if shown
  // and the generation element data.
  void ResetFocusState();

  // Sets the number of generation bottom sheet rejections in a row to 0.
  // Expected to be called when user voluntary triggers password generation.
  void ResetPasswordGenerationDismissBottomSheetCount();

  // The PasswordManagerClient associated with the current `web_contents_`.
  // Used to tell the renderer that manual generation was requested.
  const raw_ptr<password_manager::PasswordManagerClient> client_;

  // Data for the generation element used to generate the password.
  std::unique_ptr<PasswordGenerationElementData> generation_element_data_;

  // Password manager driver for the currently active frame. This is set
  // when a password field focus event arrives from the renderer and unset
  // whenever a focus event for a non-password field is received.
  base::WeakPtr<password_manager::ContentPasswordManagerDriver>
      active_frame_driver_;

  // The manual filling controller object to forward client requests to.
  base::WeakPtr<ManualFillingController> manual_filling_controller_;

  std::unique_ptr<TouchToFillPasswordGenerationController>
      touch_to_fill_generation_controller_;

  // Creation callback for the password generation bottom sheet controller to
  // facilitate testing.
  CreateTouchToFillGenerationControllerFactory
      create_touch_to_fill_generation_controller_;

  // Whether manual generation was requested from the UI. Used to filter out
  // unexpected or delayed manual generation responses from the renderer.
  bool manual_generation_requested_ = false;

  // Whether password generation bottom sheet was already shown.
  TouchToFillState touch_to_fill_generation_state_ = TouchToFillState::kNone;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_GENERATION_CONTROLLER_IMPL_H_

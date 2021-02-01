// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_GENERATION_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_GENERATION_CONTROLLER_IMPL_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/android/password_generation_controller.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/geometry/rect.h"

class ManualFillingController;
class PasswordGenerationDialogViewInterface;

namespace password_manager {
class PasswordManagerDriver;
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
      public content::WebContentsUserData<PasswordGenerationControllerImpl> {
 public:
  using CreateDialogFactory = base::RepeatingCallback<std::unique_ptr<
      PasswordGenerationDialogViewInterface>(PasswordGenerationController*)>;
  ~PasswordGenerationControllerImpl() override;

  // PasswordGenerationController:
  base::WeakPtr<password_manager::PasswordManagerDriver> GetActiveFrameDriver()
      const override;
  void FocusedInputChanged(
      autofill::mojom::FocusedFieldType focused_field_type,
      base::WeakPtr<password_manager::PasswordManagerDriver> driver) override;
  void OnAutomaticGenerationAvailable(
      const password_manager::PasswordManagerDriver* target_frame_driver,
      const autofill::password_generation::PasswordGenerationUIData& ui_data,
      gfx::RectF element_bounds_in_screen_space) override;
  void ShowManualGenerationDialog(
      const password_manager::PasswordManagerDriver* target_frame_driver,
      const autofill::password_generation::PasswordGenerationUIData& ui_data)
      override;
  void OnGenerationRequested(
      autofill::password_generation::PasswordGenerationType type) override;
  void GeneratedPasswordAccepted(
      const base::string16& password,
      base::WeakPtr<password_manager::PasswordManagerDriver> driver,
      autofill::password_generation::PasswordGenerationType type) override;
  void GeneratedPasswordRejected(
      autofill::password_generation::PasswordGenerationType type) override;
  gfx::NativeWindow top_level_native_window() const override;

  // Like |CreateForWebContents|, it creates the controller and attaches it to
  // the given |web_contents|. Additionally, it allows injecting mocks for
  // testing.
  static void CreateForWebContentsForTesting(
      content::WebContents* web_contents,
      password_manager::PasswordManagerClient* client,
      base::WeakPtr<ManualFillingController> manual_filling_controller,
      CreateDialogFactory create_dialog_callback);

 protected:
  // Callable in tests.
  explicit PasswordGenerationControllerImpl(content::WebContents* web_contents);

 private:
  // Data including the form and field for which generation was requested,
  // their signatures and the maximum password size.
  struct GenerationElementData;

  friend class content::WebContentsUserData<PasswordGenerationControllerImpl>;

  // Constructor that allows to inject a mock or fake dependencies
  PasswordGenerationControllerImpl(
      content::WebContents* web_contents,
      password_manager::PasswordManagerClient* client,
      base::WeakPtr<ManualFillingController> manual_filling_controller,
      CreateDialogFactory create_dialog_callback);

  // Checks if the given PasswordManagerDriver is the same as the one
  // belonging to the currently considered active frame for generation.
  // The active frame is the latest focused frame that received a field focus
  // event.
  bool IsActiveFrameDriver(
      const password_manager::PasswordManagerDriver* driver) const;

  // Called to show the generation modal dialog. |manual| - whether the
  // dialog was shown for a manual or automatic generation flow. This is used
  // for metrics.
  void ShowDialog(autofill::password_generation::PasswordGenerationType type);

  // Resets the current active frame driver, as well as the dialog if shown
  // and the generation element data.
  void ResetState();

  // The tab for which this class is scoped.
  content::WebContents* web_contents_;

  // The PasswordManagerClient associated with the current |web_contents_|.
  // Used to tell the renderer that manual generation was requested.
  password_manager::PasswordManagerClient* client_ = nullptr;

  // Data for the generation element used to generate the password.
  std::unique_ptr<GenerationElementData> generation_element_data_;

  // Password manager driver for the currently active frame. This is set
  // when a password field focus event arrives from the renderer and unset
  // whenever a focus event for a non-password field is received.
  base::WeakPtr<password_manager::PasswordManagerDriver> active_frame_driver_;

  // The manual filling controller object to forward client requests to.
  base::WeakPtr<ManualFillingController> manual_filling_controller_;

  // Modal dialog view meant to display the generated password.
  std::unique_ptr<PasswordGenerationDialogViewInterface> dialog_view_;

  // Creation callback for the modal dialog view meant to facilitate testing.
  CreateDialogFactory create_dialog_factory_;

  // Whether manual generation was requested from the UI. Used to filter out
  // unexpected or delayed manual generation responses from the renderer.
  bool manual_generation_requested_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationControllerImpl);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_GENERATION_CONTROLLER_IMPL_H_

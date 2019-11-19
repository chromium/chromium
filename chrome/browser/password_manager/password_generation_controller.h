// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_GENERATION_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_GENERATION_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rect.h"

namespace password_manager {
class PasswordManagerDriver;
class PasswordManagerDriver;
}  // namespace password_manager

// Interface for the controller responsible for overseeing the UI flow for
// password generation.
//
// As part of this, it communicates with the PasswordAccessoryController and
// it manages the modal dialog used to display the generated password.
//
// There is a single instance per WebContents that can be accessed by calling:
//     PasswordGenerationController::GetOrCreate(web_contents);
// On the first call, an instance is attached to |web_contents|, so it can be
// returned by subsequent calls.
class PasswordGenerationController {
 public:
  PasswordGenerationController() = default;
  virtual ~PasswordGenerationController() = default;

  // Returns true if the generation controller may exist for |web_contents|.
  // Otherwise (e.g. if the keyboard accessory is not allowed),
  // it returns false.
  static bool AllowedForWebContents(content::WebContents* web_contents);

  // Returns a reference to the unique PasswordGenerationController associated
  // with |web_contents|. A new instance is created if the first time this
  // function is called.
  static PasswordGenerationController* GetOrCreate(
      content::WebContents* web_contents);

  // Returns a reference to the PasswordGenerationController associated with the
  // |web_contents| or null if there is no such instance.
  static PasswordGenerationController* GetIfExisting(
      content::WebContents* web_contents);

  // --------------------------------------------------
  // Methods called by the ChromePasswordManagerClient:
  // --------------------------------------------------

  // Returns the driver associated with the frame that is considered active
  // for generation.
  virtual base::WeakPtr<password_manager::PasswordManagerDriver>
  GetActiveFrameDriver() const = 0;

  // This signals that the focus has moved. |focused_field_type| tells
  // the generation controller whether the focus moved to a fillable password
  // field. This event sets/unsets the active frame for generation.
  virtual void FocusedInputChanged(
      autofill::mojom::FocusedFieldType focused_field_type,
      base::WeakPtr<password_manager::PasswordManagerDriver> driver) = 0;

  // Notifies the UI that automatic password generation is available.
  // A button should be displayed in the accessory bar.
  virtual void OnAutomaticGenerationAvailable(
      const password_manager::PasswordManagerDriver* target_frame_driver,
      const autofill::password_generation::PasswordGenerationUIData& ui_data,
      gfx::RectF element_bounds_in_screen_space) = 0;

  // This is called after the user requested manual generation and the
  // corresponding setup was done in the renderer.
  virtual void ShowManualGenerationDialog(
      const password_manager::PasswordManagerDriver* target_frame_driver,
      const autofill::password_generation::PasswordGenerationUIData&
          ui_data) = 0;

  // -------------------------
  // Methods called by the UI:
  // -------------------------

  // Called by the UI code to signal that the user requested password
  // generation. This should prompt a modal dialog with the generated password.
  // |type| - whether the requests originates from a an automatic
  // generation flow or from a manual one.
  virtual void OnGenerationRequested(
      autofill::password_generation::PasswordGenerationType type) = 0;

  // Called from the modal dialog if the user accepted the generated password.
  // |driver| is used to communicate the message back to the renderer.
  // |type| what type of generation led to the accepted password
  // (automatic or manual).
  virtual void GeneratedPasswordAccepted(
      const base::string16& password,
      base::WeakPtr<password_manager::PasswordManagerDriver> driver,
      autofill::password_generation::PasswordGenerationType type) = 0;

  // Called from the modal dialog if the user rejected the generated password.
  // |type| what type of generation led to the rejected password
  // (automatic or manual).
  virtual void GeneratedPasswordRejected(
      autofill::password_generation::PasswordGenerationType type) = 0;

  // -----------------
  // Member accessors:
  // -----------------

  virtual gfx::NativeWindow top_level_native_window() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationController);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_GENERATION_CONTROLLER_H_

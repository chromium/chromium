// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_MANUAL_FILLING_CONTROLLER_H_
#define CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_MANUAL_FILLING_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_data.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_enums.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/web_contents_user_data.h"

// Controller interface for the view that includes the keyboard accessory and
// the accessory sheet below it. Implementations of this interface create and
// own a ManualFillingViewInterface.
//
// The manual filling controller forwards requests from type-specific accessory
// controllers (Passwords and Autofill) to the view. The view notifies this
// controller about interactions (such as requesting to fill a password
// suggestion) and forwards the request to the corresponding type-specific
// accessory controller.
//
// This controller also implements the logic to show/hide the keyboard
// accessory.
//
// ManualFillingController::GetOrCreate() should be used
// by type-specific controllers to obtain an instance of this class for a given
// WebContents. There is only one instance per WebContents, which is created the
// first time |GetOrCreate()| is invoked.
//
// Usage example:
//   auto controller = ManualFillingController::GetOrCreate(web_contents);
//   DCHECK(controller);
//   controller->RefreshSuggestionsForField(...);
class ManualFillingController {
 public:
  // The controller checks if at least one of these sources needs the accessory
  // to be displayed.
  enum class FillingSource {
    AUTOFILL,
    PASSWORD_FALLBACKS,
    CREDIT_CARD_FALLBACKS,
    ADDRESS_FALLBACKS,
  };

  using ShouldShowAction = base::StrongAlias<struct ShouldShowActionTag, bool>;

  ManualFillingController() = default;

  ManualFillingController(const ManualFillingController&) = delete;
  ManualFillingController& operator=(const ManualFillingController&) = delete;

  virtual ~ManualFillingController() = default;

  // Returns a weak pointer to the unique ManualFillingController instance
  // associated with a WebContents. The first invocation creates an instance
  // and attaches it to the WebContents; the same instance is returned by all
  // future invocations for the same WebContents.
  static base::WeakPtr<ManualFillingController> GetOrCreate(
      content::WebContents* contents);

  // Returns a weak pointer to the unique ManualFillingController instance
  // associated with a WebContents.
  static base::WeakPtr<ManualFillingController> Get(
      content::WebContents* contents);

  // --------------------------------------------
  // Methods called by type-specific controllers.
  // --------------------------------------------

  // Notifies that the focused field changed which allows the controller to
  // update the UI visibility.
  virtual void NotifyFocusedInputChanged(
      autofill::FieldRendererId focused_field_id,
      autofill::mojom::FocusedFieldType focused_field_type) = 0;

  // Returns the last field id passed to `NotifyFocusedInputChanged()`.
  // TODO: crbug.com/327838324 - Remove this method from the public interface.
  virtual autofill::FieldGlobalId GetLastFocusedFieldId() const = 0;

  // Reports for a source whether it provides suggestions or just default
  // options. The controller then updates the UI visibility accordingly.
  // TODO(crbug.com/40165275): Use
  // AccessoryController::RegisterFillingSourceObserver to get this signal from
  // sheet controllers.
  virtual void UpdateSourceAvailability(FillingSource source,
                                        bool has_suggestions) = 0;

  // Explicitly hides all manual filling UI without checking any filling source.
  // E.g. after autofilling suggestions, or generating a password.
  virtual void Hide() = 0;

  // Notifies the view that availability of the given action changed.
  virtual void OnAccessoryActionAvailabilityChanged(
      ShouldShowAction shouldShowAction,
      autofill::AccessoryAction action) = 0;

  // Instructs the view to show the manual filling sheet for the given
  // |tab_type|.
  virtual void ShowAccessorySheetTab(
      const autofill::AccessoryTabType& tab_type) = 0;

  // --------------------------
  // Methods called by UI code:
  // --------------------------

  // Called by the UI code to request that |text_to_fill| is to be filled into
  // the currently focused field. Forwards the request to a type-specific
  // accessory controller.
  virtual void OnFillingTriggered(
      autofill::AccessoryTabType type,
      const autofill::AccessorySheetField& selection) = 0;

  // Called by the UI code to request that `passkey_id` should be used to
  // authenticate the user.
  virtual void OnPasskeySelected(autofill::AccessoryTabType type,
                                 const std::vector<uint8_t>& passkey_id) = 0;

  // Called by the UI code because a user triggered the |selected_action|,
  // such as "Manage passwords...".
  virtual void OnOptionSelected(
      autofill::AccessoryAction selected_action) const = 0;

  // Called by the UI code because a user toggled the |toggled_action|,
  // such as "Save passwords for this site".
  virtual void OnToggleChanged(autofill::AccessoryAction toggled_action,
                               bool enabled) const = 0;

  // Called by the UI to explicitly request a new sheet of the given type.
  virtual void RequestAccessorySheet(
      autofill::AccessoryTabType tab_type,
      base::OnceCallback<void(autofill::AccessorySheetData)> callback) = 0;

  // -----------------
  // Member accessors:
  // -----------------

  // The web page view containing the focused field.
  virtual gfx::NativeView container_view() const = 0;
};

#endif  // CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_MANUAL_FILLING_CONTROLLER_H_

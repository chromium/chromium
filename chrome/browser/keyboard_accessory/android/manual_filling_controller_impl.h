// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_MANUAL_FILLING_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_MANUAL_FILLING_CONTROLLER_IMPL_H_

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/memory_dump_provider.h"
#include "chrome/browser/autofill/manual_filling_view_interface.h"
#include "chrome/browser/keyboard_accessory/android/accessory_controller.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_enums.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/plus_addresses/plus_address_types.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {
class AddressAccessoryController;
class PaymentMethodAccessoryController;
}  // namespace autofill

class AffiliatedPlusProfilesCache;
class PasswordAccessoryController;

// Use ManualFillingController::GetOrCreate to obtain instances of this class.
class ManualFillingControllerImpl
    : public ManualFillingController,
      public content::WebContentsUserData<ManualFillingControllerImpl>,
      public base::trace_event::MemoryDumpProvider {
 public:
  ManualFillingControllerImpl(const ManualFillingControllerImpl&) = delete;
  ManualFillingControllerImpl& operator=(const ManualFillingControllerImpl&) =
      delete;

  ~ManualFillingControllerImpl() override;

  // ManualFillingController:
  void NotifyFocusedInputChanged(
      autofill::FieldRendererId focused_field_id,
      autofill::mojom::FocusedFieldType focused_field_type) override;
  autofill::FieldGlobalId GetLastFocusedFieldId() const override;
  void UpdateSourceAvailability(FillingSource source,
                                bool has_suggestions) override;
  void Hide() override;
  void OnAccessoryActionAvailabilityChanged(
      ShouldShowAction shouldShowAction,
      autofill::AccessoryAction action) override;
  void ShowAccessorySheetTab(
      const autofill::AccessoryTabType& tab_type) override;
  void OnFillingTriggered(
      autofill::AccessoryTabType type,
      const autofill::AccessorySheetField& selection) override;
  void OnPasskeySelected(autofill::AccessoryTabType type,
                         const std::vector<uint8_t>& passkey_id) override;
  void OnOptionSelected(
      autofill::AccessoryAction selected_action) const override;
  void OnToggleChanged(autofill::AccessoryAction toggled_action,
                       bool enabled) const override;
  void RequestAccessorySheet(
      autofill::AccessoryTabType tab_type,
      base::OnceCallback<void(autofill::AccessorySheetData)> callback) override;

  gfx::NativeView container_view() const override;

  // Returns a weak pointer for this object.
  base::WeakPtr<ManualFillingController> AsWeakPtr();

  // Like |CreateForWebContents|, it creates the controller and attaches it to
  // the given |web_contents|. Additionally, it allows injecting a fake/mock
  // view and type-specific controllers.
  static void CreateForWebContentsForTesting(
      content::WebContents* web_contents,
      base::WeakPtr<PasswordAccessoryController> pwd_controller,
      base::WeakPtr<autofill::AddressAccessoryController> address_controller,
      base::WeakPtr<autofill::PaymentMethodAccessoryController>
          payment_method_controller,
      std::unique_ptr<ManualFillingViewInterface> test_view);

#if defined(UNIT_TEST)
  // Returns the held view for testing.
  ManualFillingViewInterface* view() const { return view_.get(); }

  // Returns the plus profiles cache for testing.
  AffiliatedPlusProfilesCache* plus_profiles_cache() const {
    return plus_profiles_cache_.get();
  }
#endif  // defined(UNIT_TEST)

 protected:
  friend class ManualFillingController;  // Allow protected access in factories.

  // Enables calling initialization code that relies on a fully constructed
  // ManualFillingController that is attached to a WebContents instance.
  // This is matters for subcomponents which lazily trigger the creation of this
  // class. If called in constructors, it would cause an infinite creation loop.
  void Initialize();

 private:
  friend class content::WebContentsUserData<ManualFillingControllerImpl>;

  // Required for construction via |CreateForWebContents|:
  explicit ManualFillingControllerImpl(content::WebContents* contents);

  // Constructor that allows to inject a mock view.
  ManualFillingControllerImpl(
      content::WebContents* web_contents,
      base::WeakPtr<PasswordAccessoryController> pwd_controller,
      base::WeakPtr<autofill::AddressAccessoryController> address_controller,
      base::WeakPtr<autofill::PaymentMethodAccessoryController>
          payment_method_controller,
      std::unique_ptr<ManualFillingViewInterface> view);

  // Creates the plus profiles cache if the feature flag is enabled and all its
  // dependencies are not `nullptr`.
  void InitializePlusProfilesCache();

  // MemoryDumpProvider:
  bool OnMemoryDump(
      const base::trace_event::MemoryDumpArgs& args,
      base::trace_event::ProcessMemoryDump* process_memory_dump) override;

  // Returns true if the keyboard accessory needs to be shown.
  bool ShouldShowAccessory() const;

  // Adjusts visibility based on focused field type and available suggestions.
  void UpdateVisibility();

  // Registers this filling controller as observer on all sources which are
  // allowed for this tab. This means `OnSourceAvailabilityChanged()` triggers
  // as soon as the observed source changes.
  void RegisterObserverForAllowedSources();

  void OnSourceAvailabilityChanged(
      FillingSource source,
      AccessoryController* source_controller,
      AccessoryController::IsFillingSourceAvailable is_source_available);

  // Returns the controller that is responsible for a tab of given `type`.
  AccessoryController* GetControllerForTabType(
      autofill::AccessoryTabType type) const;

  // Returns the controller that is responsible to handle requests for a given
  // `filling_source`.
  AccessoryController* GetControllerForFillingSource(
      const FillingSource& filling_source) const;

  // Returns the controller that is responsible for a given `action`.
  AccessoryController* GetControllerForAction(
      autofill::AccessoryAction action) const;

  // This set contains sources to be shown to the user.
  base::flat_set<FillingSource> available_sources_;

  // The unique renderer ID of the last known selected field.
  autofill::FieldGlobalId last_focused_field_id_;

  // Type of the last known selected field. Helps to determine UI visibility.
  autofill::mojom::FocusedFieldType last_focused_field_type_ =
      autofill::mojom::FocusedFieldType::kUnknown;

  // Controllers which handle events relating to a specific tab and the
  // associated data.
  base::WeakPtr<PasswordAccessoryController> pwd_controller_;
  base::WeakPtr<autofill::AddressAccessoryController> address_controller_;
  base::WeakPtr<autofill::PaymentMethodAccessoryController>
      payment_method_controller_;

  // This cache is initialized if the manual fallback feature flag is enabled
  // and its dependencies are not null. The cached plus providers are accessed
  // by individual accessory controllers.
  // The cache is populated when the manual filling component is shown and
  // cleared when the component is hidden. The cache is cleared for memory
  // efficiency to avoid caching the list of plus addresses for every open
  // browser tab.
  std::unique_ptr<AffiliatedPlusProfilesCache> plus_profiles_cache_;

  // Hold the native instance of the view. Must be last declared and initialized
  // member so the view can be created in the constructor with a fully set up
  // controller instance.
  std::unique_ptr<ManualFillingViewInterface> view_ =
      ManualFillingViewInterface::Create(this, &GetWebContents());

  base::WeakPtrFactory<ManualFillingControllerImpl> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_MANUAL_FILLING_CONTROLLER_IMPL_H_

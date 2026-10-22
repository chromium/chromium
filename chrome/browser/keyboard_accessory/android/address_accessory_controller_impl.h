// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_ADDRESS_ACCESSORY_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_ADDRESS_ACCESSORY_CONTROLLER_IMPL_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/keyboard_accessory/android/address_accessory_controller.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

// Use either AddressAccessoryController::GetOrCreate or
// AddressAccessoryController::GetIfExisting to obtain instances of this class.
// This class exists for every tab and should never store state based on the
// contents of one of its frames.
class AddressAccessoryControllerImpl
    : public AddressAccessoryController,
      public AddressDataManager::Observer,
      public content::WebContentsUserData<AddressAccessoryControllerImpl> {
 public:
  AddressAccessoryControllerImpl(const AddressAccessoryControllerImpl&) =
      delete;
  AddressAccessoryControllerImpl& operator=(
      const AddressAccessoryControllerImpl&) = delete;

  ~AddressAccessoryControllerImpl() override;

  // AccessoryController:
  void RegisterFillingSourceObserver(FillingSourceObserver observer) override;
  std::optional<AccessorySheetData> GetSheetData() const override;
  void OnFillingTriggered(FieldGlobalId focused_field_id,
                          const AccessorySheetField& selection) override;
  void OnPasskeySelected(const std::vector<uint8_t>& passkey_id) override;
  void OnOptionSelected(AccessoryAction selected_action) override;
  void OnToggleChanged(AccessoryAction toggled_action, bool enabled) override;

  // AddressAccessoryController:

  void RefreshSuggestions() override;
  base::WeakPtr<AddressAccessoryController> AsWeakPtr() override;

  // AddressDataManagerObserver:
  void OnAddressDataChanged() override;

 private:
  friend class content::WebContentsUserData<AddressAccessoryControllerImpl>;

  // Required for construction via |CreateForWebContents|:
  explicit AddressAccessoryControllerImpl(content::WebContents* contents);

  // Constructs a vector of available manual fallback actions subject to
  // enabled features and available user data.
  std::vector<FooterCommand> CreateManageAddressesFooter() const;



  // Given that `RenderFrameHost` and `ContentAutofillDriver` exist, enters the
  // `value` into the field identified by the `focused_field_id`.
  void FillValueIntoField(FieldGlobalId focused_field_id,
                          const std::u16string& value);

  // The observer to notify if available suggestions change.
  FillingSourceObserver source_observer_;

  // Responsible for observing the `AddressDataManager` of the current profile.
  base::ScopedObservation<AddressDataManager, AddressDataManager::Observer>
      adm_observation_{this};



  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<AddressAccessoryControllerImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_ADDRESS_ACCESSORY_CONTROLLER_IMPL_H_

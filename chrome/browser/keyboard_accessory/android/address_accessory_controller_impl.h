// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_ADDRESS_ACCESSORY_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_ADDRESS_ACCESSORY_CONTROLLER_IMPL_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/keyboard_accessory/android/address_accessory_controller.h"
#include "chrome/browser/keyboard_accessory/android/affiliated_plus_profiles_provider.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

class ManualFillingController;

namespace plus_addresses {
class AllPlusAddressesBottomSheetController;
class PlusAddressService;
}  // namespace plus_addresses

namespace autofill {
class PersonalDataManager;

// Use either AddressAccessoryController::GetOrCreate or
// AddressAccessoryController::GetIfExisting to obtain instances of this class.
// This class exists for every tab and should never store state based on the
// contents of one of its frames.
class AddressAccessoryControllerImpl
    : public AddressAccessoryController,
      public PersonalDataManagerObserver,
      public AffiliatedPlusProfilesProvider::Observer,
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
  void RegisterPlusProfilesProvider(
      base::WeakPtr<AffiliatedPlusProfilesProvider> provider) override;
  void RefreshSuggestions() override;
  base::WeakPtr<AddressAccessoryController> AsWeakPtr() override;

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override;

  // AffiliatedPlusProfilesProvider::Observer:
  void OnAffiliatedPlusProfilesFetched() override;

  // Like |CreateForWebContents|, it creates the controller and attaches it to
  // the given |web_contents|. Additionally, it allows inject a manual filling
  // controller.
  static void CreateForWebContentsForTesting(
      content::WebContents* web_contents,
      base::WeakPtr<ManualFillingController> mf_controller);

 private:
  friend class content::WebContentsUserData<AddressAccessoryControllerImpl>;

  // Required for construction via |CreateForWebContents|:
  explicit AddressAccessoryControllerImpl(content::WebContents* contents);

  // Constructor that allows to inject a mock filling controller.
  AddressAccessoryControllerImpl(
      content::WebContents* web_contents,
      base::WeakPtr<ManualFillingController> mf_controller);

  // Constructs a vector of available manual fallback actions subject to
  // enabled features and available user data.
  std::vector<FooterCommand> CreateManageAddressesFooter() const;

  // Fills `plus_address` into the web form field identified by
  // `focused_field_id`. Called when manually triggered plus address creation
  // bottom sheet is accepted by the user.
  void OnPlusAddressCreated(FieldGlobalId focused_field_id,
                            const std::string& plus_address);

  // Triggers the filling `plus_address` into the field with `focused_field_id`.
  void OnPlusAddressSelected(
      FieldGlobalId focused_field_id,
      base::optional_ref<const std::string> plus_address);

  // Given that `RenderFrameHost` and `ContentAutofillDriver` exist, enters the
  // `value` into the field identified by the `focused_field_id`.
  void FillValueIntoField(FieldGlobalId focused_field_id,
                          const std::u16string& value);

  // Lazy-initializes and returns the ManualFillingController for the current
  // |web_contents_|. The lazy initialization allows injecting mocks for tests.
  base::WeakPtr<ManualFillingController> GetManualFillingController();

  // The observer to notify if available suggestions change.
  FillingSourceObserver source_observer_;

  // The password accessory controller object to forward client requests to.
  base::WeakPtr<ManualFillingController> mf_controller_;

  // The plus profiles provider that is used to generate the plus profiles
  // section for the frontend.
  base::WeakPtr<AffiliatedPlusProfilesProvider> plus_profiles_provider_;

  // The data manager used to retrieve the profiles.
  raw_ptr<PersonalDataManager> personal_data_manager_;

  const raw_ptr<const plus_addresses::PlusAddressService> plus_address_service_;

  std::unique_ptr<plus_addresses::AllPlusAddressesBottomSheetController>
      all_plus_addresses_bottom_sheet_controller_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<AddressAccessoryControllerImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_ADDRESS_ACCESSORY_CONTROLLER_IMPL_H_

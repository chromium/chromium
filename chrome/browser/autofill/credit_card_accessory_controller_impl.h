// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_CREDIT_CARD_ACCESSORY_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_AUTOFILL_CREDIT_CARD_ACCESSORY_CONTROLLER_IMPL_H_

#include "chrome/browser/autofill/credit_card_accessory_controller.h"

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "content/public/browser/web_contents_user_data.h"

class ManualFillingController;

namespace autofill {

// Use either CreditCardAccessoryController::GetOrCreate or
// CreditCardAccessoryController::GetIfExisting to obtain instances of this
// class.
class CreditCardAccessoryControllerImpl
    : public CreditCardAccessoryController,
      public content::WebContentsUserData<CreditCardAccessoryControllerImpl> {
 public:
  ~CreditCardAccessoryControllerImpl() override;

  // AccessoryController:
  void RegisterFillingSourceObserver(FillingSourceObserver observer) override;
  absl::optional<autofill::AccessorySheetData> GetSheetData() const override;
  void OnFillingTriggered(FieldGlobalId focused_field_id,
                          const UserInfo::Field& selection) override;
  void OnOptionSelected(AccessoryAction selected_action) override;
  void OnToggleChanged(AccessoryAction toggled_action, bool enabled) override;

  // CreditCardAccessoryController:
  void RefreshSuggestions() override;

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override;

  // CreditCardAccessManager::Accessor:
  void OnCreditCardFetched(CreditCardFetchResult result,
                           const CreditCard* credit_card,
                           const std::u16string& cvc) override;

  static void CreateForWebContentsForTesting(
      content::WebContents* web_contents,
      base::WeakPtr<ManualFillingController> mf_controller,
      autofill::PersonalDataManager* personal_data_manager,
      autofill::BrowserAutofillManager* af_manager,
      autofill::AutofillDriver* af_driver);

 private:
  friend class content::WebContentsUserData<CreditCardAccessoryControllerImpl>;

  // Required for construction via |CreateForWebContents|:
  explicit CreditCardAccessoryControllerImpl(content::WebContents* contents);

  // Used by CreateForWebContentsForTesting:
  CreditCardAccessoryControllerImpl(
      content::WebContents* web_contents,
      base::WeakPtr<ManualFillingController> mf_controller,
      PersonalDataManager* personal_data_manager,
      autofill::BrowserAutofillManager* af_manager,
      autofill::AutofillDriver* af_driver);

  void FetchSuggestions();
  base::WeakPtr<ManualFillingController> GetManualFillingController();
  std::vector<CreditCard*> GetCardsFromManager() const;
  autofill::AutofillDriver* GetDriver();
  autofill::BrowserAutofillManager* GetManager() const;

  // Pointers to cards owned by PersonalDataManager.
  std::vector<CreditCard*> cards_cache_;
  // Virtual cards that are created based on the enrollment status of the cards
  // returned by the PersonalDataManager.
  std::vector<std::unique_ptr<CreditCard>> virtual_cards_cache_;
  content::WebContents* web_contents_;
  base::WeakPtr<ManualFillingController> mf_controller_;
  PersonalDataManager* const personal_data_manager_;
  autofill::BrowserAutofillManager* af_manager_for_testing_ = nullptr;
  autofill::AutofillDriver* af_driver_for_testing_ = nullptr;

  // Cached cards that are already unmasked by the user. These are shown to the
  // user in plaintext and won't require any authentication when filling is
  // triggered.
  std::vector<const CachedServerCardInfo*> cached_server_cards_;

  // The observer to notify if available suggestions change.
  FillingSourceObserver source_observer_;

  // OnFillingTriggered() sets this so that OnCreditCardFetched() can assert
  // that the focused frame has not changed and knows the field to be filled.
  FieldGlobalId last_focused_field_id_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_CREDIT_CARD_ACCESSORY_CONTROLLER_IMPL_H_

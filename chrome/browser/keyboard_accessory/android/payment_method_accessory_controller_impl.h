// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_PAYMENT_METHOD_ACCESSORY_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_PAYMENT_METHOD_ACCESSORY_CONTROLLER_IMPL_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/keyboard_accessory/android/payment_method_accessory_controller.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "content/public/browser/web_contents_user_data.h"

class ManualFillingController;

namespace autofill {

class BrowserAutofillManager;

// Use either PaymentMethodAccessoryController::GetOrCreate or
// PaymentMethodAccessoryController::GetIfExisting to obtain instances of this
// class.
class PaymentMethodAccessoryControllerImpl
    : public PaymentMethodAccessoryController,
      public content::WebContentsUserData<PaymentMethodAccessoryControllerImpl> {
 public:
  ~PaymentMethodAccessoryControllerImpl() override;

  // AccessoryController:
  void RegisterFillingSourceObserver(FillingSourceObserver observer) override;
  std::optional<AccessorySheetData> GetSheetData() const override;
  void OnFillingTriggered(FieldGlobalId focused_field_id,
                          const AccessorySheetField& selection) override;
  void OnPasskeySelected(const std::vector<uint8_t>& passkey_id) override;
  void OnOptionSelected(AccessoryAction selected_action) override;
  void OnToggleChanged(AccessoryAction toggled_action, bool enabled) override;

  // PaymentMethodAccessoryController:
  void RefreshSuggestions() override;
  base::WeakPtr<PaymentMethodAccessoryController> AsWeakPtr() override;

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override;

  static void CreateForWebContentsForTesting(
      content::WebContents* web_contents,
      base::WeakPtr<ManualFillingController> mf_controller,
      PersonalDataManager* personal_data_manager,
      BrowserAutofillManager* af_manager,
      AutofillDriver* af_driver);

 private:
  friend class content::WebContentsUserData<PaymentMethodAccessoryControllerImpl>;

  using CardOrVirtualCard =
      absl::variant<const CreditCard*, std::unique_ptr<CreditCard>>;

  // Required for construction via |CreateForWebContents|:
  explicit PaymentMethodAccessoryControllerImpl(content::WebContents* contents);

  // Used by CreateForWebContentsForTesting:
  PaymentMethodAccessoryControllerImpl(
      content::WebContents* web_contents,
      base::WeakPtr<ManualFillingController> mf_controller,
      PersonalDataManager* personal_data_manager,
      BrowserAutofillManager* af_manager,
      AutofillDriver* af_driver);

  // Queries the `personal_data_manager_` for regular and virtual credit cards.
  // Virtual cards are (re-)created based on the enrollment status of the cards
  // and only exist temporarily, so in addition to `CreditCard` pointers, the
  // returned array can contain `unique_ptr<CreditCard>`s for virtual cards.
  // Recreation works only because CreditCard::CreateVirtualCard is a constant
  // projection for a card (based only on its GUID and a static suffix).
  std::vector<CardOrVirtualCard> GetAllCreditCards() const;

  // Cards that are already unmasked by the user. These are shown to the user in
  // plaintext and won't require any authentication when filling is triggered.
  std::vector<const CachedServerCardInfo*> GetUnmaskedCreditCards() const;

  // `OnFillingTriggered()` fetches the credit card and calls this function
  // once the `credit_card` is available. If successful, it fills it.
  void OnCreditCardFetched(CreditCardFetchResult result,
                           const CreditCard* credit_card);

  // Applies the given `value` to the `last_focused_field_id_` if applicable.
  void ApplyToField(const std::u16string& value);

  // Gets promo code offers from personal data manager.
  std::vector<const AutofillOfferData*> GetPromoCodeOffers() const;

  // Gets IBANs from the personal data manager.
  std::vector<Iban> GetIbans() const;

  base::WeakPtr<ManualFillingController> GetManualFillingController();
  AutofillDriver* GetDriver();
  const BrowserAutofillManager* GetAutofillManager() const;
  BrowserAutofillManager* GetAutofillManager();

  content::WebContents& GetWebContents() const;

  // Returns true if the given `selection_id` represents an existing credit
  // card. This method also tries to fetch credit card and fill the form field.
  bool FetchIfCreditCardId(const std::string& selection_id);

  // Returns true if the given `selection_id` represents an existing IBAN. This
  // method also tries to fetch IBAN and fill the form field.
  bool FetchIfIban(const std::string& selection_id);

  base::WeakPtr<ManualFillingController> mf_controller_;
  const raw_ptr<PersonalDataManager> personal_data_manager_;
  raw_ptr<BrowserAutofillManager> af_manager_for_testing_ = nullptr;
  raw_ptr<AutofillDriver> af_driver_for_testing_ = nullptr;

  // The observer to notify if available suggestions change.
  FillingSourceObserver source_observer_;

  // OnFillingTriggered() sets this so that OnCreditCardFetched() can assert
  // that the focused frame has not changed and knows the field to be filled.
  FieldGlobalId last_focused_field_id_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<PaymentMethodAccessoryControllerImpl> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_PAYMENT_METHOD_ACCESSORY_CONTROLLER_IMPL_H_

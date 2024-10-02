// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/keyboard_accessory/android/payment_method_accessory_controller_impl.h"

#include <iterator>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/preferences/autofill/settings_navigation_helper.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

// Return the card art url to displayed in the autofill suggestions. The card
// art is only supported for Capital One virtual cards. For other cards, we show
// the default network icon.
GURL GetCardArtUrl(const CreditCard& card) {
  return card.record_type() == CreditCard::RecordType::kVirtualCard &&
                 card.card_art_url().spec() == kCapitalOneCardArtUrl
             ? card.card_art_url()
             : GURL();
}

std::u16string GetTitle(bool has_suggestions) {
  return has_suggestions
             ? std::u16string()
             : l10n_util::GetStringUTF16(
                   IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_EMPTY_MESSAGE);
}

void AddSimpleField(std::u16string data, UserInfo* user_info, bool enabled) {
  user_info->add_field(AccessorySheetField::Builder()
                           .SetDisplayText(std::move(data))
                           .SetSelectable(enabled)
                           .Build());
}

void AddCardDetailsToUserInfo(const CreditCard& card,
                              UserInfo* user_info,
                              std::u16string cvc,
                              bool enabled) {
  if (card.HasValidExpirationDate()) {
    AddSimpleField(card.Expiration2DigitMonthAsString(), user_info, enabled);
    AddSimpleField(card.Expiration4DigitYearAsString(), user_info, enabled);
  } else {
    AddSimpleField(std::u16string(), user_info, enabled);
    AddSimpleField(std::u16string(), user_info, enabled);
  }

  if (card.HasNameOnCard()) {
    AddSimpleField(card.GetRawInfo(CREDIT_CARD_NAME_FULL), user_info, enabled);
  } else {
    AddSimpleField(std::u16string(), user_info, enabled);
  }
  AddSimpleField(cvc, user_info, enabled);
}

UserInfo TranslateCard(const CreditCard* data, bool enabled) {
  DCHECK(data);

  UserInfo user_info(data->network(), GetCardArtUrl(*data));

  std::u16string obfuscated_number =
      data->CardIdentifierStringForManualFilling();
  // The `text_to_fill` field is set to an empty string as we're populating the
  // `id` of the `UserInfoField` which would be used to determine the type of
  // the card and fill the form accordingly.
  user_info.add_field(AccessorySheetField::Builder()
                          .SetDisplayText(obfuscated_number)
                          .SetId(data->guid())
                          .SetSelectable(enabled)
                          .Build());
  AddCardDetailsToUserInfo(*data, &user_info, std::u16string(), enabled);

  return user_info;
}

UserInfo TranslateCachedCard(const CachedServerCardInfo* data, bool enabled) {
  DCHECK(data);

  const CreditCard& card = data->card;
  UserInfo user_info(card.network(), GetCardArtUrl(card));
  std::u16string card_number = card.GetRawInfo(CREDIT_CARD_NUMBER);
  user_info.add_field(AccessorySheetField::Builder()
                          .SetDisplayText(card.FullDigitsForDisplay())
                          .SetTextToFill(card_number)
                          .SetA11yDescription(card_number)
                          .SetSelectable(enabled)
                          .Build());
  AddCardDetailsToUserInfo(card, &user_info, data->cvc, enabled);

  return user_info;
}

bool ShouldCreateVirtualCard(const CreditCard* card) {
  return card->virtual_card_enrollment_state() ==
         CreditCard::VirtualCardEnrollmentState::kEnrolled;
}

const CreditCard* UnwrapCardOrVirtualCard(
    const absl::variant<const CreditCard*, std::unique_ptr<CreditCard>>& card) {
  if (absl::holds_alternative<std::unique_ptr<CreditCard>>(card))
    return absl::get<std::unique_ptr<CreditCard>>(card).get();
  DCHECK(absl::holds_alternative<const CreditCard*>(card));
  return absl::get<const CreditCard*>(card);
}

PromoCodeInfo TranslateOffer(const AutofillOfferData* data) {
  DCHECK(data);
  DCHECK(data->IsPromoCodeOffer());

  std::u16string promo_code = base::ASCIIToUTF16(data->GetPromoCode());
  std::u16string details_text =
      base::ASCIIToUTF16(data->GetDisplayStrings().value_prop_text);
  PromoCodeInfo promo_code_info(promo_code, details_text);

  return promo_code_info;
}

IbanInfo TranslateIban(const Iban& data) {
  bool is_local = data.record_type() == Iban::kLocalIban;
  std::string id_string;
  if (!is_local) {
    id_string = base::NumberToString(data.instrument_id());
  }
  IbanInfo iban_info(data.GetIdentifierStringForAutofillDisplay(),
                     is_local ? data.value() : std::u16string(), id_string);

  return iban_info;
}

}  // namespace

PaymentMethodAccessoryControllerImpl::~PaymentMethodAccessoryControllerImpl() {
  if (personal_data_manager_)
    personal_data_manager_->RemoveObserver(this);
}

void PaymentMethodAccessoryControllerImpl::RegisterFillingSourceObserver(
    FillingSourceObserver observer) {
  source_observer_ = std::move(observer);
}

std::optional<AccessorySheetData>
PaymentMethodAccessoryControllerImpl::GetSheetData() const {
  // Note that also GetAutofillManager() can return nullptr.
  const BrowserAutofillManager* autofill_manager =
      GetWebContents().GetFocusedFrame() ? GetAutofillManager() : nullptr;

  std::vector<UserInfo> info_to_add;
  bool allow_filling =
      autofill_manager &&
      ShouldAllowCreditCardFallbacks(autofill_manager->client(),
                                     autofill_manager->last_query_form());

  std::vector<const CachedServerCardInfo*> unmasked_cards =
      GetUnmaskedCreditCards();
  if (!unmasked_cards.empty()) {
    // Add the cached server cards first, so that they show up on the top of the
    // manual filling view.
    base::ranges::transform(unmasked_cards, std::back_inserter(info_to_add),
                            [allow_filling](const CachedServerCardInfo* data) {
                              return TranslateCachedCard(data, allow_filling);
                            });
  }
  // Only add cards that are not present in the cache. Otherwise, we might
  // show duplicates.
  bool add_all_cards = unmasked_cards.empty() || !autofill_manager;
  for (const CardOrVirtualCard& card_or_virtual : GetAllCreditCards()) {
    const CreditCard* card = UnwrapCardOrVirtualCard(card_or_virtual);
    if (add_all_cards || !autofill_manager->GetCreditCardAccessManager()
                              .IsCardPresentInUnmaskedCache(*card)) {
      info_to_add.push_back(TranslateCard(card, allow_filling));
    }
  }

  const std::vector<FooterCommand> footer_commands = {FooterCommand(
      l10n_util::GetStringUTF16(
          IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_ALL_ADDRESSES_LINK),
      AccessoryAction::MANAGE_CREDIT_CARDS)};

  bool has_suggestions = !info_to_add.empty();

  AccessorySheetData data = CreateAccessorySheetData(
      AccessoryTabType::CREDIT_CARDS, GetTitle(has_suggestions),
      /*plusAddressTitle=*/std::u16string(), std::move(info_to_add),
      std::move(footer_commands));

  for (auto* offer : GetPromoCodeOffers()) {
    data.add_promo_code_info(TranslateOffer(offer));
  }

  for (const Iban& iban : GetIbans()) {
    data.add_iban_info(TranslateIban(iban));
  }

  if (has_suggestions && !allow_filling && autofill_manager) {
    data.set_warning(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_INSECURE_CONNECTION));
  }
  return data;
}

void PaymentMethodAccessoryControllerImpl::OnFillingTriggered(
    FieldGlobalId focused_field_id,
    const AccessorySheetField& selection) {
  content::RenderFrameHost* rfh = GetWebContents().GetFocusedFrame();
  if (!rfh)
    return;  // Without focused frame, driver and manager will be undefined.
  if (!GetDriver() || !GetAutofillManager()) {
    // Even with a valid frame, driver or manager might be invalid. Log these
    // cases to check how we can recover and fail gracefully so users can retry.
    base::debug::DumpWithoutCrashing();
    return;
  }

  // IBAN field or Credit card number fields have a GUID populated to allow
  // deobfuscation before filling.
  if (selection.id().empty()) {
    GetDriver()->ApplyFieldAction(mojom::FieldActionType::kReplaceAll,
                                  mojom::ActionPersistence::kFill,
                                  focused_field_id, selection.text_to_fill());
    return;
  }

  last_focused_field_id_ = focused_field_id;
  if (FetchIfCreditCardId(selection.id())) {
    return;
  }
  if (FetchIfIban(selection.id())) {
    return;
  }

  NOTREACHED() << "Neither fillable value nor known ID.";
}

void PaymentMethodAccessoryControllerImpl::OnPasskeySelected(
    const std::vector<uint8_t>& passkey_id) {
  NOTIMPLEMENTED()
      << "Passkey support not available in credit card controller.";
}

void PaymentMethodAccessoryControllerImpl::OnOptionSelected(
    AccessoryAction selected_action) {
  if (selected_action == AccessoryAction::MANAGE_CREDIT_CARDS) {
    ShowAutofillCreditCardSettings(&GetWebContents());
    return;
  }
  NOTREACHED_IN_MIGRATION()
      << "Unhandled selected action: " << static_cast<int>(selected_action);
}

void PaymentMethodAccessoryControllerImpl::OnToggleChanged(
    AccessoryAction toggled_action,
    bool enabled) {
  NOTREACHED_IN_MIGRATION()
      << "Unhandled toggled action: " << static_cast<int>(toggled_action);
}

// static
PaymentMethodAccessoryController* PaymentMethodAccessoryController::GetOrCreate(
    content::WebContents* web_contents) {
  PaymentMethodAccessoryControllerImpl::CreateForWebContents(web_contents);
  return PaymentMethodAccessoryControllerImpl::FromWebContents(web_contents);
}

// static
PaymentMethodAccessoryController* PaymentMethodAccessoryController::GetIfExisting(
    content::WebContents* web_contents) {
  return PaymentMethodAccessoryControllerImpl::FromWebContents(web_contents);
}

void PaymentMethodAccessoryControllerImpl::RefreshSuggestions() {
  TRACE_EVENT0("passwords",
               "PaymentMethodAccessoryControllerImpl::RefreshSuggestions");
  CHECK(source_observer_);
  source_observer_.Run(this,
                       IsFillingSourceAvailable(!GetAllCreditCards().empty() ||
                                                !GetPromoCodeOffers().empty() ||
                                                !GetIbans().empty()));
}

base::WeakPtr<PaymentMethodAccessoryController>
PaymentMethodAccessoryControllerImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PaymentMethodAccessoryControllerImpl::OnPersonalDataChanged() {
  RefreshSuggestions();
}

void PaymentMethodAccessoryControllerImpl::OnCreditCardFetched(
    CreditCardFetchResult result,
    const CreditCard* credit_card) {
  if (result != CreditCardFetchResult::kSuccess)
    return;
  DCHECK(credit_card);

  ApplyToField(credit_card->number());
}

void PaymentMethodAccessoryControllerImpl::ApplyToField(
    const std::u16string& value) {
  content::RenderFrameHost* rfh = GetWebContents().GetFocusedFrame();
  if (!rfh || !last_focused_field_id_ ||
      last_focused_field_id_.frame_token !=
          LocalFrameToken(rfh->GetFrameToken().value())) {
    last_focused_field_id_ = {};
    return;  // If frame isn't focused anymore, don't attempt to fill.
  }

  DCHECK(GetDriver());

  GetDriver()->ApplyFieldAction(mojom::FieldActionType::kReplaceAll,
                                mojom::ActionPersistence::kFill,
                                last_focused_field_id_, value);
  last_focused_field_id_ = {};
}

// static
void PaymentMethodAccessoryControllerImpl::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    base::WeakPtr<ManualFillingController> mf_controller,
    PersonalDataManager* personal_data_manager,
    BrowserAutofillManager* af_manager,
    AutofillDriver* af_driver) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  DCHECK(mf_controller);

  web_contents->SetUserData(
      UserDataKey(), base::WrapUnique(new PaymentMethodAccessoryControllerImpl(
                         web_contents, std::move(mf_controller),
                         personal_data_manager, af_manager, af_driver)));
}

PaymentMethodAccessoryControllerImpl::PaymentMethodAccessoryControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsUserData<PaymentMethodAccessoryControllerImpl>(
          *web_contents),
      personal_data_manager_(PersonalDataManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext())) {
  if (personal_data_manager_)
    personal_data_manager_->AddObserver(this);
}

PaymentMethodAccessoryControllerImpl::PaymentMethodAccessoryControllerImpl(
    content::WebContents* web_contents,
    base::WeakPtr<ManualFillingController> mf_controller,
    PersonalDataManager* personal_data_manager,
    BrowserAutofillManager* af_manager,
    AutofillDriver* af_driver)
    : content::WebContentsUserData<PaymentMethodAccessoryControllerImpl>(
          *web_contents),
      mf_controller_(mf_controller),
      personal_data_manager_(personal_data_manager),
      af_manager_for_testing_(af_manager),
      af_driver_for_testing_(af_driver) {
  if (personal_data_manager_)
    personal_data_manager_->AddObserver(this);
}

std::vector<PaymentMethodAccessoryControllerImpl::CardOrVirtualCard>
PaymentMethodAccessoryControllerImpl::GetAllCreditCards() const {
  if (!GetWebContents().GetFocusedFrame() || !personal_data_manager_)
    return std::vector<CardOrVirtualCard>();

  std::vector<CardOrVirtualCard> cards;
  for (const CreditCard* card : personal_data_manager_->payments_data_manager()
                                    .GetCreditCardsToSuggest()) {
    // If any of cards is enrolled for virtual cards and the feature is active,
    // then insert a virtual card suggestion right before the actual card.
    if (ShouldCreateVirtualCard(card)) {
      cards.push_back(CreditCard::CreateVirtualCardWithGuidSuffix(*card));
    }
    cards.push_back(card);
  }
  return cards;
}

std::vector<const CachedServerCardInfo*>
PaymentMethodAccessoryControllerImpl::GetUnmaskedCreditCards() const {
  if (!GetWebContents().GetFocusedFrame())
    return std::vector<const CachedServerCardInfo*>();
  const BrowserAutofillManager* autofill_manager = GetAutofillManager();
  if (!autofill_manager) {
    return std::vector<const CachedServerCardInfo*>();
  }
  std::vector<const CachedServerCardInfo*> unmasked_cards =
      autofill_manager->GetCreditCardAccessManager().GetCachedUnmaskedCards();
  // Show unmasked virtual cards in the manual filling view if they exist. All
  // other cards are dropped.
  auto not_virtual_card = [](const CachedServerCardInfo* card_info) {
    return card_info->card.record_type() !=
           CreditCard::RecordType::kVirtualCard;
  };
  std::erase_if(unmasked_cards, not_virtual_card);
  return unmasked_cards;
}

std::vector<const AutofillOfferData*>
PaymentMethodAccessoryControllerImpl::GetPromoCodeOffers() const {
  const AutofillManager* autofill_manager =
      GetWebContents().GetFocusedFrame() ? GetAutofillManager() : nullptr;
  if (!personal_data_manager_ || !autofill_manager)
    return std::vector<const AutofillOfferData*>();

  return personal_data_manager_->payments_data_manager()
      .GetActiveAutofillPromoCodeOffersForOrigin(
          autofill_manager->client()
              .GetLastCommittedPrimaryMainFrameURL()
              .DeprecatedGetOriginAsURL());
}

std::vector<Iban> PaymentMethodAccessoryControllerImpl::GetIbans() const {
  const AutofillManager* autofill_manager =
      GetWebContents().GetFocusedFrame() ? GetAutofillManager() : nullptr;
  if (!personal_data_manager_ || !autofill_manager) {
    return std::vector<Iban>();
  }

  return personal_data_manager_->payments_data_manager()
      .GetOrderedIbansToSuggest();
}

base::WeakPtr<ManualFillingController>
PaymentMethodAccessoryControllerImpl::GetManualFillingController() {
  if (!mf_controller_)
    mf_controller_ = ManualFillingController::GetOrCreate(&GetWebContents());
  DCHECK(mf_controller_);
  return mf_controller_;
}

AutofillDriver* PaymentMethodAccessoryControllerImpl::GetDriver() {
  DCHECK(GetWebContents().GetFocusedFrame());
  return af_driver_for_testing_ ? af_driver_for_testing_.get()
                                : ContentAutofillDriver::GetForRenderFrameHost(
                                      GetWebContents().GetFocusedFrame());
}

const BrowserAutofillManager*
PaymentMethodAccessoryControllerImpl::GetAutofillManager() const {
  return const_cast<PaymentMethodAccessoryControllerImpl*>(this)
      ->GetAutofillManager();
}

BrowserAutofillManager*
PaymentMethodAccessoryControllerImpl::GetAutofillManager() {
  DCHECK(GetWebContents().GetFocusedFrame());
  if (af_manager_for_testing_)
    return af_manager_for_testing_;
  ContentAutofillDriver* driver = ContentAutofillDriver::GetForRenderFrameHost(
      GetWebContents().GetFocusedFrame());
  // This cast is always safe in Chrome - only WebView has a different
  // AutofillManager implementation.
  return driver ? static_cast<BrowserAutofillManager*>(
                      &driver->GetAutofillManager())
                : nullptr;
}

content::WebContents& PaymentMethodAccessoryControllerImpl::GetWebContents()
    const {
  // While a const_cast is not ideal. The Autofill API uses const in various
  // spots and the content public API doesn't have const accessors. So the const
  // cast is the lesser of two evils.
  return const_cast<content::WebContents&>(
      content::WebContentsUserData<
          PaymentMethodAccessoryControllerImpl>::GetWebContents());
}

bool PaymentMethodAccessoryControllerImpl::FetchIfCreditCardId(
    const std::string& selection_id) {
  std::vector<CardOrVirtualCard> cards = GetAllCreditCards();
  auto card_iter = base::ranges::find_if(
      cards, [&selection_id](const auto& card_or_virtual) {
        const CreditCard* card = UnwrapCardOrVirtualCard(card_or_virtual);
        return card && card->guid() == selection_id;
      });

  if (card_iter == cards.end()) {
    return false;
  }

  GetAutofillManager()->GetCreditCardAccessManager().FetchCreditCard(
      UnwrapCardOrVirtualCard(*card_iter),
      base::BindOnce(&PaymentMethodAccessoryControllerImpl::OnCreditCardFetched,
                     weak_ptr_factory_.GetWeakPtr()));
  return true;
}

bool PaymentMethodAccessoryControllerImpl::FetchIfIban(
    const std::string& selection_id) {
  std::vector<Iban> ibans = GetIbans();
  auto iban_iter =
      base::ranges::find_if(ibans, [&selection_id](const Iban& available_iban) {
        return available_iban.record_type() == Iban::kServerIban &&
               base::NumberToString(available_iban.instrument_id()) ==
                   selection_id;
      });

  if (iban_iter == ibans.end()) {
    return false;
  }

  Suggestion::BackendId backend_id = Suggestion::BackendId(
      Suggestion::InstrumentId((*iban_iter).instrument_id()));
  GetAutofillManager()
      ->client()
      .GetPaymentsAutofillClient()
      ->GetIbanAccessManager()
      ->FetchValue(
          backend_id,
          base::BindOnce(&PaymentMethodAccessoryControllerImpl::ApplyToField,
                         weak_ptr_factory_.GetWeakPtr()));
  return true;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PaymentMethodAccessoryControllerImpl);

}  // namespace autofill

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/credit_card_accessory_controller_impl.h"

#include <iterator>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/preferences/autofill/autofill_profile_bridge.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/autofill/manual_filling_utils.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

// Return the card art url to displayed in the autofill suggestions. The card
// art is only supported for virtual cards. For other cards, we show the default
// network icon.
GURL GetCardArtUrl(const CreditCard& card) {
  return card.record_type() == CreditCard::VIRTUAL_CARD ? card.card_art_url()
                                                        : GURL();
}

std::u16string GetTitle(bool has_suggestions) {
  return l10n_util::GetStringUTF16(
      has_suggestions ? IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_TITLE
                      : IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_EMPTY_MESSAGE);
}

void AddSimpleField(const std::u16string& data,
                    UserInfo* user_info,
                    bool enabled) {
  user_info->add_field(AccessorySheetField(
      /*display_text=*/data, /*text_to_fill=*/data, /*a11y_description=*/data,
      /*id=*/std::string(),
      /*is_password=*/false, enabled));
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
  user_info.add_field(AccessorySheetField(
      obfuscated_number, /*text_to_fill=*/std::u16string(), obfuscated_number,
      data->guid(), /*is_password=*/false, enabled));
  AddCardDetailsToUserInfo(*data, &user_info, std::u16string(), enabled);

  return user_info;
}

UserInfo TranslateCachedCard(const CachedServerCardInfo* data, bool enabled) {
  DCHECK(data);

  const CreditCard& card = data->card;
  UserInfo user_info(card.network(), GetCardArtUrl(card));
  std::u16string card_number = card.GetRawInfo(CREDIT_CARD_NUMBER);
  user_info.add_field(AccessorySheetField(
      card.FullDigitsForDisplay(), card_number, card_number,
      /*id=*/std::string(), /*is_password=*/false, enabled));
  AddCardDetailsToUserInfo(card, &user_info, data->cvc, enabled);

  return user_info;
}

bool ShouldCreateVirtualCard(const CreditCard* card) {
  return card->virtual_card_enrollment_state() == CreditCard::ENROLLED;
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

}  // namespace

CreditCardAccessoryControllerImpl::~CreditCardAccessoryControllerImpl() {
  if (personal_data_manager_)
    personal_data_manager_->RemoveObserver(this);
}

void CreditCardAccessoryControllerImpl::RegisterFillingSourceObserver(
    FillingSourceObserver observer) {
  source_observer_ = std::move(observer);
}

absl::optional<AccessorySheetData>
CreditCardAccessoryControllerImpl::GetSheetData() const {
  // Note that also GetManager() can return nullptr.
  AutofillManager* autofill_manager =
      GetWebContents().GetFocusedFrame() ? GetManager() : nullptr;
  // This cast is safe because the Chrome embedder only uses
  // BrowserAutofillManager.
  auto* browser_autofill_manager =
      static_cast<BrowserAutofillManager*>(autofill_manager);

  std::vector<UserInfo> info_to_add;
  bool allow_filling =
      autofill_manager && ShouldAllowCreditCardFallbacks(
                              autofill_manager->client(),
                              browser_autofill_manager->last_query_form());

  std::vector<const CachedServerCardInfo*> unmasked_cards =
      GetUnmaskedCreditCards();
  if (!unmasked_cards.empty()) {
    // Add the cached server cards first, so that they show up on the top of the
    // manual filling view.
    std::transform(unmasked_cards.begin(), unmasked_cards.end(),
                   std::back_inserter(info_to_add),
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
                              ->IsCardPresentInUnmaskedCache(*card)) {
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
      std::move(info_to_add), std::move(footer_commands));

  if (base::FeatureList::IsEnabled(
          features::kAutofillFillMerchantPromoCodeFields)) {
    for (auto* offer : GetPromoCodeOffers()) {
      data.add_promo_code_info(TranslateOffer(offer));
    }
  }

  if (has_suggestions && !allow_filling && autofill_manager) {
    data.set_warning(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_INSECURE_CONNECTION));
  }
  return data;
}

void CreditCardAccessoryControllerImpl::OnFillingTriggered(
    FieldGlobalId focused_field_id,
    const AccessorySheetField& selection) {
  content::RenderFrameHost* rfh = GetWebContents().GetFocusedFrame();
  if (!rfh)
    return;  // Without focused frame, driver and manager will be undefined.
  if (!GetDriver() || !GetManager()) {
    // Even with a valid frame, driver or manager might be invalid. Log these
    // cases to check how we can recover and fail gracefully so users can retry.
    base::debug::DumpWithoutCrashing();
    return;
  }

  // Credit card number fields have a GUID populated to allow deobfuscation
  // before filling.
  if (selection.id().empty()) {
    GetDriver()->RendererShouldFillFieldWithValue(focused_field_id,
                                                  selection.text_to_fill());
    return;
  }

  std::vector<CardOrVirtualCard> cards = GetAllCreditCards();
  auto card_iter =
      base::ranges::find_if(cards, [&selection](const auto& card_or_virtual) {
        const CreditCard* card = UnwrapCardOrVirtualCard(card_or_virtual);
        return card && card->guid() == selection.id();
      });

  if (card_iter == cards.end()) {
    NOTREACHED() << "Tried to fill card with unknown GUID";
    return;
  }

  const CreditCard* matching_card = UnwrapCardOrVirtualCard(*card_iter);
  switch (matching_card->record_type()) {
    case CreditCard::RecordType::MASKED_SERVER_CARD:
    case CreditCard::RecordType::VIRTUAL_CARD:
      last_focused_field_id_ = focused_field_id;
      GetManager()->GetCreditCardAccessManager()->FetchCreditCard(matching_card,
                                                                  AsWeakPtr());
      break;
    case CreditCard::RecordType::LOCAL_CARD:
    case CreditCard::RecordType::FULL_SERVER_CARD:
      GetDriver()->RendererShouldFillFieldWithValue(focused_field_id,
                                                    matching_card->number());
      break;
  }
}

void CreditCardAccessoryControllerImpl::OnOptionSelected(
    AccessoryAction selected_action) {
  if (selected_action == AccessoryAction::MANAGE_CREDIT_CARDS) {
    ShowAutofillCreditCardSettings(&GetWebContents());
    return;
  }
  NOTREACHED() << "Unhandled selected action: "
               << static_cast<int>(selected_action);
}

void CreditCardAccessoryControllerImpl::OnToggleChanged(
    AccessoryAction toggled_action,
    bool enabled) {
  NOTREACHED() << "Unhandled toggled action: "
               << static_cast<int>(toggled_action);
}

// static
bool CreditCardAccessoryController::AllowedForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  if (vr::VrTabHelper::IsInVr(web_contents)) {
    return false;  // TODO(crbug.com/902305): Re-enable if possible.
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableManualFallbackForVirtualCards)) {
    PersonalDataManager* personal_data_manager =
        PersonalDataManagerFactory::GetForBrowserContext(
            web_contents->GetBrowserContext());
    if (personal_data_manager) {
      std::vector<CreditCard*> cards =
          personal_data_manager->GetCreditCardsToSuggest(
              /*include_server_cards=*/true);
      bool has_virtual_card = base::ranges::any_of(cards, [](const auto& card) {
        return card->virtual_card_enrollment_state() ==
               CreditCard::VirtualCardEnrollmentState::ENROLLED;
      });
      if (has_virtual_card) {
        // Virtual cards are available. We should always show manual fallback
        // for virtual cards.
        return true;
      }
    }
  }

  // For non-virtual cards show the credit card accessory sheet only
  // when both keyboard accessory and manual fallback flags are enabled.
  return features::IsAutofillManualFallbackEnabled();
}

// static
CreditCardAccessoryController* CreditCardAccessoryController::GetOrCreate(
    content::WebContents* web_contents) {
  DCHECK(CreditCardAccessoryController::AllowedForWebContents(web_contents));

  CreditCardAccessoryControllerImpl::CreateForWebContents(web_contents);
  return CreditCardAccessoryControllerImpl::FromWebContents(web_contents);
}

// static
CreditCardAccessoryController* CreditCardAccessoryController::GetIfExisting(
    content::WebContents* web_contents) {
  return CreditCardAccessoryControllerImpl::FromWebContents(web_contents);
}

void CreditCardAccessoryControllerImpl::RefreshSuggestions() {
  TRACE_EVENT0("passwords",
               "CreditCardAccessoryControllerImpl::RefreshSuggestions");
  if (source_observer_) {
    source_observer_.Run(
        this, IsFillingSourceAvailable(!GetAllCreditCards().empty() ||
                                       !GetPromoCodeOffers().empty()));
  } else {
    // TODO(crbug.com/1169167): Remove once filling controller pulls this
    // information instead of waiting to get it pushed.
    absl::optional<AccessorySheetData> data = GetSheetData();
    DCHECK(data.has_value());
    GetManualFillingController()->RefreshSuggestions(std::move(data.value()));
  }
}

void CreditCardAccessoryControllerImpl::OnPersonalDataChanged() {
  RefreshSuggestions();
}

void CreditCardAccessoryControllerImpl::OnCreditCardFetched(
    CreditCardFetchResult result,
    const CreditCard* credit_card,
    const std::u16string& cvc) {
  if (result != CreditCardFetchResult::kSuccess)
    return;
  content::RenderFrameHost* rfh = GetWebContents().GetFocusedFrame();
  if (!rfh || !last_focused_field_id_ ||
      last_focused_field_id_.frame_token !=
          LocalFrameToken(rfh->GetFrameToken().value())) {
    last_focused_field_id_ = {};
    return;  // If frame isn't focused anymore, don't attempt to fill.
  }
  DCHECK(credit_card);
  DCHECK(GetDriver());

  GetDriver()->RendererShouldFillFieldWithValue(last_focused_field_id_,
                                                credit_card->number());
  last_focused_field_id_ = {};
}

// static
void CreditCardAccessoryControllerImpl::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    base::WeakPtr<ManualFillingController> mf_controller,
    PersonalDataManager* personal_data_manager,
    BrowserAutofillManager* af_manager,
    AutofillDriver* af_driver) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  DCHECK(mf_controller);

  web_contents->SetUserData(
      UserDataKey(), base::WrapUnique(new CreditCardAccessoryControllerImpl(
                         web_contents, std::move(mf_controller),
                         personal_data_manager, af_manager, af_driver)));
}

CreditCardAccessoryControllerImpl::CreditCardAccessoryControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsUserData<CreditCardAccessoryControllerImpl>(
          *web_contents),
      personal_data_manager_(PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
  if (personal_data_manager_)
    personal_data_manager_->AddObserver(this);
}

CreditCardAccessoryControllerImpl::CreditCardAccessoryControllerImpl(
    content::WebContents* web_contents,
    base::WeakPtr<ManualFillingController> mf_controller,
    PersonalDataManager* personal_data_manager,
    BrowserAutofillManager* af_manager,
    AutofillDriver* af_driver)
    : content::WebContentsUserData<CreditCardAccessoryControllerImpl>(
          *web_contents),
      mf_controller_(mf_controller),
      personal_data_manager_(personal_data_manager),
      af_manager_for_testing_(af_manager),
      af_driver_for_testing_(af_driver) {
  if (personal_data_manager_)
    personal_data_manager_->AddObserver(this);
}

std::vector<CreditCardAccessoryControllerImpl::CardOrVirtualCard>
CreditCardAccessoryControllerImpl::GetAllCreditCards() const {
  if (!GetWebContents().GetFocusedFrame() || !personal_data_manager_)
    return std::vector<CardOrVirtualCard>();

  std::vector<CardOrVirtualCard> cards;
  for (const CreditCard* card : personal_data_manager_->GetCreditCardsToSuggest(
           /*include_server_cards=*/true)) {
    // If any of cards is enrolled for virtual cards and the feature is active,
    // then insert a virtual card suggestion right before the actual card.
    if (ShouldCreateVirtualCard(card)) {
      cards.push_back(CreditCard::CreateVirtualCard(*card));
    }
    cards.push_back(card);
  }
  return cards;
}

std::vector<const CachedServerCardInfo*>
CreditCardAccessoryControllerImpl::GetUnmaskedCreditCards() const {
  if (!GetWebContents().GetFocusedFrame())
    return std::vector<const CachedServerCardInfo*>();
  AutofillManager* autofill_manager = GetManager();
  if (!autofill_manager || !autofill_manager->GetCreditCardAccessManager())
    return std::vector<const CachedServerCardInfo*>();
  std::vector<const CachedServerCardInfo*> unmasked_cards =
      autofill_manager->GetCreditCardAccessManager()->GetCachedUnmaskedCards();
  // If the feature to show unmasked cards in manual filling view is
  // enabled, show all cards in the view. Even if not, still show
  // virtual cards in the manual filling view if they exist. All other cards
  // are dropped.
  if (base::FeatureList::IsEnabled(
          features::kAutofillShowUnmaskedCachedCardInManualFillingView)) {
    return unmasked_cards;
  }
  auto not_virtual_card = [](const CachedServerCardInfo* card_info) {
    return card_info->card.record_type() != CreditCard::VIRTUAL_CARD;
  };
  base::EraseIf(unmasked_cards, not_virtual_card);
  return unmasked_cards;
}

std::vector<const AutofillOfferData*>
CreditCardAccessoryControllerImpl::GetPromoCodeOffers() const {
  AutofillManager* autofill_manager =
      GetWebContents().GetFocusedFrame() ? GetManager() : nullptr;
  if (!personal_data_manager_ || !autofill_manager)
    return std::vector<const AutofillOfferData*>();

  return personal_data_manager_->GetActiveAutofillPromoCodeOffersForOrigin(
      autofill_manager->client()
          ->GetLastCommittedPrimaryMainFrameURL()
          .DeprecatedGetOriginAsURL());
}

base::WeakPtr<ManualFillingController>
CreditCardAccessoryControllerImpl::GetManualFillingController() {
  if (!mf_controller_)
    mf_controller_ = ManualFillingController::GetOrCreate(&GetWebContents());
  DCHECK(mf_controller_);
  return mf_controller_;
}

AutofillDriver* CreditCardAccessoryControllerImpl::GetDriver() {
  DCHECK(GetWebContents().GetFocusedFrame());
  return af_driver_for_testing_ ? af_driver_for_testing_.get()
                                : ContentAutofillDriver::GetForRenderFrameHost(
                                      GetWebContents().GetFocusedFrame());
}

AutofillManager* CreditCardAccessoryControllerImpl::GetManager() const {
  DCHECK(GetWebContents().GetFocusedFrame());
  if (af_manager_for_testing_)
    return af_manager_for_testing_;
  ContentAutofillDriver* driver = ContentAutofillDriver::GetForRenderFrameHost(
      GetWebContents().GetFocusedFrame());
  return driver ? driver->autofill_manager() : nullptr;
}

content::WebContents& CreditCardAccessoryControllerImpl::GetWebContents()
    const {
  // While a const_cast is not ideal. The Autofill API uses const in various
  // spots and the content public API doesn't have const accessors. So the const
  // cast is the lesser of two evils.
  return const_cast<content::WebContents&>(
      content::WebContentsUserData<
          CreditCardAccessoryControllerImpl>::GetWebContents());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CreditCardAccessoryControllerImpl);

}  // namespace autofill

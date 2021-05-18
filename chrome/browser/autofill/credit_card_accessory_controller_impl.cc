// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/credit_card_accessory_controller_impl.h"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/preferences/autofill/autofill_profile_bridge.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/autofill/manual_filling_utils.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

std::u16string GetTitle(bool has_suggestions) {
  return l10n_util::GetStringUTF16(
      has_suggestions ? IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_TITLE
                      : IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_EMPTY_MESSAGE);
}

void AddSimpleField(const std::u16string& data,
                    UserInfo* user_info,
                    bool enabled) {
  user_info->add_field(UserInfo::Field(data, data,
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
    AddSimpleField(card.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL), user_info,
                   enabled);
  } else {
    AddSimpleField(std::u16string(), user_info, enabled);
  }
  AddSimpleField(cvc, user_info, enabled);
}

UserInfo TranslateCard(const CreditCard* data, bool enabled) {
  DCHECK(data);

  UserInfo user_info(data->network());

  std::u16string obfuscated_number = data->ObfuscatedLastFourDigits();
  user_info.add_field(UserInfo::Field(obfuscated_number, obfuscated_number,
                                      data->guid(), /*is_password=*/false,
                                      enabled));
  AddCardDetailsToUserInfo(*data, &user_info, std::u16string(), enabled);

  return user_info;
}

UserInfo TranslateCachedCard(const CachedServerCardInfo* data, bool enabled) {
  DCHECK(data);

  const CreditCard& card = data->card;
  UserInfo user_info(card.network());

  AddSimpleField(card.GetRawInfo(autofill::CREDIT_CARD_NUMBER), &user_info,
                 enabled);
  AddCardDetailsToUserInfo(card, &user_info, data->cvc, enabled);

  return user_info;
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

absl::optional<autofill::AccessorySheetData>
CreditCardAccessoryControllerImpl::GetSheetData() const {
  bool valid_manager = web_contents_->GetFocusedFrame() && GetManager();
  std::vector<UserInfo> info_to_add;
  bool allow_filling = valid_manager && ShouldAllowCreditCardFallbacks(
                                            GetManager()->client(),
                                            GetManager()->last_query_form());

  if (!cached_server_cards_.empty()) {
    // Add the cached server cards first, so that they show up on the top of the
    // manual filling view.
    std::transform(cached_server_cards_.begin(), cached_server_cards_.end(),
                   std::back_inserter(info_to_add),
                   [allow_filling](const CachedServerCardInfo* data) {
                     return TranslateCachedCard(data, allow_filling);
                   });
  }
  // Only add cards that are not present in the cache. Otherwise, we might
  // show duplicates.
  for (auto* card : cards_cache_) {
    if (cached_server_cards_.empty() ||
        !GetManager()
             ->credit_card_access_manager()
             ->IsCardPresentInUnmaskedCache(card->server_id())) {
      info_to_add.push_back(TranslateCard(card, allow_filling));
    }
  }

  const std::vector<FooterCommand> footer_commands = {FooterCommand(
      l10n_util::GetStringUTF16(
          IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_ALL_ADDRESSES_LINK),
      AccessoryAction::MANAGE_CREDIT_CARDS)};

  bool has_suggestions = !info_to_add.empty();

  AccessorySheetData data = autofill::CreateAccessorySheetData(
      AccessoryTabType::CREDIT_CARDS, GetTitle(has_suggestions),
      std::move(info_to_add), std::move(footer_commands));
  if (has_suggestions && !allow_filling && valid_manager) {
    data.set_warning(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_INSECURE_CONNECTION));
  }
  return data;
}

void CreditCardAccessoryControllerImpl::OnFillingTriggered(
    FieldGlobalId focused_field_id,
    const UserInfo::Field& selection) {
  content::RenderFrameHost* rfh = web_contents_->GetFocusedFrame();
  if (!rfh)
    return;  // Without focused frame, driver and manager will be undefined.
  DCHECK(GetDriver());

  // Credit card number fields have a GUID populated to allow deobfuscation
  // before filling.
  if (selection.id().empty()) {
    GetDriver()->RendererShouldFillFieldWithValue(focused_field_id,
                                                  selection.display_text());
    return;
  }

  auto card_iter = std::find_if(cards_cache_.begin(), cards_cache_.end(),
                                [&selection](const auto* card) {
                                  return card && card->guid() == selection.id();
                                });

  if (card_iter == cards_cache_.end()) {
    NOTREACHED() << "Tried to fill card with unknown GUID";
    return;
  }

  CreditCard* matching_card = *card_iter;
  if (matching_card->record_type() ==
      CreditCard::RecordType::MASKED_SERVER_CARD) {
    DCHECK(GetManager());
    last_focused_field_id_ = focused_field_id;
    GetManager()->credit_card_access_manager()->FetchCreditCard(matching_card,
                                                                AsWeakPtr());
  } else {
    GetDriver()->RendererShouldFillFieldWithValue(focused_field_id,
                                                  matching_card->number());
  }
}

void CreditCardAccessoryControllerImpl::OnOptionSelected(
    AccessoryAction selected_action) {
  if (selected_action == AccessoryAction::MANAGE_CREDIT_CARDS) {
    autofill::ShowAutofillCreditCardSettings(web_contents_);
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
  return base::FeatureList::IsEnabled(
             autofill::features::kAutofillKeyboardAccessory) &&
         base::FeatureList::IsEnabled(
             autofill::features::kAutofillManualFallbackAndroid);
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
  if (web_contents_->GetFocusedFrame() && GetManager()) {
    FetchSuggestionsFromPersonalDataManager();
  } else {
    cards_cache_.clear();  // If cards cannot be filled, don't show them.
    cached_server_cards_.clear();
  }
  absl::optional<AccessorySheetData> data = GetSheetData();
  if (source_observer_) {
    source_observer_.Run(this, IsFillingSourceAvailable(data.has_value()));
  } else {
    // TODO(crbug.com/1169167): Remove once filling controller pulls this
    // information instead of waiting to get it pushed.
    DCHECK(data.has_value());
    GetManualFillingController()->RefreshSuggestions(std::move(data.value()));
  }
}

void CreditCardAccessoryControllerImpl::OnPersonalDataChanged() {
  RefreshSuggestions();
}

void CreditCardAccessoryControllerImpl::OnCreditCardFetched(
    bool did_succeed,
    const CreditCard* credit_card,
    const std::u16string& cvc) {
  if (!did_succeed)
    return;
  content::RenderFrameHost* rfh = web_contents_->GetFocusedFrame();
  if (!rfh || !last_focused_field_id_ ||
      last_focused_field_id_.frame_token !=
          autofill::LocalFrameToken(rfh->GetFrameToken().value())) {
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
    autofill::PersonalDataManager* personal_data_manager,
    autofill::BrowserAutofillManager* af_manager,
    autofill::AutofillDriver* af_driver) {
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
    : web_contents_(web_contents),
      personal_data_manager_(
          autofill::PersonalDataManagerFactory::GetForProfile(
              Profile::FromBrowserContext(
                  web_contents_->GetBrowserContext()))) {
  if (personal_data_manager_)
    personal_data_manager_->AddObserver(this);
}

CreditCardAccessoryControllerImpl::CreditCardAccessoryControllerImpl(
    content::WebContents* web_contents,
    base::WeakPtr<ManualFillingController> mf_controller,
    PersonalDataManager* personal_data_manager,
    autofill::BrowserAutofillManager* af_manager,
    autofill::AutofillDriver* af_driver)
    : web_contents_(web_contents),
      mf_controller_(mf_controller),
      personal_data_manager_(personal_data_manager),
      af_manager_for_testing_(af_manager),
      af_driver_for_testing_(af_driver) {
  if (personal_data_manager_)
    personal_data_manager_->AddObserver(this);
}

void CreditCardAccessoryControllerImpl::
    FetchSuggestionsFromPersonalDataManager() {
  if (!personal_data_manager_) {
    cards_cache_.clear();  // No data available.
  } else {
    cards_cache_ = personal_data_manager_->GetCreditCardsToSuggest(
        /*include_server_cards=*/true);
  }
  // TODO(crbug.com/1196021) Update the below logic to allow for showing only
  // virtual cards if the kAutofillShowUnmaskedCachedCardInManualFillingView is
  // disabled.
  if (GetManager() && GetManager()->credit_card_access_manager() &&
      base::FeatureList::IsEnabled(
          autofill::features::
              kAutofillShowUnmaskedCachedCardInManualFillingView)) {
    cached_server_cards_ =
        GetManager()->credit_card_access_manager()->GetCachedUnmaskedCards();
  } else {
    cached_server_cards_.clear();  // No data available.
  }
}

base::WeakPtr<ManualFillingController>
CreditCardAccessoryControllerImpl::GetManualFillingController() {
  if (!mf_controller_)
    mf_controller_ = ManualFillingController::GetOrCreate(web_contents_);
  DCHECK(mf_controller_);
  return mf_controller_;
}

autofill::AutofillDriver* CreditCardAccessoryControllerImpl::GetDriver() {
  DCHECK(web_contents_->GetFocusedFrame());
  return af_driver_for_testing_
             ? af_driver_for_testing_
             : autofill::ContentAutofillDriver::GetForRenderFrameHost(
                   web_contents_->GetFocusedFrame());
}

autofill::BrowserAutofillManager*
CreditCardAccessoryControllerImpl::GetManager() const {
  DCHECK(web_contents_->GetFocusedFrame());
  return af_manager_for_testing_
             ? af_manager_for_testing_
             : autofill::ContentAutofillDriver::GetForRenderFrameHost(
                   web_contents_->GetFocusedFrame())
                   ->browser_autofill_manager();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CreditCardAccessoryControllerImpl)

}  // namespace autofill

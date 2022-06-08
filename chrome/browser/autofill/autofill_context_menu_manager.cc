// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_context_menu_manager.h"

#include <string>

#include "chrome/app/chrome_command_ids.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

namespace {

// The range of command IDs reserved for autofill's custom menus.
static constexpr int kAutofillContextCustomFirst =
    IDC_CONTENT_CONTEXT_AUTOFILL_CUSTOM_FIRST;
static constexpr int kAutofillContextCustomLast =
    IDC_CONTENT_CONTEXT_AUTOFILL_CUSTOM_LAST;

// Field types that are supposed to be shown in the menu. |UNKNOWN_TYPE|
// denotes a separator needs to be added.
static constexpr std::array kAddressFieldTypesToShow = {
    NAME_FULL,
    UNKNOWN_TYPE,
    ADDRESS_HOME_STREET_ADDRESS,
    ADDRESS_HOME_CITY,
    ADDRESS_HOME_ZIP,
    UNKNOWN_TYPE,
    PHONE_HOME_WHOLE_NUMBER,
    EMAIL_ADDRESS};

// Field types that are supposed to be shown in the menu. |UNKNOWN_TYPE|
// denotes a separator needs to be added.
static constexpr std::array kCardFieldTypesToShow = {
    CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER, UNKNOWN_TYPE,
    CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR};

}  // namespace

// static
AutofillContextMenuManager::CommandId
AutofillContextMenuManager::ConvertToAutofillCustomCommandId(int offset) {
  return AutofillContextMenuManager::CommandId(kAutofillContextCustomFirst +
                                               offset);
}

// static
bool AutofillContextMenuManager::IsAutofillCustomCommandId(
    CommandId command_id) {
  return command_id.value() >= kAutofillContextCustomFirst &&
         command_id.value() <= kAutofillContextCustomLast;
}

AutofillContextMenuManager::AutofillContextMenuManager(
    PersonalDataManager* personal_data_manager,
    ui::SimpleMenuModel::Delegate* delegate,
    ui::SimpleMenuModel* menu_model)
    : personal_data_manager_(personal_data_manager),
      menu_model_(menu_model),
      delegate_(delegate) {}

AutofillContextMenuManager::~AutofillContextMenuManager() {
  cached_menu_models_.clear();
}

void AutofillContextMenuManager::AppendItems() {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillShowManualFallbackInContextMenu)) {
    return;
  }

  DCHECK(personal_data_manager_);
  DCHECK(menu_model_);

  AppendAddressItems();
  AppendCreditCardItems();
}

bool AutofillContextMenuManager::IsCommandIdChecked(
    CommandId command_id) const {
  return false;
}

bool AutofillContextMenuManager::IsCommandIdVisible(
    CommandId command_id) const {
  return true;
}

bool AutofillContextMenuManager::IsCommandIdEnabled(
    CommandId command_id) const {
  return true;
}

void AutofillContextMenuManager::ExecuteCommand(
    CommandId command_id,
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host) {
  // TODO(crbug.com/1325811): Implement.
}

void AutofillContextMenuManager::AppendAddressItems() {
  std::vector<AutofillProfile*> address_profiles =
      personal_data_manager_->GetProfiles();
  if (address_profiles.empty()) {
    return;
  }

  // Used to create menu model for storing address description. Would be
  // attached to the top level "Fill Address Info" item in the context menu.
  ui::SimpleMenuModel* profile_menu = new ui::SimpleMenuModel(delegate_);
  cached_menu_models_.push_back(base::WrapUnique(profile_menu));

  // True if a row is added in the address section.
  bool address_added = false;

  for (const AutofillProfile* profile : address_profiles) {
    // Creates a menu model for storing address details. Would be attached
    // to the address description menu item.
    ui::SimpleMenuModel* address_details_submenu =
        new ui::SimpleMenuModel(delegate_);
    cached_menu_models_.push_back(base::WrapUnique(address_details_submenu));

    // Create a submenu for each address profile with their details.
    CreateSubMenuWithData(profile, kAddressFieldTypesToShow,
                          address_details_submenu);

    // Add a menu item showing address profile description. Hovering over it
    // opens a submenu with the address details.
    absl::optional<CommandId> profile_menu_id =
        GetNextAvailableAutofillCommandId();
    if (profile_menu_id) {
      address_added = true;
      profile_menu->AddSubMenu(profile_menu_id->value(),
                               GetProfileDescription(*profile),
                               address_details_submenu);
    }
  }

  // Add a menu option to suggest filling address in the context menu.
  // Hovering over it opens a submenu suggesting all the address profiles
  // stored in the profile.
  absl::optional<CommandId> address_menu_id =
      GetNextAvailableAutofillCommandId();
  if (address_menu_id && address_added) {
    // TODO(crbug.com/1325811): Use i18n string.
    menu_model_->AddSubMenu(address_menu_id->value(), u"Fill Address Info",
                            profile_menu);
  }
}

void AutofillContextMenuManager::AppendCreditCardItems() {
  std::vector<CreditCard*> cards = personal_data_manager_->GetCreditCards();
  if (cards.empty()) {
    return;
  }

  // Used to create menu model for storing credit card description. Would be
  // attached to the top level "Fill Payment" item in the context menu.
  ui::SimpleMenuModel* card_submenu = new ui::SimpleMenuModel(delegate_);
  cached_menu_models_.push_back(base::WrapUnique(card_submenu));

  // True if a row is added in the credit card section.
  bool card_added = false;

  for (const CreditCard* card : cards) {
    // Creates a menu model for storing credit card details. Would be attached
    // to the credit card description menu item.
    ui::SimpleMenuModel* card_details_submenu =
        new ui::SimpleMenuModel(delegate_);
    cached_menu_models_.push_back(base::WrapUnique(card_details_submenu));

    // Create a submenu for each credit card with their details.
    CreateSubMenuWithData(card, kCardFieldTypesToShow, card_details_submenu);

    // Add a menu item showing credit card  description. Hovering over it
    // opens a submenu with the credit card details.
    absl::optional<CommandId> submenu_id = GetNextAvailableAutofillCommandId();
    if (submenu_id) {
      card_added = true;
      card_submenu->AddSubMenu(submenu_id->value(),
                               card->CardIdentifierStringForAutofillDisplay(),
                               card_details_submenu);
    }
  }

  // Add a menu option to suggest filling a credit card in the context menu.
  // Hovering over it opens a submenu suggesting all the credit cards
  // stored in the profile.
  absl::optional<CommandId> card_submenu_id =
      GetNextAvailableAutofillCommandId();
  if (card_submenu_id && card_added) {
    // TODO(crbug.com/1325811): Use i18n string.
    menu_model_->AddSubMenu(card_submenu_id->value(), u"Fill Payment",
                            card_submenu);
  }
}

void AutofillContextMenuManager::CreateSubMenuWithData(
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card,
    base::span<const ServerFieldType> field_types_to_show,
    ui::SimpleMenuModel* menu_model) {
  // True when an item is added to the context menu which is not a separator.
  bool is_separator_required = false;

  // True when a `UNKNOWN_TYPE` is seen in `field_types_to_show`.
  bool was_prev_unknown = false;

  for (const ServerFieldType type : field_types_to_show) {
    if (type == UNKNOWN_TYPE) {
      was_prev_unknown = true;
      continue;
    }

    std::u16string value = absl::visit(
        [type](const auto& alternative) {
          return alternative->GetRawInfo(type);
        },
        profile_or_credit_card);

    if (!value.empty()) {
      absl::optional<CommandId> value_menu_id =
          GetNextAvailableAutofillCommandId();
      if (value_menu_id) {
        if (was_prev_unknown && is_separator_required) {
          menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
          is_separator_required = false;
        }

        // Create a menu item with the address/credit card details and attach
        // to the model.
        menu_model->AddItem(value_menu_id->value(), value);
        was_prev_unknown = false;
        is_separator_required = true;
      }
    }
  }
}

absl::optional<AutofillContextMenuManager::CommandId>
AutofillContextMenuManager::GetNextAvailableAutofillCommandId() {
  int max_index = kAutofillContextCustomLast - kAutofillContextCustomFirst;

  if (count_of_items_added_to_menu_model_ >= max_index)
    return absl::nullopt;

  return ConvertToAutofillCustomCommandId(
      count_of_items_added_to_menu_model_++);
}

std::u16string AutofillContextMenuManager::GetProfileDescription(
    const AutofillProfile& profile) {
  // All user-visible fields.
  static constexpr ServerFieldType kDetailsFields[] = {
      NAME_FULL,
      ADDRESS_HOME_LINE1,
      ADDRESS_HOME_LINE2,
      ADDRESS_HOME_DEPENDENT_LOCALITY,
      ADDRESS_HOME_CITY,
      ADDRESS_HOME_STATE,
      ADDRESS_HOME_ZIP,
      EMAIL_ADDRESS,
      PHONE_HOME_WHOLE_NUMBER,
      COMPANY_NAME,
      ADDRESS_HOME_COUNTRY};

  return profile.ConstructInferredLabel(
      kDetailsFields, std::size(kDetailsFields),
      /*num_fields_to_include=*/2, personal_data_manager_->app_locale());
}

}  // namespace autofill

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_context_menu_manager.h"

#include <string>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
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
  return AutofillContextMenuManager::CommandId(
      kAutofillContextCustomFirst + SubMenuType::NUM_SUBMENU_TYPES + offset);
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
    ui::SimpleMenuModel* menu_model,
    Browser* browser,
    content::RenderFrameHost* render_frame_host)
    : personal_data_manager_(personal_data_manager),
      menu_model_(menu_model),
      delegate_(delegate),
      browser_(browser),
      render_frame_host_(render_frame_host) {
  DCHECK(render_frame_host_);
}

AutofillContextMenuManager::~AutofillContextMenuManager() {
  cached_menu_models_.clear();
  command_id_to_menu_item_value_mapper_.clear();
}

void AutofillContextMenuManager::AppendItems() {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillShowManualFallbackInContextMenu)) {
    return;
  }

  DCHECK(personal_data_manager_);
  DCHECK(menu_model_);

  AutofillClient* autofill_client =
      autofill::ChromeAutofillClient::FromWebContents(
          content::WebContents::FromRenderFrameHost(render_frame_host_));
  // If the autofill popup is shown and the user double clicks from within the
  // bounds of the initiating field, it is assumed that the context menu would
  // overlap with the autofill popup. In that case, hide the autofill popup.
  if (autofill_client) {
    autofill_client->HideAutofillPopup(
        PopupHidingReason::kOverlappingWithAutofillContextMenu);
  }

  // Stores all the profile values added to the content menu alongwith the
  // command id of the row.
  std::vector<std::pair<CommandId, ContextMenuItem>>
      detail_items_added_to_context_menu;

  AppendAddressItems(detail_items_added_to_context_menu);
  AppendCreditCardItems(detail_items_added_to_context_menu);

  command_id_to_menu_item_value_mapper_ =
      base::flat_map<CommandId, ContextMenuItem>(
          std::move(detail_items_added_to_context_menu));
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
    const content::ContextMenuParams& params) {
  ContentAutofillDriver* driver =
      ContentAutofillDriver::GetForRenderFrameHost(render_frame_host_);
  if (!driver)
    return;

  ExecuteCommand(command_id, driver, params,
                 render_frame_host_->GetFrameToken());
}

void AutofillContextMenuManager::ExecuteCommand(
    CommandId command_id,
    ContentAutofillDriver* driver,
    const content::ContextMenuParams& params,
    const blink::LocalFrameToken local_frame_token) {
  auto it = command_id_to_menu_item_value_mapper_.find(command_id);
  if (it == command_id_to_menu_item_value_mapper_.end())
    return;

  DCHECK(IsAutofillCustomCommandId(command_id));

  // Field Renderer id should be present because the context menu is triggered
  // on a input field. Otherwise, Autofill context menu models would not have
  // been added to the context menu.
  if (!params.field_renderer_id)
    return;

  if (it->second.is_manage_item) {
    DCHECK(browser_);
    switch (it->second.sub_menu_type) {
      case SubMenuType::SUB_MENU_TYPE_ADDRESS:
        chrome::ShowAddresses(browser_);
        break;
      case SubMenuType::SUB_MENU_TYPE_CREDIT_CARD:
        chrome::ShowPaymentMethods(browser_);
        break;
      case SubMenuType::SUB_MENU_TYPE_PASSWORD:
        chrome::ShowPasswordManager(browser_);
        break;
      case SubMenuType::NUM_SUBMENU_TYPES:
        [[fallthrough]];
      default:
        NOTREACHED();
    }
    return;
  }

  driver->RendererShouldFillFieldWithValue(
      {LocalFrameToken(local_frame_token.value()),
       FieldRendererId(params.field_renderer_id.value())},
      it->second.fill_value);

  // TODO(crbug.com/1325811): Use `it->second.sub_menu_type` to record the usage
  // of the context menu based on the type.
}

void AutofillContextMenuManager::AppendAddressItems(
    std::vector<std::pair<CommandId, ContextMenuItem>>&
        detail_items_added_to_context_menu) {
  std::vector<AutofillProfile*> address_profiles =
      personal_data_manager_->GetProfiles();
  if (address_profiles.empty()) {
    return;
  }

  // Used to create menu model for storing address description. Would be
  // attached to the top level "Fill Address Info" item in the context menu.
  cached_menu_models_.push_back(
      std::make_unique<ui::SimpleMenuModel>(delegate_));
  ui::SimpleMenuModel* profile_menu = cached_menu_models_.back().get();

  // True if a row is added in the address section.
  bool address_added = false;

  for (const AutofillProfile* profile : address_profiles) {
    // Creates a menu model for storing address details. Would be attached
    // to the address description menu item.
    cached_menu_models_.push_back(
        std::make_unique<ui::SimpleMenuModel>(delegate_));
    ui::SimpleMenuModel* address_details_submenu =
        cached_menu_models_.back().get();

    // Create a submenu for each address profile with their details.
    bool submenu_items_added = CreateSubMenuWithData(
        profile, kAddressFieldTypesToShow, address_details_submenu,
        detail_items_added_to_context_menu, SubMenuType::SUB_MENU_TYPE_ADDRESS);

    if (!submenu_items_added) {
      break;
    }

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

  if (!address_added)
    return;

  profile_menu->AddSeparator(ui::NORMAL_SEPARATOR);
  absl::optional<CommandId> manage_item_command_id =
      GetNextAvailableAutofillCommandId();
  DCHECK(manage_item_command_id);
  // TODO(crbug.com/1325811): Use i18n string.
  profile_menu->AddItem(manage_item_command_id->value(), u"Manage addresses");
  detail_items_added_to_context_menu.emplace_back(
      *manage_item_command_id,
      ContextMenuItem{u"", SubMenuType::SUB_MENU_TYPE_ADDRESS, true});

  // Add a menu option to suggest filling address in the context menu.
  // Hovering over it opens a submenu suggesting all the address profiles
  // stored in the profile.
  // TODO(crbug.com/1325811): Use i18n string.
  menu_model_->AddSubMenu(
      kAutofillContextCustomFirst + SubMenuType::SUB_MENU_TYPE_ADDRESS,
      u"Fill Address Info", profile_menu);
}

void AutofillContextMenuManager::AppendCreditCardItems(
    std::vector<std::pair<CommandId, ContextMenuItem>>&
        detail_items_added_to_context_menu) {
  std::vector<CreditCard*> cards = personal_data_manager_->GetCreditCards();
  if (cards.empty()) {
    return;
  }

  // Used to create menu model for storing credit card description. Would be
  // attached to the top level "Fill Payment" item in the context menu.
  cached_menu_models_.push_back(
      std::make_unique<ui::SimpleMenuModel>(delegate_));
  ui::SimpleMenuModel* card_submenu = cached_menu_models_.back().get();

  // True if a row is added in the credit card section.
  bool card_added = false;

  for (const CreditCard* card : cards) {
    // Creates a menu model for storing credit card details. Would be attached
    // to the credit card description menu item.
    cached_menu_models_.push_back(
        std::make_unique<ui::SimpleMenuModel>(delegate_));
    ui::SimpleMenuModel* card_details_submenu =
        cached_menu_models_.back().get();

    // Create a submenu for each credit card with their details.
    bool submenu_items_added =
        CreateSubMenuWithData(card, kCardFieldTypesToShow, card_details_submenu,
                              detail_items_added_to_context_menu,
                              SubMenuType::SUB_MENU_TYPE_CREDIT_CARD);

    if (!submenu_items_added)
      break;

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

  if (!card_added)
    return;

  card_submenu->AddSeparator(ui::NORMAL_SEPARATOR);
  absl::optional<CommandId> manage_item_command_id =
      GetNextAvailableAutofillCommandId();
  DCHECK(manage_item_command_id);
  // TODO(crbug.com/1325811): Use i18n string.
  card_submenu->AddItem(manage_item_command_id->value(),
                        u"Manage payment methods");
  detail_items_added_to_context_menu.emplace_back(
      *manage_item_command_id,
      ContextMenuItem{u"", SubMenuType::SUB_MENU_TYPE_CREDIT_CARD, true});

  // Add a menu option to suggest filling a credit card in the context menu.
  // Hovering over it opens a submenu suggesting all the credit cards
  // stored in the profile.
  // TODO(crbug.com/1325811): Use i18n string.
  menu_model_->AddSubMenu(
      kAutofillContextCustomFirst + SubMenuType::SUB_MENU_TYPE_CREDIT_CARD,
      u"Fill Payment", card_submenu);
}

bool AutofillContextMenuManager::CreateSubMenuWithData(
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card,
    base::span<const ServerFieldType> field_types_to_show,
    ui::SimpleMenuModel* menu_model,
    std::vector<std::pair<CommandId, ContextMenuItem>>&
        detail_items_added_to_context_menu,
    SubMenuType sub_menu_type) {
  // Count of items to be added to the context menu. Empty values are not
  // considered.
  int count_of_items_to_be_added = base::ranges::count_if(
      field_types_to_show, [&](ServerFieldType field_type) {
        return field_type != UNKNOWN_TYPE &&
               !absl::visit(
                    [field_type](const auto& alternative) {
                      return alternative->GetRawInfo(field_type);
                    },
                    profile_or_credit_card)
                    .empty();
      });

  // Check if there are enough command ids for adding all the items to the
  // context menu.
  // 1 is added to count for the address/credit card description.
  // Another 1 is added to account for the manage addresses/payment methods
  // option.
  if (!IsAutofillCustomCommandId(CommandId(
          kAutofillContextCustomFirst + SubMenuType::NUM_SUBMENU_TYPES +
          count_of_items_added_to_menu_model_ + count_of_items_to_be_added + 1 +
          1))) {
    return false;
  }

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

    if (value.empty())
      continue;

    absl::optional<CommandId> value_menu_id =
        GetNextAvailableAutofillCommandId();

    DCHECK(value_menu_id);

    if (was_prev_unknown && is_separator_required) {
      menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
      is_separator_required = false;
    }

    // Create a menu item with the address/credit card details and attach
    // to the model.
    menu_model->AddItem(value_menu_id->value(), value);
    detail_items_added_to_context_menu.emplace_back(
        *value_menu_id, ContextMenuItem{value, sub_menu_type});
    was_prev_unknown = false;
    is_separator_required = true;
  }

  return true;
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

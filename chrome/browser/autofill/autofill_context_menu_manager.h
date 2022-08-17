// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/browser/field_types.h"
#include "content/public/browser/context_menu_params.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace autofill {

class AutofillProfile;
class ContentAutofillDriver;
class CreditCard;
class PersonalDataManager;

// Determines the type of the item added to the context menu.
enum SubMenuType {
  // Addresses
  SUB_MENU_TYPE_ADDRESS = 0,
  // Credit Cards
  SUB_MENU_TYPE_CREDIT_CARD = 1,
  // Passwords
  SUB_MENU_TYPE_PASSWORD = 2,
  NUM_SUBMENU_TYPES
};

// Stores data about an item added to the  context menu.
struct ContextMenuItem {
  // Represents the string value that is displayed in a row in the context menu.
  // If selected, should result in filling.
  std::u16string fill_value;

  // Represents the type that this item belongs to out of address, credit cards
  // and passwords.
  SubMenuType sub_menu_type;

  // The context menu item represents a manage option. Example: Manage
  // addresses/ Manage payment methods options.
  bool is_manage_item = false;
};

// `AutofillContextMenuManager` is responsible for adding/executing Autofill
// related context menu items. `RenderViewContextMenu` is intended to own and
// control the lifetime of `AutofillContextMenuManager`.
class AutofillContextMenuManager {
 public:
  // Represents command id used to denote a row in the context menu. The
  // command ids are created when the items are added to the context menu during
  // it's initialization. For Autofill, it ranges from
  // `IDC_CONTENT_CONTEXT_AUTOFILL_CUSTOM_FIRST` to
  // `IDC_CONTENT_CONTEXT_AUTOFILL_CUSTOM_LAST`.
  using CommandId = base::StrongAlias<class CommandIdTag, int>;

  // Convert a command ID so that it fits within the range for
  // autofill context menu. `offset` is the count of the items added to the
  // autofill context menu.
  static CommandId ConvertToAutofillCustomCommandId(int offset);

  // Returns true if the given id is one generated for autofill context menu.
  static bool IsAutofillCustomCommandId(CommandId command_id);

  AutofillContextMenuManager(PersonalDataManager* personal_data_manager,
                             ui::SimpleMenuModel::Delegate* delegate,
                             ui::SimpleMenuModel* menu_model,
                             Browser* browser,
                             content::RenderFrameHost* render_frame_host);
  ~AutofillContextMenuManager();
  AutofillContextMenuManager(const AutofillContextMenuManager&) = delete;
  AutofillContextMenuManager& operator=(const AutofillContextMenuManager&) =
      delete;

  // Adds items such as "Addresses"/"Credit Cards"/"Passwords" to the top level
  // of the context menu.
  void AppendItems();

  // `AutofillContextMenuManager` specific, called from `RenderViewContextMenu`.
  bool IsCommandIdChecked(CommandId command_id) const;
  bool IsCommandIdVisible(CommandId command_id) const;
  bool IsCommandIdEnabled(CommandId command_id) const;
  // TODO(crbug.com/1325811): Add tests for the method.
  void ExecuteCommand(CommandId command_id,
                      const content::ContextMenuParams& params);
  void ExecuteCommand(CommandId command_id,
                      ContentAutofillDriver* driver,
                      const content::ContextMenuParams& params,
                      const blink::LocalFrameToken local_frame_token);

#if defined(UNIT_TEST)
  // Getter for `command_id_to_menu_item_value_mapper_` used for testing
  // purposes.
  const base::flat_map<CommandId, ContextMenuItem>&
  command_id_to_menu_item_value_mapper_for_testing() const {
    return command_id_to_menu_item_value_mapper_;
  }
#endif

 private:
  // Adds address items to the context menu.
  void AppendAddressItems(std::vector<std::pair<CommandId, ContextMenuItem>>&
                              item_details_added_to_context_menu);

  // Adds credit card items to the context menu.
  void AppendCreditCardItems(std::vector<std::pair<CommandId, ContextMenuItem>>&
                                 item_details_added_to_context_menu);

  // Fetches value from `profile_or_credit_card` based on the type from
  // `field_types_to_show` and adds them to the `menu_model`.
  bool CreateSubMenuWithData(
      absl::variant<const AutofillProfile*, const CreditCard*>
          profile_or_credit_card,
      base::span<const ServerFieldType> field_types_to_show,
      ui::SimpleMenuModel* menu_model,
      std::vector<std::pair<CommandId, ContextMenuItem>>&
          item_details_added_to_context_menu,
      SubMenuType sub_menu_type);

  // Returns a description for the given `profile`.
  std::u16string GetProfileDescription(const AutofillProfile& profile);

  // Returns the next available command id for adding an item to the context
  // menu for Autofill.
  absl::optional<CommandId> GetNextAvailableAutofillCommandId();

  raw_ptr<PersonalDataManager> personal_data_manager_;
  raw_ptr<ui::SimpleMenuModel> menu_model_;
  raw_ptr<ui::SimpleMenuModel::Delegate> delegate_;
  raw_ptr<Browser> browser_;
  raw_ptr<content::RenderFrameHost> render_frame_host_;

  // Stores the count of items added to the context menu from Autofill.
  int count_of_items_added_to_menu_model_ = 0;

  // Keep track of and clean up menu models for submenus.
  std::vector<std::unique_ptr<ui::SimpleMenuModel>> cached_menu_models_;

  // Stores the mapping of command ids with the item values added to the context
  // menu.
  // Only items that contain the address or credit card details are stored.
  base::flat_map<CommandId, ContextMenuItem>
      command_id_to_menu_item_value_mapper_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_

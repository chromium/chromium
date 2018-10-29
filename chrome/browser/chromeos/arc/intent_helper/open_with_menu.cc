// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/intent_helper/open_with_menu.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/grit/generated_resources.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/context_menu_params.h"
#include "ui/base/l10n/l10n_util.h"

namespace arc {

const int OpenWithMenu::kNumMainMenuCommands = 4;
const int OpenWithMenu::kNumSubMenuCommands = 10;

bool OpenWithMenu::SubMenuDelegate::IsCommandIdChecked(int command_id) const {
  return false;
}

bool OpenWithMenu::SubMenuDelegate::IsCommandIdEnabled(int command_id) const {
  return true;
}

void OpenWithMenu::SubMenuDelegate::ExecuteCommand(int command_id,
                                                   int event_flags) {
  parent_->ExecuteCommand(command_id);
}

OpenWithMenu::OpenWithMenu(content::BrowserContext* context,
                           RenderViewContextMenuProxy* proxy)
    : context_(context),
      proxy_(proxy),
      more_apps_label_(
          l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_MORE_APPS)) {}

OpenWithMenu::~OpenWithMenu() = default;

void OpenWithMenu::InitMenu(const content::ContextMenuParams& params) {
  // Enforcing no items are added to the context menu during incognito mode.
  if (context_->IsOffTheRecord())
    return;

  menu_model_ = LinkHandlerModel::Create(context_, params.link_url);
  if (!menu_model_)
    return;

  // Add placeholder items.
  std::unique_ptr<ui::SimpleMenuModel> submenu(
      new ui::SimpleMenuModel(&submenu_delegate_));
  AddPlaceholderItems(proxy_, submenu.get());
  submenu_ = std::move(submenu);

  menu_model_->AddObserver(this);
}

bool OpenWithMenu::IsCommandIdSupported(int command_id) {
  return command_id >= IDC_CONTENT_CONTEXT_OPEN_WITH1 &&
         command_id <= IDC_CONTENT_CONTEXT_OPEN_WITH_LAST;
}

bool OpenWithMenu::IsCommandIdChecked(int command_id) {
  return false;
}

bool OpenWithMenu::IsCommandIdEnabled(int command_id) {
  return true;
}

void OpenWithMenu::ExecuteCommand(int command_id) {
  // Note: SubmenuDelegate also calls this method with a command_id for the
  // submenu.
  const auto it = handlers_.find(command_id);
  if (it == handlers_.end())
    return;

  menu_model_->OpenLinkWithHandler(it->second.id);
}

void OpenWithMenu::ModelChanged(const std::vector<LinkHandlerInfo>& handlers) {
  auto result = BuildHandlersMap(handlers);
  handlers_ = std::move(result.first);
  const int submenu_parent_id = result.second;
  for (int command_id = IDC_CONTENT_CONTEXT_OPEN_WITH1;
       command_id <= IDC_CONTENT_CONTEXT_OPEN_WITH_LAST; ++command_id) {
    const auto it = handlers_.find(command_id);
    if (command_id == submenu_parent_id) {
      // Show the submenu parent.
      proxy_->UpdateMenuItem(command_id, true, false, more_apps_label_);
    } else if (it == handlers_.end()) {
      // Hide the menu or submenu parent.
      proxy_->UpdateMenuItem(command_id, false, true, base::string16());
    } else {
      // Update the menu with the new model.
      const base::string16 label = l10n_util::GetStringFUTF16(
          IDS_CONTENT_CONTEXT_OPEN_WITH_APP, it->second.name);
      proxy_->UpdateMenuItem(command_id, true, false, label);
      if (!it->second.icon.IsEmpty())
        proxy_->UpdateMenuIcon(command_id, it->second.icon);
    }
  }
}

void OpenWithMenu::AddPlaceholderItemsForTesting(
    RenderViewContextMenuProxy* proxy,
    ui::SimpleMenuModel* submenu) {
  return AddPlaceholderItems(proxy, submenu);
}

std::pair<OpenWithMenu::HandlerMap, int>
OpenWithMenu::BuildHandlersMapForTesting(
    const std::vector<LinkHandlerInfo>& handlers) {
  return BuildHandlersMap(handlers);
}

void OpenWithMenu::AddPlaceholderItems(RenderViewContextMenuProxy* proxy,
                                       ui::SimpleMenuModel* submenu) {
  for (int i = 0; i < kNumSubMenuCommands; ++i) {
    const int command_id =
        IDC_CONTENT_CONTEXT_OPEN_WITH1 + kNumMainMenuCommands + i;
    submenu->AddItem(command_id, base::string16());
  }
  int command_id;
  for (int i = 0; i < kNumMainMenuCommands - 1; ++i) {
    command_id = IDC_CONTENT_CONTEXT_OPEN_WITH1 + i;
    proxy->AddMenuItem(command_id, base::string16());
  }
  proxy->AddSubMenu(++command_id, base::string16(), submenu);
}

std::pair<OpenWithMenu::HandlerMap, int> OpenWithMenu::BuildHandlersMap(
    const std::vector<LinkHandlerInfo>& handlers) {
  const int kInvalidCommandId = -1;
  const int submenu_id_start =
      IDC_CONTENT_CONTEXT_OPEN_WITH1 + kNumMainMenuCommands;

  OpenWithMenu::HandlerMap handler_map;
  int submenu_parent_command_id = kInvalidCommandId;

  const int num_apps = handlers.size();
  size_t handlers_index = 0;
  // We use the last item in the main menu (IDC_CONTENT_CONTEXT_OPEN_WITH1 +
  // kNumMainMenuCommands- 1) as a parent of a submenu, and others as regular
  // menu items.
  if (num_apps < kNumMainMenuCommands) {
    // All apps can be shown with the regular main menu items.
    for (int i = 0; i < num_apps; ++i) {
      handler_map[IDC_CONTENT_CONTEXT_OPEN_WITH1 + i] =
          handlers[handlers_index++];
    }
  } else {
    // Otherwise, use the submenu too. In this case, disable the last item of
    // the regular main menu (hence '-2').
    for (int i = 0; i < kNumMainMenuCommands - 2; ++i) {
      handler_map[IDC_CONTENT_CONTEXT_OPEN_WITH1 + i] =
          handlers[handlers_index++];
    }
    submenu_parent_command_id =
        IDC_CONTENT_CONTEXT_OPEN_WITH1 + kNumMainMenuCommands - 1;
    const int sub_items =
        std::min(num_apps - (kNumMainMenuCommands - 2), kNumSubMenuCommands);
    for (int i = 0; i < sub_items; ++i) {
      handler_map[submenu_id_start + i] = handlers[handlers_index++];
    }
  }

  return std::make_pair(std::move(handler_map), submenu_parent_command_id);
}

}  // namespace arc

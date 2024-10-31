// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/coral_util.h"

#include "ash/multi_user/multi_user_window_manager_impl.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"

namespace ash::coral_util {

TabsAndApps::TabsAndApps() = default;

TabsAndApps::TabsAndApps(const TabsAndApps& other) = default;

TabsAndApps& TabsAndApps::operator=(const TabsAndApps& other) = default;

TabsAndApps::~TabsAndApps() = default;

std::string GetIdentifier(const coral::mojom::EntityPtr& item) {
  if (item->is_app()) {
    return item->get_app()->id;
  }
  if (item->is_tab()) {
    return item->get_tab()->url.possibly_invalid_spec();
  }
  NOTREACHED();
}

std::string GetIdentifier(const coral::mojom::Entity& item) {
  if (item.is_app()) {
    return item.get_app()->id;
  }
  if (item.is_tab()) {
    return item.get_tab()->url.possibly_invalid_spec();
  }
  NOTREACHED();
}

bool CanMoveToNewDesk(aura::Window* window) {
  auto* delegate = Shell::Get()->saved_desk_delegate();

  // We should guarantee the window can be launched in saved desk template.
  if (!delegate->IsWindowSupportedForSavedDesk(window)) {
    return false;
  }

  // The window should belongs to the current active user.
  if (auto* window_manager = MultiUserWindowManagerImpl::Get()) {
    const AccountId& window_owner = window_manager->GetWindowOwner(window);
    const AccountId& active_owner =
        Shell::Get()->session_controller()->GetActiveAccountId();
    if (window_owner.is_valid() && active_owner != window_owner) {
      return false;
    }
  }
  return true;
}

TabsAndApps SplitContentData(
    const std::vector<coral::mojom::EntityPtr>& content) {
  TabsAndApps split;

  // Extract tab data and app data from content data.
  for (const auto& data : content) {
    if (data->is_tab()) {
      split.tabs.emplace_back(*data->get_tab());
    } else {
      split.apps.emplace_back(*data->get_app());
    }
  }

  return split;
}

base::Value::List EntitiesToListValue(
    const std::vector<coral::mojom::EntityPtr>& entities) {
  auto list = base::Value::List();
  for (const coral::mojom::EntityPtr& entity : entities) {
    auto entity_value = base::Value::Dict();
    if (entity->is_tab()) {
      entity_value.Set("Tab", base::Value::Dict()
                                  .Set("Title", entity->get_tab()->title)
                                  .Set("Url", entity->get_tab()->url.spec()));
    }
    if (entity->is_app()) {
      entity_value.Set("App", base::Value::Dict()
                                  .Set("Title", entity->get_app()->title)
                                  .Set("Id", entity->get_app()->id));
    }
    list.Append(std::move(entity_value));
  }
  return list;
}

std::string GroupToString(const coral::mojom::GroupPtr& group) {
  auto root = base::Value::Dict().Set(
      "Group",
      base::Value::Dict()
          .Set("Title", group->title.value_or("No title"))
          .Set("Entities", coral_util::EntitiesToListValue(group->entities)));
  return root.DebugString();
}

}  // namespace ash::coral_util

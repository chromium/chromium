// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/status_icons/status_icon_menu_model.h"

#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/image/image.h"

struct StatusIconMenuModel::ItemState {
  ItemState()
      : checked(false),
        enabled(true),
        visible(true),
        is_dynamic(false) {}
  bool checked;
  bool enabled;
  bool visible;
  bool is_dynamic;
  ui::Accelerator accelerator;
  std::u16string label;
  gfx::Image icon;
};

////////////////////////////////////////////////////////////////////////////////
// StatusIconMenuModel, public:

StatusIconMenuModel::StatusIconMenuModel(Delegate* delegate)
    : ui::SimpleMenuModel(this), delegate_(delegate) {
}

StatusIconMenuModel::~StatusIconMenuModel() {
}

void StatusIconMenuModel::SetCommandIdChecked(int command_id, bool checked) {
  item_states_[command_id].checked = checked;
  NotifyMenuStateChanged();
}

void StatusIconMenuModel::SetCommandIdEnabled(int command_id, bool enabled) {
  item_states_[command_id].enabled = enabled;
  NotifyMenuStateChanged();
}

void StatusIconMenuModel::SetCommandIdVisible(int command_id, bool visible) {
  item_states_[command_id].visible = visible;
  NotifyMenuStateChanged();
}

void StatusIconMenuModel::SetAcceleratorForCommandId(
    int command_id, const ui::Accelerator* accelerator) {
  item_states_[command_id].accelerator = *accelerator;
  NotifyMenuStateChanged();
}

void StatusIconMenuModel::ChangeLabelForCommandId(int command_id,
                                                  const std::u16string& label) {
  item_states_[command_id].is_dynamic = true;
  item_states_[command_id].label = label;
  NotifyMenuStateChanged();
}

void StatusIconMenuModel::ChangeIconForCommandId(
    int command_id, const gfx::Image& icon) {
  item_states_[command_id].is_dynamic = true;
  item_states_[command_id].icon = icon;
  NotifyMenuStateChanged();
}

void StatusIconMenuModel::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void StatusIconMenuModel::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool StatusIconMenuModel::IsCommandIdChecked(int command_id) const {
  auto iter = item_states_.find(command_id);
  if (iter != item_states_.end())
    return iter->second.checked;
  return false;
}

bool StatusIconMenuModel::IsCommandIdEnabled(int command_id) const {
  auto iter = item_states_.find(command_id);
  if (iter != item_states_.end())
    return iter->second.enabled;
  return true;
}

bool StatusIconMenuModel::IsCommandIdVisible(int command_id) const {
  auto iter = item_states_.find(command_id);
  if (iter != item_states_.end())
    return iter->second.visible;
  return true;
}

bool StatusIconMenuModel::GetAcceleratorForCommandId(
    int command_id, ui::Accelerator* accelerator) const {
  auto iter = item_states_.find(command_id);
  if (iter != item_states_.end() &&
      iter->second.accelerator.key_code() != ui::VKEY_UNKNOWN) {
    *accelerator = iter->second.accelerator;
    return true;
  }
  return false;
}

bool StatusIconMenuModel::IsItemForCommandIdDynamic(int command_id) const {
  auto iter = item_states_.find(command_id);
  if (iter != item_states_.end())
    return iter->second.is_dynamic;
  return false;
}

std::u16string StatusIconMenuModel::GetLabelForCommandId(int command_id) const {
  auto iter = item_states_.find(command_id);
  if (iter != item_states_.end())
    return iter->second.label;
  return std::u16string();
}

ui::ImageModel StatusIconMenuModel::GetIconForCommandId(int command_id) const {
  auto iter = item_states_.find(command_id);
  if (iter != item_states_.end() && !iter->second.icon.IsEmpty())
    return ui::ImageModel::FromImage(iter->second.icon);
  return ui::ImageModel();
}

////////////////////////////////////////////////////////////////////////////////
// StatusIconMenuModel, protected:

void StatusIconMenuModel::MenuItemsChanged() {
  NotifyMenuStateChanged();
}

void StatusIconMenuModel::NotifyMenuStateChanged() {
  for (Observer& observer : observer_list_)
    observer.OnMenuStateChanged();
}

////////////////////////////////////////////////////////////////////////////////
// StatusIconMenuModel, private:

void StatusIconMenuModel::ExecuteCommand(int command_id, int event_flags) {
  if (delegate_)
    delegate_->ExecuteCommand(command_id, event_flags);
}

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_context_menu_observer.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_metrics.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sync_device_info/device_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"

ClickToCallContextMenuObserver::SubMenuDelegate::SubMenuDelegate(
    ClickToCallContextMenuObserver* parent)
    : parent_(parent) {}

ClickToCallContextMenuObserver::SubMenuDelegate::~SubMenuDelegate() = default;

bool ClickToCallContextMenuObserver::SubMenuDelegate::IsCommandIdEnabled(
    int command_id) const {
  // All supported commands are enabled in sub menu.
  return true;
}

void ClickToCallContextMenuObserver::SubMenuDelegate::ExecuteCommand(
    int command_id,
    int event_flags) {
  if (command_id < kSubMenuFirstDeviceCommandId ||
      command_id > kSubMenuLastDeviceCommandId) {
    return;
  }
  int device_index = command_id - kSubMenuFirstDeviceCommandId;
  parent_->SendClickToCallMessage(device_index);
}

ClickToCallContextMenuObserver::ClickToCallContextMenuObserver(
    RenderViewContextMenuProxy* proxy)
    : proxy_(proxy),
      controller_(ClickToCallUiController::GetOrCreateFromWebContents(
          proxy_->GetWebContents())) {}

ClickToCallContextMenuObserver::~ClickToCallContextMenuObserver() = default;

void ClickToCallContextMenuObserver::BuildMenu(
    const std::string& phone_number,
    const std::string& selection_text,
    SharingClickToCallEntryPoint entry_point) {
  DCHECK(!phone_number.empty());

  phone_number_ = phone_number;
  selection_text_ = selection_text;
  entry_point_ = entry_point;
  devices_ = controller_->GetDevices();
  LogSharingDevicesToShow(controller_->GetFeatureMetricsPrefix(),
                          kSharingUiContextMenu, devices_.size());
  if (devices_.empty())
    return;

  if (devices_.size() == 1) {
#if BUILDFLAG(IS_MAC)
    proxy_->AddMenuItem(
        IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
        l10n_util::GetStringFUTF16(
            IDS_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
            base::UTF8ToUTF16(devices_[0].client_name())));
#else
    proxy_->AddMenuItemWithIcon(
        IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
        l10n_util::GetStringFUTF16(
            IDS_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
            base::UTF8ToUTF16(devices_[0].client_name())),
        ui::ImageModel::FromVectorIcon(controller_->GetVectorIcon(),
                                       ui::kColorMenuIcon,
                                       ui::SimpleMenuModel::kDefaultIconSize));
#endif
  } else {
    BuildSubMenu();
#if BUILDFLAG(IS_MAC)
    proxy_->AddSubMenu(
        IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES,
        l10n_util::GetStringUTF16(
            IDS_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES),
        sub_menu_model_.get());
#else
    proxy_->AddSubMenuWithStringIdAndIcon(
        IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES,
        IDS_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES,
        sub_menu_model_.get(),
        ui::ImageModel::FromVectorIcon(controller_->GetVectorIcon(),
                                       ui::kColorMenuIcon,
                                       ui::SimpleMenuModel::kDefaultIconSize));
#endif
  }
}

void ClickToCallContextMenuObserver::BuildSubMenu() {
  sub_menu_model_ = std::make_unique<ui::SimpleMenuModel>(&sub_menu_delegate_);

  int command_id = kSubMenuFirstDeviceCommandId;
  for (const auto& device : devices_) {
    if (command_id > kSubMenuLastDeviceCommandId)
      break;
    sub_menu_model_->AddItem(command_id++,
                             base::UTF8ToUTF16(device.client_name()));
  }
}

bool ClickToCallContextMenuObserver::IsCommandIdSupported(int command_id) {
  size_t device_count = devices_.size();
  if (device_count == 0)
    return false;

  if (device_count == 1) {
    return command_id ==
           IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE;
  } else {
    return command_id ==
           IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES;
  }
}

bool ClickToCallContextMenuObserver::IsCommandIdEnabled(int command_id) {
  // All supported commands are enabled.
  return true;
}

void ClickToCallContextMenuObserver::ExecuteCommand(int command_id) {
  if (command_id == IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE) {
    DCHECK_EQ(1u, devices_.size());
    SendClickToCallMessage(0);
  }
}

void ClickToCallContextMenuObserver::SendClickToCallMessage(
    int chosen_device_index) {
  DCHECK(entry_point_);
  if (static_cast<size_t>(chosen_device_index) >= devices_.size())
    return;

  LogSharingSelectedIndex(controller_->GetFeatureMetricsPrefix(),
                          kSharingUiContextMenu, chosen_device_index);

  controller_->OnDeviceSelected(phone_number_, devices_[chosen_device_index],
                                *entry_point_);
}

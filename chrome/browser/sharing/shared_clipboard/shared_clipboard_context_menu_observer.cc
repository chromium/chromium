// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_context_menu_observer.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_ui_controller.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync_device_info/device_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"

SharedClipboardContextMenuObserver::SubMenuDelegate::SubMenuDelegate(
    SharedClipboardContextMenuObserver* parent)
    : parent_(parent) {}

SharedClipboardContextMenuObserver::SubMenuDelegate::~SubMenuDelegate() =
    default;

bool SharedClipboardContextMenuObserver::SubMenuDelegate::IsCommandIdEnabled(
    int command_id) const {
  // All supported commands are enabled in sub menu.
  return true;
}

void SharedClipboardContextMenuObserver::SubMenuDelegate::ExecuteCommand(
    int command_id,
    int event_flags) {
  if (command_id < kSubMenuFirstDeviceCommandId ||
      command_id > kSubMenuLastDeviceCommandId)
    return;
  int device_index = command_id - kSubMenuFirstDeviceCommandId;
  parent_->SendSharedClipboardMessage(device_index);
}

SharedClipboardContextMenuObserver::SharedClipboardContextMenuObserver(
    RenderViewContextMenuProxy* proxy)
    : proxy_(proxy),
      controller_(SharedClipboardUiController::GetOrCreateFromWebContents(
          proxy_->GetWebContents())) {}

SharedClipboardContextMenuObserver::~SharedClipboardContextMenuObserver() =
    default;

void SharedClipboardContextMenuObserver::InitMenu(
    const content::ContextMenuParams& params) {
  text_ = params.selection_text;
  devices_ = controller_->GetDevices();
  LogSharingDevicesToShow(controller_->GetFeatureMetricsPrefix(),
                          nullptr /* No suffix */, devices_.size());

  if (devices_.empty())
    return;

  if (devices_.size() == 1) {
#if defined(OS_MAC)
    proxy_->AddMenuItem(
        IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_SINGLE_DEVICE,
        l10n_util::GetStringFUTF16(
            IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_SINGLE_DEVICE,
            base::UTF8ToUTF16(devices_[0]->client_name())));
#else
    proxy_->AddMenuItemWithIcon(
        IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_SINGLE_DEVICE,
        l10n_util::GetStringFUTF16(
            IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_SINGLE_DEVICE,
            base::UTF8ToUTF16(devices_[0]->client_name())),
        ui::ImageModel::FromVectorIcon(controller_->GetVectorIcon(),
                                       /*color_id=*/-1,
                                       ui::SimpleMenuModel::kDefaultIconSize));
#endif
  } else {
    BuildSubMenu();
#if defined(OS_MAC)
    proxy_->AddSubMenu(
        IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_MULTIPLE_DEVICES,
        l10n_util::GetStringUTF16(
            IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_MULTIPLE_DEVICES),
        sub_menu_model_.get());
#else
    proxy_->AddSubMenuWithStringIdAndIcon(
        IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_MULTIPLE_DEVICES,
        IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_MULTIPLE_DEVICES,
        sub_menu_model_.get(),
        ui::ImageModel::FromVectorIcon(controller_->GetVectorIcon(),
                                       /*color_id=*/-1,
                                       ui::SimpleMenuModel::kDefaultIconSize));
#endif
  }
}

void SharedClipboardContextMenuObserver::BuildSubMenu() {
  sub_menu_model_ = std::make_unique<ui::SimpleMenuModel>(&sub_menu_delegate_);

  int command_id = kSubMenuFirstDeviceCommandId;
  for (const auto& device : devices_) {
    if (command_id > kSubMenuLastDeviceCommandId)
      break;
    sub_menu_model_->AddItem(command_id++,
                             base::UTF8ToUTF16(device->client_name()));
  }
}

bool SharedClipboardContextMenuObserver::IsCommandIdSupported(int command_id) {
  size_t device_count = devices_.size();
  if (device_count == 0)
    return false;

  if (device_count == 1) {
    return command_id ==
           IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_SINGLE_DEVICE;
  } else {
    return command_id ==
           IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_MULTIPLE_DEVICES;
  }
}

bool SharedClipboardContextMenuObserver::IsCommandIdEnabled(int command_id) {
  // All supported commands are enabled.
  return true;
}

void SharedClipboardContextMenuObserver::ExecuteCommand(int command_id) {
  if (command_id ==
      IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_SINGLE_DEVICE) {
    DCHECK_EQ(1u, devices_.size());
    SendSharedClipboardMessage(0);
  }
}

void SharedClipboardContextMenuObserver::SendSharedClipboardMessage(
    int chosen_device_index) {
  if (size_t{chosen_device_index} >= devices_.size())
    return;
  LogSharingSelectedIndex(controller_->GetFeatureMetricsPrefix(),
                          nullptr /* No suffix */, chosen_device_index);

  controller_->OnDeviceSelected(text_, *devices_[chosen_device_index]);
  LogSharedClipboardSelectedTextSize(text_.size());
}

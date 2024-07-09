// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/device_permissions_dialog_controller.h"

#include "base/not_fatal_until.h"
#include "chrome/browser/chooser_controller/title_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

DevicePermissionsDialogController::DevicePermissionsDialogController(
    content::RenderFrameHost* owner,
    scoped_refptr<extensions::DevicePermissionsPrompt::Prompt> prompt)
    : ChooserController(CreateChooserTitle(
          owner,
          prompt->multiple() ? IDS_DEVICE_PERMISSIONS_PROMPT_MULTIPLE_SELECTION
                             : IDS_DEVICE_PERMISSIONS_PROMPT_SINGLE_SELECTION)),
      prompt_(prompt) {
  prompt_->SetObserver(this);
}

DevicePermissionsDialogController::~DevicePermissionsDialogController() {
  prompt_->SetObserver(nullptr);
}

bool DevicePermissionsDialogController::ShouldShowHelpButton() const {
  return false;
}

bool DevicePermissionsDialogController::AllowMultipleSelection() const {
  return prompt_->multiple();
}

std::u16string DevicePermissionsDialogController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
}

std::u16string DevicePermissionsDialogController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_DEVICE_PERMISSIONS_DIALOG_SELECT);
}

std::pair<std::u16string, std::u16string>
DevicePermissionsDialogController::GetThrobberLabelAndTooltip() const {
  return {
      l10n_util::GetStringUTF16(IDS_DEVICE_PERMISSIONS_DIALOG_LOADING_LABEL),
      l10n_util::GetStringUTF16(
          IDS_DEVICE_PERMISSIONS_DIALOG_LOADING_LABEL_TOOLTIP)};
}

size_t DevicePermissionsDialogController::NumOptions() const {
  return prompt_->GetDeviceCount();
}

std::u16string DevicePermissionsDialogController::GetOption(
    size_t index) const {
  std::u16string device_name = prompt_->GetDeviceName(index);
  const auto& it = device_name_map_.find(device_name);
  CHECK(it != device_name_map_.end(), base::NotFatalUntil::M130);
  return it->second == 1
             ? device_name
             : l10n_util::GetStringFUTF16(
                   IDS_DEVICE_CHOOSER_DEVICE_NAME_WITH_ID, device_name,
                   prompt_->GetDeviceSerialNumber(index));
}

void DevicePermissionsDialogController::Select(
    const std::vector<size_t>& indices) {
  for (size_t index : indices)
    prompt_->GrantDevicePermission(index);
  prompt_->Dismissed();
}

void DevicePermissionsDialogController::Cancel() {
  prompt_->Dismissed();
}

void DevicePermissionsDialogController::Close() {
  prompt_->Dismissed();
}

void DevicePermissionsDialogController::OpenHelpCenterUrl() const {}

void DevicePermissionsDialogController::OnDevicesInitialized() {
  if (view()) {
    view()->OnOptionsInitialized();
  }
}

void DevicePermissionsDialogController::OnDeviceAdded(
    size_t index,
    const std::u16string& device_name) {
  if (view()) {
    ++device_name_map_[device_name];
    view()->OnOptionAdded(index);
  }
}

void DevicePermissionsDialogController::OnDeviceRemoved(
    size_t index,
    const std::u16string& device_name) {
  if (view()) {
    DCHECK_GT(device_name_map_[device_name], 0);
    if (--device_name_map_[device_name] == 0)
      device_name_map_.erase(device_name);
    view()->OnOptionRemoved(index);
  }
}

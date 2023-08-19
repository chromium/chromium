// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DEVICE_PERMISSIONS_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_DEVICE_PERMISSIONS_DIALOG_CONTROLLER_H_

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "components/permissions/chooser_controller.h"
#include "extensions/browser/api/device_permissions_prompt.h"

namespace content {
class RenderFrameHost;
}

class DevicePermissionsDialogController
    : public permissions::ChooserController,
      public extensions::DevicePermissionsPrompt::Prompt::Observer {
 public:
  DevicePermissionsDialogController(
      content::RenderFrameHost* owner,
      scoped_refptr<extensions::DevicePermissionsPrompt::Prompt> prompt);

  DevicePermissionsDialogController(const DevicePermissionsDialogController&) =
      delete;
  DevicePermissionsDialogController& operator=(
      const DevicePermissionsDialogController&) = delete;

  ~DevicePermissionsDialogController() override;

  // permissions::ChooserController:
  bool ShouldShowHelpButton() const override;
  bool AllowMultipleSelection() const override;
  std::u16string GetNoOptionsText() const override;
  std::u16string GetOkButtonLabel() const override;
  std::pair<std::u16string, std::u16string> GetThrobberLabelAndTooltip()
      const override;
  size_t NumOptions() const override;
  std::u16string GetOption(size_t index) const override;
  void Select(const std::vector<size_t>& indices) override;
  void Cancel() override;
  void Close() override;
  void OpenHelpCenterUrl() const override;

  // extensions::DevicePermissionsPrompt::Prompt::Observer:
  void OnDevicesInitialized() override;
  void OnDeviceAdded(size_t index, const std::u16string& device_name) override;
  void OnDeviceRemoved(size_t index,
                       const std::u16string& device_name) override;

 private:
  scoped_refptr<extensions::DevicePermissionsPrompt::Prompt> prompt_;
  // Maps from device name to number of devices.
  base::flat_map<std::u16string, int> device_name_map_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_DEVICE_PERMISSIONS_DIALOG_CONTROLLER_H_

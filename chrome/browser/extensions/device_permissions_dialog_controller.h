// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DEVICE_PERMISSIONS_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_DEVICE_PERMISSIONS_DIALOG_CONTROLLER_H_

#include <unordered_map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "extensions/browser/api/device_permissions_prompt.h"

class DevicePermissionsDialogController
    : public ChooserController,
      public extensions::DevicePermissionsPrompt::Prompt::Observer {
 public:
  DevicePermissionsDialogController(
      content::RenderFrameHost* owner,
      scoped_refptr<extensions::DevicePermissionsPrompt::Prompt> prompt);
  ~DevicePermissionsDialogController() override;

  // ChooserController:
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
  std::unordered_map<std::u16string, int> device_name_map_;

  DISALLOW_COPY_AND_ASSIGN(DevicePermissionsDialogController);
};

#endif  // CHROME_BROWSER_EXTENSIONS_DEVICE_PERMISSIONS_DIALOG_CONTROLLER_H_

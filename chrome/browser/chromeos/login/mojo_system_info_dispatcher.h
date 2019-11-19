// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOJO_SYSTEM_INFO_DISPATCHER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOJO_SYSTEM_INFO_DISPATCHER_H_

#include "chrome/browser/chromeos/login/version_info_updater.h"

namespace chromeos {

// Fetches system information and sends it over the login_screen mojo.
class MojoSystemInfoDispatcher : public VersionInfoUpdater::Delegate {
 public:
  MojoSystemInfoDispatcher();
  ~MojoSystemInfoDispatcher() override;

  // Request the system info.
  void StartRequest();

  // VersionInfoUpdater::Delegate:
  void OnOSVersionLabelTextUpdated(
      const std::string& os_version_label_text) override;
  void OnEnterpriseInfoUpdated(const std::string& enterprise_info,
                               const std::string& asset_id) override;
  void OnDeviceInfoUpdated(const std::string& bluetooth_name) override;
  void OnAdbSideloadStatusUpdated(bool enabled) override;

 private:
  // Sends a new mojo call based on the currently stored system information.
  void OnSystemInfoUpdated();

  void OnQueryAdbSideload(bool success, bool enabled);

  // Used to fetch the system/version information.
  VersionInfoUpdater version_info_updater_{this};

  std::string os_version_label_text_;
  std::string enterprise_info_;
  std::string bluetooth_name_;
  bool adb_sideloading_enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(MojoSystemInfoDispatcher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOJO_SYSTEM_INFO_DISPATCHER_H_

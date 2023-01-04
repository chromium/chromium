// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_H_

#include "base/guid.h"
#include "chromeos/ash/components/dbus/concierge/concierge_service.pb.h"
#include "components/download/public/background_service/download_metadata.h"

namespace bruschetta {

class BruschettaInstaller {
 public:
  enum class State {
    kInstallStarted,
    kDlcInstall,
    kFirmwareDownload,
    kFirmwareMount,
    kBootDiskDownload,
    kBootDiskMount,
    kOpenFiles,
    kCreateVmDisk,
    kStartVm,
    kLaunchTerminal,
  };

  class Observer {
   public:
    virtual void StateChanged(State state) = 0;
    virtual void Error() = 0;
  };

  virtual ~BruschettaInstaller() = default;

  virtual void Cancel() = 0;
  virtual void Install(std::string vm_name, std::string config_id) = 0;

  virtual const base::GUID& GetDownloadGuid() const = 0;

  virtual void DownloadStarted(
      const std::string& guid,
      download::DownloadParams::StartResult result) = 0;
  virtual void DownloadFailed() = 0;
  virtual void DownloadSucceeded(
      const download::CompletionInfo& completion_info) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 private:
};

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_H_

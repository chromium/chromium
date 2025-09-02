// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DAPPNET_IPFS_CONTROLLER_H_
#define CHROME_BROWSER_DAPPNET_IPFS_CONTROLLER_H_

#include "chrome/browser/dappnet/process_controller.h"

#include "base/files/file_path.h"

namespace dappnet {

// Controller for managing the IPFS daemon process.
class IpfsController : public ProcessController {
 public:
  IpfsController();
  ~IpfsController() override;

  IpfsController(const IpfsController&) = delete;
  IpfsController& operator=(const IpfsController&) = delete;

  // Additional IPFS-specific methods
  int GetApiPort() const { return api_port_; }
  int GetGatewayPort() const { return gateway_port_; }
  int GetPeerCount() const { return peer_count_; }

 protected:
  // ProcessController implementation
  base::CommandLine GetCommandLine() override;
  bool VerifyStartup() override;
  int GetDefaultPort() const override;
  base::EnvironmentMap GetEnvironmentVariables() const override;

 private:
  base::FilePath GetIpfsExecutablePath();
  base::FilePath GetIpfsDataPath() const;
  bool CheckIpfsApi();
  void UpdatePeerCount();

  int api_port_ = 5001;
  int gateway_port_ = 8081;
  int peer_count_ = 0;
};

}  // namespace dappnet

#endif  // CHROME_BROWSER_DAPPNET_IPFS_CONTROLLER_H_
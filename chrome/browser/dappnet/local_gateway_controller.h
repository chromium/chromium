// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DAPPNET_LOCAL_GATEWAY_CONTROLLER_H_
#define CHROME_BROWSER_DAPPNET_LOCAL_GATEWAY_CONTROLLER_H_

#include "chrome/browser/dappnet/process_controller.h"

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"

class Profile;

namespace dappnet {

// Controller for managing the local gateway process.
class LocalGatewayController : public ProcessController {
 public:
  explicit LocalGatewayController(Profile* profile);
  ~LocalGatewayController() override;

  LocalGatewayController(const LocalGatewayController&) = delete;
  LocalGatewayController& operator=(const LocalGatewayController&) = delete;

 protected:
  // ProcessController implementation
  base::CommandLine GetCommandLine() override;
  bool VerifyStartup() override;
  int GetDefaultPort() const override;

 private:
  base::FilePath GetGatewayExecutablePath();
  std::string GetCurrentRpcUrl();
  bool CheckHealthEndpoint();

  raw_ptr<Profile> profile_;
};

}  // namespace dappnet

#endif  // CHROME_BROWSER_DAPPNET_LOCAL_GATEWAY_CONTROLLER_H_
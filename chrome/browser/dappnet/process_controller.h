// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DAPPNET_PROCESS_CONTROLLER_H_
#define CHROME_BROWSER_DAPPNET_PROCESS_CONTROLLER_H_

#include "base/command_line.h"
#include "base/environment.h"
#include "base/process/process.h"

namespace dappnet {

// Base class for managing external processes like local gateway and IPFS.
class ProcessController {
 public:
  ProcessController();
  virtual ~ProcessController();

  ProcessController(const ProcessController&) = delete;
  ProcessController& operator=(const ProcessController&) = delete;

  // Process control methods
  bool Start();
  bool Stop();
  bool Restart();
  bool IsRunning() const;
  int GetPid() const;
  int GetPort() const { return port_; }

 protected:
  // Subclasses must implement these methods
  virtual base::CommandLine GetCommandLine() = 0;
  virtual bool VerifyStartup() = 0;
  virtual int GetDefaultPort() const = 0;
  
  // Subclasses can override to set environment variables
  virtual base::EnvironmentMap GetEnvironmentVariables() const;

  void SetPort(int port) { port_ = port; }

 private:
  base::Process process_;
  int port_ = 0;
};

}  // namespace dappnet

#endif  // CHROME_BROWSER_DAPPNET_PROCESS_CONTROLLER_H_
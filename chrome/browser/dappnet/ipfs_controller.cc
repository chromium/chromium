// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dappnet/ipfs_controller.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/common/chrome_paths.h"

namespace dappnet {

namespace {
constexpr int kDefaultApiPort = 5001;
constexpr int kDefaultGatewayPort = 8081;
constexpr int kApiCheckTimeoutMs = 200;
constexpr int kMaxApiCheckRetries = 30;
}  // namespace

IpfsController::IpfsController() {
  api_port_ = kDefaultApiPort;
  gateway_port_ = kDefaultGatewayPort;
  SetPort(api_port_);  // Use API port as the primary port
}

IpfsController::~IpfsController() = default;

base::CommandLine IpfsController::GetCommandLine() {
  base::FilePath exe_path = GetIpfsExecutablePath();
  base::CommandLine cmd(exe_path);
  
  // IPFS daemon command
  cmd.AppendArg("daemon");
  
  // Configure ports
  cmd.AppendArg("--api");
  cmd.AppendArg("/ip4/127.0.0.1/tcp/" + base::NumberToString(api_port_));
  
  cmd.AppendArg("--gateway");
  cmd.AppendArg("/ip4/127.0.0.1/tcp/" + base::NumberToString(gateway_port_));
  
  // Enable writeable gateway for local use
  cmd.AppendArg("--writable");
  
  return cmd;
}

bool IpfsController::VerifyStartup() {
  // Poll the IPFS API to verify the daemon started successfully
  for (int i = 0; i < kMaxApiCheckRetries; i++) {
    if (CheckIpfsApi()) {
      UpdatePeerCount();
      return true;
    }
    base::PlatformThread::Sleep(base::Milliseconds(kApiCheckTimeoutMs));
  }
  
  LOG(ERROR) << "IPFS API check failed after " << kMaxApiCheckRetries 
             << " retries";
  return false;
}

int IpfsController::GetDefaultPort() const {
  return kDefaultApiPort;
}

base::EnvironmentMap IpfsController::GetEnvironmentVariables() const {
  base::EnvironmentMap env;
  base::FilePath ipfs_path = GetIpfsDataPath();
  env["IPFS_PATH"] = ipfs_path.AsUTF8Unsafe();
  return env;
}

base::FilePath IpfsController::GetIpfsExecutablePath() {
  base::FilePath exe_dir;
  
#if BUILDFLAG(IS_WIN)
  if (!base::PathService::Get(base::DIR_EXE, &exe_dir)) {
    LOG(ERROR) << "Failed to get executable directory";
    return base::FilePath();
  }
  return exe_dir.AppendASCII("ipfs.exe");
#else
  if (!base::PathService::Get(base::DIR_EXE, &exe_dir)) {
    LOG(ERROR) << "Failed to get executable directory";
    return base::FilePath();
  }
  return exe_dir.AppendASCII("ipfs");
#endif
}

base::FilePath IpfsController::GetIpfsDataPath() const {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    LOG(ERROR) << "Failed to get user data directory";
    return base::FilePath();
  }
  return user_data_dir.AppendASCII("ipfs");
}

bool IpfsController::CheckIpfsApi() {
  // In a real implementation, this would make an HTTP request to
  // http://localhost:api_port/api/v0/version or similar endpoint
  // to check if IPFS daemon is responding
  // For now, we'll simulate this check
  
  if (!IsRunning()) {
    return false;
  }
  
  // TODO: Implement actual IPFS API check
  // This would involve creating a SimpleURLLoader to check the API endpoint
  // For now, assume the process is healthy if it's running
  return true;
}

void IpfsController::UpdatePeerCount() {
  // In a real implementation, this would query the IPFS API to get
  // the current peer count via /api/v0/swarm/peers
  // For now, simulate a reasonable peer count
  peer_count_ = 12;  // Placeholder value
}

}  // namespace dappnet
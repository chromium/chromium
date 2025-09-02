// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dappnet/local_gateway_controller.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/prefs/pref_service.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace dappnet {

namespace {
constexpr int kDefaultGatewayPort = 8080;
constexpr int kHealthCheckTimeoutMs = 100;
constexpr int kMaxHealthCheckRetries = 30;
}  // namespace

LocalGatewayController::LocalGatewayController(Profile* profile)
    : profile_(profile) {
  SetPort(kDefaultGatewayPort);
}

LocalGatewayController::~LocalGatewayController() = default;

base::CommandLine LocalGatewayController::GetCommandLine() {
  base::FilePath exe_path = GetGatewayExecutablePath();
  base::CommandLine cmd(exe_path);
  
  // Add command line arguments
  cmd.AppendSwitchASCII("--port", base::NumberToString(GetPort()));
  
  std::string rpc_url = GetCurrentRpcUrl();
  if (!rpc_url.empty()) {
    cmd.AppendSwitchASCII("--rpc", rpc_url);
  }
  
  // Add other necessary flags
  cmd.AppendSwitch("--quiet");  // Reduce log output
  
  return cmd;
}

bool LocalGatewayController::VerifyStartup() {
  // Poll the health endpoint to verify the gateway started successfully
  for (int i = 0; i < kMaxHealthCheckRetries; i++) {
    if (CheckHealthEndpoint()) {
      return true;
    }
    base::PlatformThread::Sleep(base::Milliseconds(kHealthCheckTimeoutMs));
  }
  
  LOG(ERROR) << "Gateway health check failed after " << kMaxHealthCheckRetries 
             << " retries";
  return false;
}

int LocalGatewayController::GetDefaultPort() const {
  return kDefaultGatewayPort;
}

base::FilePath LocalGatewayController::GetGatewayExecutablePath() {
  base::FilePath exe_dir;
  
#if BUILDFLAG(IS_WIN)
  if (!base::PathService::Get(base::DIR_EXE, &exe_dir)) {
    LOG(ERROR) << "Failed to get executable directory";
    return base::FilePath();
  }
  return exe_dir.AppendASCII("local-gateway.exe");
#else
  if (!base::PathService::Get(base::DIR_EXE, &exe_dir)) {
    LOG(ERROR) << "Failed to get executable directory";
    return base::FilePath();
  }
  return exe_dir.AppendASCII("local-gateway");
#endif
}

std::string LocalGatewayController::GetCurrentRpcUrl() {
  if (!profile_) {
    return "https://mainnet.infura.io/v3/YOUR-PROJECT-ID";  // Default fallback
  }
  
  PrefService* prefs = profile_->GetPrefs();
  const base::Value::List& endpoints = prefs->GetList("dappnet.rpc_endpoints");
  
  // Find the default RPC endpoint
  for (const auto& value : endpoints) {
    const base::Value::Dict* endpoint = value.GetIfDict();
    if (endpoint && endpoint->FindBool("is_default").value_or(false)) {
      const std::string* url = endpoint->FindString("url");
      return url ? *url : "";
    }
  }
  
  // If no default is set, use the first available endpoint
  if (!endpoints.empty()) {
    const base::Value::Dict* first_endpoint = endpoints[0].GetIfDict();
    if (first_endpoint) {
      const std::string* url = first_endpoint->FindString("url");
      return url ? *url : "";
    }
  }
  
  // Fallback to a default RPC URL
  return "https://mainnet.infura.io/v3/YOUR-PROJECT-ID";
}

bool LocalGatewayController::CheckHealthEndpoint() {
  // In a real implementation, this would make an HTTP request to
  // http://localhost:port/health to check if the gateway is responding
  // For now, we'll simulate this check
  
  if (!IsRunning()) {
    return false;
  }
  
  // TODO: Implement actual HTTP health check
  // This would involve creating a SimpleURLLoader to check the health endpoint
  // For now, assume the process is healthy if it's running
  return true;
}

}  // namespace dappnet
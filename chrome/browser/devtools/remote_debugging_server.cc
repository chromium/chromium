// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/remote_debugging_server.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/expected.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/common/content_switches.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_server_socket.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/public/public_buildflags.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

base::LazyInstance<bool>::Leaky g_tethering_enabled = LAZY_INSTANCE_INITIALIZER;

const uint16_t kMinTetheringPort = 9333;
const uint16_t kMaxTetheringPort = 9444;
const int kBackLog = 10;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DevToolsDebuggingUserDataDirStatus {
  kNotBeingDebugged = 0,
  kDebuggingRequestedWithNonDefaultUserDataDir = 1,
  kDebuggingRequestedWithDefaultUserDataDir = 2,
  kDebuggingRequestedErrorObtainingUserDataDir = 3,

  // New values go above here.
  kMaxValue = kDebuggingRequestedErrorObtainingUserDataDir,
};

class TCPServerSocketFactory
    : public content::DevToolsSocketFactory {
 public:
  explicit TCPServerSocketFactory(uint16_t port)
      : port_(port), last_tethering_port_(kMinTetheringPort) {}

  TCPServerSocketFactory(const TCPServerSocketFactory&) = delete;
  TCPServerSocketFactory& operator=(const TCPServerSocketFactory&) = delete;

 private:
  std::unique_ptr<net::ServerSocket> CreateLocalHostServerSocket(int port) {
    std::unique_ptr<net::ServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLogSource()));
    if (socket->ListenWithAddressAndPort(
            "127.0.0.1", port, kBackLog) == net::OK)
      return socket;
    if (socket->ListenWithAddressAndPort("::1", port, kBackLog) == net::OK)
      return socket;
    return nullptr;
  }

  // content::DevToolsSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    return CreateLocalHostServerSocket(port_);
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* name) override {
    if (!g_tethering_enabled.Get())
      return nullptr;

    if (last_tethering_port_ == kMaxTetheringPort)
      last_tethering_port_ = kMinTetheringPort;
    uint16_t port = ++last_tethering_port_;
    *name = base::NumberToString(port);
    return CreateLocalHostServerSocket(port);
  }

  uint16_t port_;
  uint16_t last_tethering_port_;
};

// Returns true, or a reason why remote debugging is not allowed.
base::expected<bool, RemoteDebuggingServer::NotStartedReason>
IsRemoteDebuggingAllowed(const std::optional<bool>& is_default_user_data_dir,
                         PrefService* local_state) {
  if (!local_state->GetBoolean(prefs::kDevToolsRemoteDebuggingAllowed)) {
    return base::unexpected(
        RemoteDebuggingServer::NotStartedReason::kDisabledByPolicy);
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (base::FeatureList::IsEnabled(features::kDevToolsDebuggingRestrictions) &&
      is_default_user_data_dir.value_or(true)) {
    return base::unexpected(
        RemoteDebuggingServer::NotStartedReason::kDisabledByDefaultUserDataDir);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return true;
}

}  // namespace

RemoteDebuggingServer::RemoteDebuggingServer() = default;

// static
void RemoteDebuggingServer::EnableTetheringForDebug() {
  g_tethering_enabled.Get() = true;
}

// static
base::expected<std::unique_ptr<RemoteDebuggingServer>,
               RemoteDebuggingServer::NotStartedReason>
RemoteDebuggingServer::GetInstance(PrefService* local_state) {
  if (!local_state->GetBoolean(prefs::kDevToolsRemoteDebuggingAllowed)) {
    return base::unexpected(NotStartedReason::kDisabledByPolicy);
  }

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Track whether debugging was requested. This determines the metric reported
  // on function exit.
  bool wanted_debugging = false;
  // Track whether debugging was started. This determines whether or not to
  // return an instance of the class.
  bool being_debugged = false;
  std::optional<bool> is_default_user_data_dir =
      chrome::IsUsingDefaultDataDirectory();

  absl::Cleanup record_histogram = [&wanted_debugging,
                                    &is_default_user_data_dir] {
    DevToolsDebuggingUserDataDirStatus status =
        DevToolsDebuggingUserDataDirStatus::kNotBeingDebugged;
    if (wanted_debugging) {
      status = DevToolsDebuggingUserDataDirStatus::
          kDebuggingRequestedErrorObtainingUserDataDir;
      if (is_default_user_data_dir.has_value()) {
        status = is_default_user_data_dir.value()
                     ? DevToolsDebuggingUserDataDirStatus::
                           kDebuggingRequestedWithDefaultUserDataDir
                     : DevToolsDebuggingUserDataDirStatus::
                           kDebuggingRequestedWithNonDefaultUserDataDir;
      }
    }
    base::UmaHistogramEnumeration("DevTools.DevToolsDebuggingUserDataDirStatus",
                                  status);
  };

  if (command_line.HasSwitch(switches::kRemoteDebuggingPipe)) {
    wanted_debugging = true;
    if (const auto maybe_allow_debugging =
            IsRemoteDebuggingAllowed(is_default_user_data_dir, local_state);
        !maybe_allow_debugging.has_value()) {
      return base::unexpected(maybe_allow_debugging.error());
    }
    being_debugged = true;
    content::DevToolsAgentHost::StartRemoteDebuggingPipeHandler(
        base::BindOnce(&ChromeDevToolsManagerDelegate::CloseBrowserSoon));
  }

  std::string port_str =
      command_line.GetSwitchValueASCII(::switches::kRemoteDebuggingPort);
  int port;
  if (base::StringToInt(port_str, &port) && port >= 0 && port < 65535) {
    base::FilePath output_dir;
    if (!port) {
      // The client requested an ephemeral port. Must write the selected
      // port to a well-known location in the profile directory to
      // bootstrap the connection process.
      bool result = base::PathService::Get(chrome::DIR_USER_DATA, &output_dir);
      DCHECK(result);
    }

    base::FilePath debug_frontend_dir;
    if (command_line.HasSwitch(::switches::kCustomDevtoolsFrontend)) {
      GURL custom_devtools_frontend_url(command_line.GetSwitchValueASCII(
          ::switches::kCustomDevtoolsFrontend));
      if (custom_devtools_frontend_url.SchemeIsFile()) {
        net::FileURLToFilePath(custom_devtools_frontend_url,
                               &debug_frontend_dir);
      }
    }
    wanted_debugging = true;
    if (const auto maybe_allow_debugging =
            IsRemoteDebuggingAllowed(is_default_user_data_dir, local_state);
        !maybe_allow_debugging.has_value()) {
      return base::unexpected(maybe_allow_debugging.error());
    }
    being_debugged = true;
    content::DevToolsAgentHost::StartRemoteDebuggingServer(
        std::make_unique<TCPServerSocketFactory>(port), output_dir,
        debug_frontend_dir);
  }

  if (being_debugged) {
    return base::WrapUnique(new RemoteDebuggingServer);
  }

  return base::unexpected(NotStartedReason::kNotRequested);
}

RemoteDebuggingServer::~RemoteDebuggingServer() {
  // Ensure Profile is alive, because the whole DevTools subsystem
  // accesses it during shutdown.
  DCHECK(g_browser_process->profile_manager());
  content::DevToolsAgentHost::StopRemoteDebuggingServer();
  content::DevToolsAgentHost::StopRemoteDebuggingPipeHandler();
}

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/remote_debugging_server.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "build/branding_buildflags.h"
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
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_server_socket.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/public/public_buildflags.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

bool g_tethering_enabled = false;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
bool g_enable_default_user_data_dir_check_for_chromium_branding_for_testing =
    false;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

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

// Returns a port if the string is a valid port number, otherwise returns
// nullopt. A valid port is a number between 0 and 65535, inclusive.
std::optional<uint16_t> ParsePort(std::string_view port_str) {
  int port;
  if (base::StringToInt(port_str, &port) && port >= 0 && port <= 65535) {
    return static_cast<uint16_t>(port);
  }
  return std::nullopt;
}

class TCPServerSocketFactory
    : public content::DevToolsSocketFactory {
 public:
  explicit TCPServerSocketFactory(uint16_t port)
      : port_(port), last_tethering_port_(kMinTetheringPort) {}

  TCPServerSocketFactory(const TCPServerSocketFactory&) = delete;
  TCPServerSocketFactory& operator=(const TCPServerSocketFactory&) = delete;

 protected:
  // content::DevToolsSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    return CreateLocalHostServerSocket(port_);
  }
  uint16_t port_;

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

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* name) override {
    if (!g_tethering_enabled) {
      return nullptr;
    }

    if (last_tethering_port_ == kMaxTetheringPort)
      last_tethering_port_ = kMinTetheringPort;
    uint16_t port = ++last_tethering_port_;
    *name = base::NumberToString(port);
    return CreateLocalHostServerSocket(port);
  }

  uint16_t last_tethering_port_;
};

// Creates a server socket on a specific port, or any available port if the port
// is busy. Prefers a free port over switching from IPv4 to IPv6.
class TCPServerSocketFactoryWithPortFallback : public TCPServerSocketFactory {
 public:
  using TCPServerSocketFactory::TCPServerSocketFactory;

 private:
  std::unique_ptr<net::ServerSocket> CreateSocketOnAddress(const char* address,
                                                           uint16_t port) {
    auto socket =
        std::make_unique<net::TCPServerSocket>(nullptr, net::NetLogSource());
    if (socket->ListenWithAddressAndPort(address, port, kBackLog) == net::OK) {
      return socket;
    }
    return nullptr;
  }

  // content::DevToolsSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::ServerSocket> socket =
        CreateSocketOnAddress("127.0.0.1", port_);
    if (socket) {
      return socket;
    }

    if (port_ != 0) {
      socket = CreateSocketOnAddress("127.0.0.1", 0);
      if (socket) {
        return socket;
      }
    }

    socket = CreateSocketOnAddress("::1", port_);
    if (socket) {
      return socket;
    }

    if (port_ != 0) {
      socket = CreateSocketOnAddress("::1", 0);
      if (socket) {
        return socket;
      }
    }
    return nullptr;
  }
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
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  constexpr bool default_user_data_dir_check_enabled = true;
#else
  const bool default_user_data_dir_check_enabled =
      g_enable_default_user_data_dir_check_for_chromium_branding_for_testing;
#endif

  if (default_user_data_dir_check_enabled &&
      is_default_user_data_dir.value_or(true)) {
    return base::unexpected(
        RemoteDebuggingServer::NotStartedReason::kDisabledByDefaultUserDataDir);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return true;
}

}  // namespace

int RemoteDebuggingServer::GetPortFromUserDataDir(
    const base::FilePath& output_dir) {
  std::string content;
  if (!base::ReadFileToString(
          output_dir.Append(content::kDevToolsActivePortFileName), &content)) {
    return kDefaultDevToolsPort;
  }

  // The file contains the port number on the first line.
  std::vector<std::string_view> lines = base::SplitStringPiece(
      content, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (lines.empty()) {
    return kDefaultDevToolsPort;
  }

  if (auto port = ParsePort(lines[0])) {
    return *port;
  }

  return kDefaultDevToolsPort;
}

void RemoteDebuggingServer::StartHttpServerInApprovalMode(
    PrefService* local_state) {
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(local_state);
  pref_change_registrar_->Add(
      prefs::kDevToolsRemoteDebuggingEnabled,
      base::BindRepeating(
          &RemoteDebuggingServer::MaybeStartOrStopServerForPrefChange,
          base::Unretained(this)));
  MaybeStartOrStopServerForPrefChange();
}

void RemoteDebuggingServer::StartHttpServerInApprovalModeWithPort(
    const base::FilePath& output_dir,
    int port) {
  is_http_server_being_started_ = false;

  // Recheck the pref value in case it changed since we posted the task.
  if (!pref_change_registrar_->prefs()->GetBoolean(
          prefs::kDevToolsRemoteDebuggingEnabled)) {
    return;
  }

  // We do not support hosting DevTools in this mode, therefore,
  // not passing the value of the kCustomDevtoolsFrontend switch.
  StartHttpServer(
      std::make_unique<TCPServerSocketFactoryWithPortFallback>(port),
      output_dir,
      /*debug_frontend_dir=*/base::FilePath(),
      content::DevToolsAgentHost::RemoteDebuggingServerMode::kWithApprovalOnly);
  is_http_server_running_ = true;
}

void RemoteDebuggingServer::MaybeStartOrStopServerForPrefChange() {
  CHECK(base::FeatureList::IsEnabled(
      ::features::kDevToolsAcceptDebuggingConnections));

  PrefService* local_state = pref_change_registrar_->prefs();

  // In case the policy is changed after the server was started somehow.
  if (!local_state->GetBoolean(prefs::kDevToolsRemoteDebuggingAllowed)) {
    StopHttpServer();
    is_http_server_running_ = false;
    return;
  }

  // Latest chrome://inspect page preference value.
  if (!local_state->GetBoolean(prefs::kDevToolsRemoteDebuggingEnabled)) {
    StopHttpServer();
    is_http_server_running_ = false;
    return;
  }

  if (is_http_server_running_ || is_http_server_being_started_) {
    return;
  }

  // The selected port is written to a well-known location in the profile
  // directory to bootstrap the connection process.
  base::FilePath output_dir;
  {
    bool result = base::PathService::Get(chrome::DIR_USER_DATA, &output_dir);
    DCHECK(result);
  }

  is_http_server_being_started_ = true;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&RemoteDebuggingServer::GetPortFromUserDataDir,
                     output_dir),
      base::BindOnce(
          &RemoteDebuggingServer::StartHttpServerInApprovalModeWithPort,
          weak_factory_.GetWeakPtr(), output_dir));
}

// static
void RemoteDebuggingServer::EnableTetheringForDebug() {
  g_tethering_enabled = true;
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// static
void RemoteDebuggingServer::EnableDefaultUserDataDirCheckForTesting() {
  g_enable_default_user_data_dir_check_for_chromium_branding_for_testing = true;
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

RemoteDebuggingServer::RemoteDebuggingServer() = default;

RemoteDebuggingServer::~RemoteDebuggingServer() {
  // Ensure Profile is alive, because the whole DevTools subsystem
  // accesses it during shutdown.
  DCHECK(g_browser_process->profile_manager());
  StopHttpServer();
  StopPipeHandler();
}

void RemoteDebuggingServer::StartHttpServer(
    std::unique_ptr<content::DevToolsSocketFactory> factory,
    const base::FilePath& output_dir,
    const base::FilePath& debug_frontend_dir,
    content::DevToolsAgentHost::RemoteDebuggingServerMode mode) {
  content::DevToolsAgentHost::StartRemoteDebuggingServer(
      std::move(factory), output_dir, debug_frontend_dir, mode);
}

void RemoteDebuggingServer::StopHttpServer() {
  content::DevToolsAgentHost::StopRemoteDebuggingServer();
}

void RemoteDebuggingServer::StartPipeHandler() {
  content::DevToolsAgentHost::StartRemoteDebuggingPipeHandler(
      base::BindOnce(&ChromeDevToolsManagerDelegate::CloseBrowserSoon));
}

void RemoteDebuggingServer::StopPipeHandler() {
  content::DevToolsAgentHost::StopRemoteDebuggingPipeHandler();
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

  auto server = base::WrapUnique(new RemoteDebuggingServer());

  if (command_line.HasSwitch(switches::kRemoteDebuggingPipe)) {
    wanted_debugging = true;
    if (const auto maybe_allow_debugging =
            IsRemoteDebuggingAllowed(is_default_user_data_dir, local_state);
        !maybe_allow_debugging.has_value()) {
      return base::unexpected(maybe_allow_debugging.error());
    }
    being_debugged = true;
    server->StartPipeHandler();
  }

  std::string port_str =
      command_line.GetSwitchValueASCII(::switches::kRemoteDebuggingPort);
  if (auto port = ParsePort(port_str)) {
    base::FilePath output_dir;
    if (*port == 0) {
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
    server->StartHttpServer(
        std::make_unique<TCPServerSocketFactory>(*port), output_dir,
        debug_frontend_dir,
        content::DevToolsAgentHost::RemoteDebuggingServerMode::kDefault);
  }

  // `--remote-debugging-port` and `--remote-debugging-pipe`
  // take precedence over the new mode.
#if !BUILDFLAG(IS_ANDROID)
  if (!being_debugged && base::FeatureList::IsEnabled(
                             ::features::kDevToolsAcceptDebuggingConnections)) {
    wanted_debugging = true;
    if (!local_state->GetBoolean(prefs::kDevToolsRemoteDebuggingAllowed)) {
      return base::unexpected(
          RemoteDebuggingServer::NotStartedReason::kDisabledByPolicy);
    }
    being_debugged = true;
    server->StartHttpServerInApprovalMode(local_state);
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  if (being_debugged) {
    return server;
  }

  return base::unexpected(NotStartedReason::kNotRequested);
}

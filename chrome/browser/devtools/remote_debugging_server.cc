// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/remote_debugging_server.h"

#include <utility>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/common/content_switches.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_server_socket.h"
#include "third_party/blink/public/public_buildflags.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

base::LazyInstance<bool>::Leaky g_tethering_enabled = LAZY_INSTANCE_INITIALIZER;

const uint16_t kMinTetheringPort = 9333;
const uint16_t kMaxTetheringPort = 9444;
const int kBackLog = 10;

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

}  // namespace

// static
void RemoteDebuggingServer::EnableTetheringForDebug() {
  g_tethering_enabled.Get() = true;
}

RemoteDebuggingServer::RemoteDebuggingServer() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kRemoteDebuggingPipe)) {
    content::DevToolsAgentHost::StartRemoteDebuggingPipeHandler(
        base::BindOnce(&ChromeDevToolsManagerDelegate::CloseBrowserSoon));
  }

  std::string port_str =
      command_line.GetSwitchValueASCII(::switches::kRemoteDebuggingPort);
  int port;
  if (!base::StringToInt(port_str, &port) || port < 0 || port >= 65535)
    return;

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
      net::FileURLToFilePath(custom_devtools_frontend_url, &debug_frontend_dir);
    }
  }
  content::DevToolsAgentHost::StartRemoteDebuggingServer(
      std::make_unique<TCPServerSocketFactory>(port), output_dir,
      debug_frontend_dir);
}

RemoteDebuggingServer::~RemoteDebuggingServer() {
  // Ensure Profile is alive, because the whole DevTools subsystem
  // accesses it during shutdown.
  DCHECK(g_browser_process->profile_manager());
  content::DevToolsAgentHost::StopRemoteDebuggingServer();
  content::DevToolsAgentHost::StopRemoteDebuggingPipeHandler();
}

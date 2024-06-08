// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_devtools_server.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/public/browser/android/devtools_auth.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/common/content_switches.h"
#include "net/base/net_errors.h"
#include "net/socket/tcp_server_socket.h"
#include "net/socket/unix_domain_server_socket_posix.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwDevToolsServer_jni.h"

using base::android::JavaParamRef;
using content::DevToolsAgentHost;

namespace {

const char kSocketNameFormat[] = "webview_devtools_remote_%d";
const char kTetheringSocketName[] = "webview_devtools_tethering_%d_%d";

const int kBackLog = 10;

bool g_is_debugging_started_ = false;

// Factory for UnixDomainServerSocket.
class UnixDomainServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  explicit UnixDomainServerSocketFactory(const std::string& socket_name)
      : socket_name_(socket_name), last_tethering_socket_(0) {}

  UnixDomainServerSocketFactory(const UnixDomainServerSocketFactory&) = delete;
  UnixDomainServerSocketFactory& operator=(
      const UnixDomainServerSocketFactory&) = delete;

 private:
  // content::DevToolsAgentHost::ServerSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::UnixDomainServerSocket> socket(
        new net::UnixDomainServerSocket(
            base::BindRepeating(&content::CanUserConnectToDevTools),
            true /* use_abstract_namespace */));
    if (socket->BindAndListen(socket_name_, kBackLog) != net::OK)
      return nullptr;

    return std::move(socket);
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* name) override {
    *name = base::StringPrintf(kTetheringSocketName, getpid(),
                               ++last_tethering_socket_);
    std::unique_ptr<net::UnixDomainServerSocket> socket(
        new net::UnixDomainServerSocket(
            base::BindRepeating(&content::CanUserConnectToDevTools),
            true /* use_abstract_namespace */));
    if (socket->BindAndListen(*name, kBackLog) != net::OK)
      return nullptr;

    return std::move(socket);
  }

  std::string socket_name_;
  int last_tethering_socket_;
};

class TCPServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  TCPServerSocketFactory(const std::string& address, uint16_t port)
      : address_(address), port_(port) {}

  TCPServerSocketFactory(const TCPServerSocketFactory&) = delete;
  TCPServerSocketFactory& operator=(const TCPServerSocketFactory&) = delete;

 private:
  // content::DevToolsSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::ServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLogSource()));
    if (socket->ListenWithAddressAndPort(address_, port_, kBackLog) != net::OK)
      return nullptr;

    net::IPEndPoint endpoint;
    return socket;
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }

  std::string address_;
  uint16_t port_;
};

std::unique_ptr<content::DevToolsSocketFactory> CreateSocketFactory() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kRemoteDebuggingPort)) {
    uint16_t port = 0;
    int temp_port;
    std::string port_str =
        command_line.GetSwitchValueASCII(switches::kRemoteDebuggingPort);
    if (base::StringToInt(port_str, &temp_port) && temp_port >= 1024 &&
        temp_port < 65535) {
      port = static_cast<uint16_t>(temp_port);
    } else {
      DLOG(WARNING) << "Invalid http debugger port number " << temp_port;
    }
    return std::make_unique<TCPServerSocketFactory>("127.0.0.1", port);
  }

  return std::make_unique<UnixDomainServerSocketFactory>(
      base::StringPrintf(kSocketNameFormat, getpid()));
}

}  // namespace

namespace android_webview {

void StartAwDevToolsServer() {
  if (g_is_debugging_started_) {
    return;
  }
  g_is_debugging_started_ = true;

  DevToolsAgentHost::StartRemoteDebuggingServer(
      CreateSocketFactory(), base::FilePath(), base::FilePath());
}

void StopAwDevToolsServer() {
  DevToolsAgentHost::StopRemoteDebuggingServer();
  g_is_debugging_started_ = false;
}

bool IsAwDevToolsServerStarted() {
  return g_is_debugging_started_;
}

static void JNI_AwDevToolsServer_SetRemoteDebuggingEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean enabled) {
  if (enabled) {
    StartAwDevToolsServer();
  } else {
    StopAwDevToolsServer();
  }
}

}  // namespace android_webview

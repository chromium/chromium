// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/tcp_device_provider.h"

#include <utility>

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/device/adb/adb_client_socket.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/tcp_client_socket.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace {

const char kDeviceModel[] = "Remote Target";
const char kBrowserName[] = "Target";

static void RunSocketCallback(
    const AndroidDeviceManager::SocketCallback& callback,
    std::unique_ptr<net::StreamSocket> socket,
    int result) {
  callback.Run(result, std::move(socket));
}

class ResolveHostAndOpenSocket final
    : public network::mojom::ResolveHostClient {
 public:
  ResolveHostAndOpenSocket(const net::HostPortPair& address,
                           const AdbClientSocket::SocketCallback& callback,
                           network::mojom::HostResolverPtr* host_resolver)
      : callback_(callback), binding_(this) {
    network::mojom::ResolveHostClientPtr client_ptr;
    binding_.Bind(mojo::MakeRequest(&client_ptr));
    binding_.set_connection_error_handler(
        base::BindOnce(&ResolveHostAndOpenSocket::OnComplete,
                       base::Unretained(this), net::ERR_FAILED, base::nullopt));

    (*host_resolver)->ResolveHost(address, nullptr, std::move(client_ptr));
  }

 private:
  // network::mojom::ResolveHostClient implementation:
  void OnComplete(
      int result,
      const base::Optional<net::AddressList>& resolved_addresses) override {
    if (result != net::OK) {
      RunSocketCallback(callback_, nullptr, result);
      delete this;
      return;
    }
    std::unique_ptr<net::StreamSocket> socket(new net::TCPClientSocket(
        resolved_addresses.value(), nullptr, nullptr, net::NetLogSource()));
    net::StreamSocket* socket_ptr = socket.get();
    net::CompletionCallback on_connect =
        base::Bind(&RunSocketCallback, callback_, base::Passed(&socket));
    result = socket_ptr->Connect(on_connect);
    if (result != net::ERR_IO_PENDING)
      on_connect.Run(result);
    delete this;
  }

  AdbClientSocket::SocketCallback callback_;
  mojo::Binding<network::mojom::ResolveHostClient> binding_;
};

}  // namespace

scoped_refptr<TCPDeviceProvider> TCPDeviceProvider::CreateForLocalhost(
    uint16_t port) {
  TCPDeviceProvider::HostPortSet targets;
  targets.insert(net::HostPortPair("127.0.0.1", port));
  return new TCPDeviceProvider(targets);
}

TCPDeviceProvider::TCPDeviceProvider(const HostPortSet& targets)
    : targets_(targets) {
}

void TCPDeviceProvider::QueryDevices(const SerialsCallback& callback) {
  std::vector<std::string> result;
  for (const net::HostPortPair& target : targets_) {
    const std::string& host = target.host();
    if (base::ContainsValue(result, host))
      continue;
    result.push_back(host);
  }
  callback.Run(result);
}

void TCPDeviceProvider::QueryDeviceInfo(const std::string& serial,
                                        const DeviceInfoCallback& callback) {
  AndroidDeviceManager::DeviceInfo device_info;
  device_info.model = kDeviceModel;
  device_info.connected = true;

  for (const net::HostPortPair& target : targets_) {
    if (serial != target.host())
      continue;
    AndroidDeviceManager::BrowserInfo browser_info;
    browser_info.socket_name = base::UintToString(target.port());
    browser_info.display_name = kBrowserName;
    browser_info.type = AndroidDeviceManager::BrowserInfo::kTypeChrome;

    device_info.browser_info.push_back(browser_info);
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(callback, device_info));
}

void TCPDeviceProvider::OpenSocket(const std::string& serial,
                                   const std::string& socket_name,
                                   const SocketCallback& callback) {
  // Use plain socket for remote debugging and port forwarding on Desktop
  // (debugging purposes).
  int port;
  base::StringToInt(socket_name, &port);
  net::HostPortPair host_port(serial, port);

  // OpenSocket() is run on the devtools ADB thread, while TCPDeviceProvider is
  // created on the UI thread, so do any initialization of |host_resolver_|
  // here.
  if (!host_resolver_) {
    InitializeHostResolver();
  }
  new ResolveHostAndOpenSocket(host_port, callback, &host_resolver_);
}

void TCPDeviceProvider::ReleaseDevice(const std::string& serial) {
  if (!release_callback_.is_null())
    release_callback_.Run();
}

void TCPDeviceProvider::set_release_callback_for_test(
    const base::Closure& callback) {
  release_callback_ = callback;
}

TCPDeviceProvider::~TCPDeviceProvider() {
}

void TCPDeviceProvider::InitializeHostResolver() {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&TCPDeviceProvider::InitializeHostResolverOnUI, this,
                     mojo::MakeRequest(&host_resolver_)));
  host_resolver_.set_connection_error_handler(base::BindOnce(
      &TCPDeviceProvider::InitializeHostResolver, base::Unretained(this)));
}

void TCPDeviceProvider::InitializeHostResolverOnUI(
    network::mojom::HostResolverRequest request) {
  g_browser_process->system_network_context_manager()
      ->GetContext()
      ->CreateHostResolver(base::nullopt, std::move(request));
}

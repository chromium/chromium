// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_MACH_BOOTSTRAP_ACCEPTOR_H_
#define CHROME_BROWSER_APPS_APP_SHIM_MACH_BOOTSTRAP_ACCEPTOR_H_

#include <memory>
#include <string>

#include "base/apple/dispatch_source_mach.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process_handle.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"

namespace apps {

// A simple Mach message server published in the system bootstrap namespace.
// When an app shim client sends a message, the server creates a
// mojo::PlatformChannelEndpoint from the Mach port specified in the
// msgh_remote_port (the Mach reply port) and passes it to its Delegate. The
// delegate then uses this to initialize a Mojo IPC channel.
class MachBootstrapAcceptor {
 public:
  class Delegate {
   public:
    // Called when a client identified by |audit_token| connects with the
    // Mach port it provided in |endpoint|.
    virtual void OnClientConnected(mojo::PlatformChannelEndpoint endpoint,
                                   audit_token_t audit_token) = 0;

    // Called when there is an error creating the server channel.
    virtual void OnServerChannelCreateError() = 0;
  };

  // Initializes the server by specifying the |name_fragment|, which will be
  // appended to the running process's bundle identifier, to be published in
  // the bootstrap server.
  MachBootstrapAcceptor(const std::string& name_fragment, Delegate* delegate);
  MachBootstrapAcceptor(const MachBootstrapAcceptor&) = delete;
  MachBootstrapAcceptor& operator=(const MachBootstrapAcceptor&) = delete;
  ~MachBootstrapAcceptor();

  // Creates a Mach receive port and publishes a send right to it in the system
  // bootstrap namespace. Clients will then be able to send messages to this
  // server.
  void Start();

  // Stops listening for client messages and un-publishes the server from the
  // bootstrap namespace.
  void Stop();

 private:
  friend class MachBootstrapAcceptorTest;

  // Called by |dispatch_source_| when a Mach message is ready to be received
  // on |endpoint_|.
  void HandleRequest();

  mach_port_t port();

  mojo::NamedPlatformChannel::ServerName server_name_;
  raw_ptr<Delegate, AcrossTasksDanglingUntriaged> delegate_;
  mojo::PlatformChannelServerEndpoint endpoint_;
  std::unique_ptr<base::apple::DispatchSourceMach> dispatch_source_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SHIM_MACH_BOOTSTRAP_ACCEPTOR_H_

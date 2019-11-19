// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_MACH_PORT_RENDEZVOUS_H_
#define BASE_MAC_MACH_PORT_RENDEZVOUS_H_

#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <stdint.h>
#include <sys/types.h>

#include <map>
#include <memory>
#include <string>

#include "base/base_export.h"
#include "base/mac/dispatch_source_mach.h"
#include "base/mac/scoped_dispatch_object.h"
#include "base/mac/scoped_mach_port.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"

namespace base {

// Mach Port Rendezvous is a technique to exchange Mach port rights across
// child process creation. macOS does not provide a way to inherit Mach port
// rights, unlike what is possible with file descriptors. Port rendezvous
// enables a parent process to register Mach port rights for a nascent child,
// which the child can then retrieve using Mach IPC by looking up the endpoint
// in launchd's bootstrap namespace.
//
// When launching a child process, the parent process' rendezvous server lets
// calling code register a collection of ports for the new child. In order to
// acquire the ports, a child looks up the rendezvous server in the bootstrap
// namespace, and it sends an IPC message to the server, the reply to which
// contains the registered ports.
//
// Port rendezvous is only permitted between a parent and its direct child
// process descendants.

// A MachRendezvousPort contains a single Mach port to pass to the child
// process. The associated disposition controls how the reference count will
// be manipulated.
class BASE_EXPORT MachRendezvousPort {
 public:
  MachRendezvousPort() = default;
  // Creates a rendezvous port that allows specifying the specific disposition.
  MachRendezvousPort(mach_port_t name, mach_msg_type_name_t disposition);
  // Creates a rendezvous port for MACH_MSG_TYPE_MOVE_SEND.
  explicit MachRendezvousPort(mac::ScopedMachSendRight send_right);
  // Creates a rendezvous port for MACH_MSG_TYPE_MOVE_RECEIVE.
  explicit MachRendezvousPort(mac::ScopedMachReceiveRight receive_right);

  // Note that the destructor does not call Destroy() explicitly.
  // To avoid leaking ports, either use dispositions that create rights during
  // transit (MAKE or COPY), or use base::LaunchProcess, which will destroy
  // rights on failure.
  ~MachRendezvousPort();

  // Destroys the Mach port right type conveyed |disposition| named by |name|.
  void Destroy();

  mach_port_t name() const { return name_; }

  mach_msg_type_name_t disposition() const { return disposition_; }

 private:
  mach_port_t name_ = MACH_PORT_NULL;
  mach_msg_type_name_t disposition_ = 0;
  // Copy and assign allowed.
};

// The collection of ports to pass to a child process. There are no restrictions
// regarding the keys of the map. Clients are responsible for avoiding
// collisions with other clients.
using MachPortsForRendezvous = std::map<uint32_t, MachRendezvousPort>;

// Class that runs a Mach message server, from which client processes can
// acquire Mach port rights registered for them.
class BASE_EXPORT MachPortRendezvousServer {
 public:
  // Returns the instance of the server. Upon the first call to this method,
  // the server is created, which registers an endpoint in the Mach bootstrap
  // namespace.
  static MachPortRendezvousServer* GetInstance();

  // Registers a collection of Mach ports |ports| to be acquirable by the
  // process known by |pid|. This cannot be called again for the same |pid|
  // until the process known by |pid| has either acquired the ports or died.
  //
  // This must be called with the lock from GetLock() held.
  void RegisterPortsForPid(pid_t pid, const MachPortsForRendezvous& ports);

  // Returns a lock on the internal port registration map. The parent process
  // should hold this lock for the duration of launching a process, including
  // after calling RegisterPortsForPid(). This ensures that a child process
  // cannot race acquiring ports before they are registered. The lock should
  // be released after the child process is launched and the ports are
  // registered.
  Lock& GetLock() { return lock_; }

 private:
  friend class MachPortRendezvousServerTest;
  friend struct MachPortRendezvousFuzzer;

  struct ClientData {
    ClientData(ScopedDispatchObject<dispatch_source_t> exit_watcher,
               MachPortsForRendezvous ports);
    ClientData(ClientData&&);
    ~ClientData();

    // A DISPATCH_SOURCE_TYPE_PROC / DISPATCH_PROC_EXIT dispatch source. When
    // the source is triggered, it calls OnClientExited().
    ScopedDispatchObject<dispatch_source_t> exit_watcher;

    MachPortsForRendezvous ports;
  };

  MachPortRendezvousServer();
  ~MachPortRendezvousServer();

  // The server-side Mach message handler. Called by |dispatch_source_| when a
  // message is received.
  void HandleRequest();

  // Returns the registered collection of ports for the specified |pid|. An
  // empty collection indicates no ports were found, as it is invalid to
  // register with an empty collection. This claims the collection of ports
  // and removes the entry from |client_data_|.
  MachPortsForRendezvous PortsForPid(pid_t pid);

  // Returns a buffer containing a well-formed Mach message, destined for
  // |reply_port| containing descriptors for the specified |ports|.
  std::unique_ptr<uint8_t[]> CreateReplyMessage(
      mach_port_t reply_port,
      const MachPortsForRendezvous& ports);

  // Called by the ClientData::exit_watcher dispatch sources when a process
  // for which ports have been registered exits. This releases port rights
  // that are strongly owned, in the event that the child has not claimed them.
  void OnClientExited(pid_t pid);

  // The Mach receive right for the server. A send right to this is port is
  // registered in the bootstrap server.
  mac::ScopedMachReceiveRight server_port_;

  // Mach message dispatch source for |server_port_|.
  std::unique_ptr<DispatchSourceMach> dispatch_source_;

  // Lock that protects |client_data_|.
  Lock lock_;
  // Association of pid-to-ports.
  std::map<pid_t, ClientData> client_data_;

  DISALLOW_COPY_AND_ASSIGN(MachPortRendezvousServer);
};

// Client class for accessing the memory object exposed by the
// MachPortRendezvousServer.
class BASE_EXPORT MachPortRendezvousClient {
 public:
  // Connects to the MachPortRendezvousServer and requests any registered Mach
  // ports. This only performs the rendezvous once. Subsequent calls to this
  // method return the same instance. If the rendezvous fails, which can happen
  // if the server is not available, this returns null. Acquiring zero ports
  // from the exchange is not considered a failure.
  static MachPortRendezvousClient* GetInstance();

  // Returns the Mach send right that was registered with |key|. If no such
  // right exists, or it was already taken, returns an invalid right. Safe to
  // call from any thread. DCHECKs if the right referenced by |key| is not a
  // send or send-once right.
  mac::ScopedMachSendRight TakeSendRight(MachPortsForRendezvous::key_type key);

  // Returns the Mach receive right that was registered with |key|. If no such
  // right exists, or it was already taken, returns an invalid right. Safe to
  // call from any thread. DCHECKs if the right referenced by |key| is not a
  // receive right.
  mac::ScopedMachReceiveRight TakeReceiveRight(
      MachPortsForRendezvous::key_type key);

  // Returns the number of ports in the client. After PerformRendezvous(), this
  // reflects the number of ports acquired. But as rights are taken, this
  // only reflects the number of remaining rights.
  size_t GetPortCount();

  // Returns the name of the server to look up in the bootstrap namespace.
  static std::string GetBootstrapName();

 private:
  MachPortRendezvousClient();
  ~MachPortRendezvousClient();

  // Helper method to look up the server in the bootstrap namespace and send
  // the acquisition request message.
  bool AcquirePorts();

  // Sends the actual IPC message to |server_port| and parses the reply.
  bool SendRequest(mac::ScopedMachSendRight server_port);

  // Returns a MachRendezvousPort for a given key and removes it from the
  // |ports_| map. If an entry does not exist for that key, then a
  // MachRendezvousPort with MACH_PORT_NULL is returned.
  MachRendezvousPort PortForKey(MachPortsForRendezvous::key_type key);

  // Lock for the below data members.
  Lock lock_;
  // The collection of ports that was acquired.
  MachPortsForRendezvous ports_;

  DISALLOW_COPY_AND_ASSIGN(MachPortRendezvousClient);
};

}  // namespace base

#endif  // BASE_MAC_MACH_PORT_RENDEZVOUS_H_

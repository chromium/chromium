// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_APPLE_MACH_PORT_RENDEZVOUS_H_
#define BASE_APPLE_MACH_PORT_RENDEZVOUS_H_

#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <stdint.h>
#include <sys/types.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/apple/dispatch_source_mach.h"
#include "base/apple/scoped_mach_port.h"
#include "base/base_export.h"
#include "base/containers/buffer_iterator.h"
#include "base/feature_list.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "build/ios_buildflags.h"

#if BUILDFLAG(IS_MAC)
#include "base/environment.h"
#include "base/mac/process_requirement.h"
#endif

namespace base {

// Mach Port Rendezvous is a technique to exchange Mach port rights across
// child process creation. macOS does not provide a way to inherit Mach port
// rights, unlike what is possible with file descriptors. Port rendezvous
// enables a parent process to register Mach port rights for a nascent child,
// which the child can then retrieve using Mach IPC by looking up the endpoint
// in launchd's bootstrap namespace.
//
// The same mechanism is used on iOS but the Mach IPC endpoint is not found
// via launchd's bootstrap namespace but via an initial XPC connection.
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
  explicit MachRendezvousPort(apple::ScopedMachSendRight send_right);
  // Creates a rendezvous port for MACH_MSG_TYPE_MOVE_RECEIVE.
  explicit MachRendezvousPort(apple::ScopedMachReceiveRight receive_right);

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

// Base class that runs a Mach message server, listening to requests on
// mach server port.
class BASE_EXPORT MachPortRendezvousServerBase {
 protected:
  MachPortRendezvousServerBase();
  virtual ~MachPortRendezvousServerBase();

  // The Mach receive right for the server. A send right to this is port is
  // registered in the bootstrap server.
  apple::ScopedMachReceiveRight server_port_;

  // Mach message dispatch source for |server_port_|.
  std::unique_ptr<apple::DispatchSourceMach> dispatch_source_;

  // Ask for the associated ports associated with `audit_token`.
  // Return `std::nullopt` if the client is not authorized to
  // retrieve ports.
  virtual std::optional<MachPortsForRendezvous> PortsForClient(
      audit_token_t audit_token) = 0;

  // Return whether `msg_id` should be accepted along with the known
  // message IDs. Platform-specific subclasses may return additional data
  // based on the `msg_id` within `AdditionalDataForReply`.
  virtual bool IsValidAdditionalMessageId(mach_msg_id_t msg_id) const = 0;

  // Return additional data to be attached to a reply for `request`.
  virtual std::vector<uint8_t> AdditionalDataForReply(
      mach_msg_id_t request) const = 0;

  // The server-side Mach message handler. Called by |dispatch_source_| when a
  // message is received.
  void HandleRequest();

  // Returns a buffer containing a well-formed Mach message, destined for
  // `reply_port` containing descriptors for the specified `ports` and
  // `additional_data`.
  std::unique_ptr<uint8_t[]> CreateReplyMessage(
      mach_port_t reply_port,
      const MachPortsForRendezvous& ports,
      std::vector<uint8_t> additional_data);
};

#if BUILDFLAG(IS_IOS)
// An implementation class that works for a single process. It is intended
// that each process spawned will create a corresponding instance and the
// mach send right of this server will be sent using XPC to the process.
class BASE_EXPORT MachPortRendezvousServerIOS final
    : public MachPortRendezvousServerBase {
 public:
  MachPortRendezvousServerIOS(const MachPortsForRendezvous& ports);
  ~MachPortRendezvousServerIOS() override;
  MachPortRendezvousServerIOS(const MachPortRendezvousServerIOS&) = delete;
  MachPortRendezvousServerIOS& operator=(const MachPortRendezvousServerIOS&) =
      delete;

  // Retrieve the send right to be sent to the process.
  apple::ScopedMachSendRight GetMachSendRight();

 protected:
  std::optional<MachPortsForRendezvous> PortsForClient(audit_token_t) override;
  bool IsValidAdditionalMessageId(mach_msg_id_t) const override;
  std::vector<uint8_t> AdditionalDataForReply(mach_msg_id_t) const override;

 private:
  apple::ScopedMachSendRight send_right_;
  MachPortsForRendezvous ports_;
};

#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_MAC)

// An implementation class that uses bootstrap to register ports to many
// processes.
class BASE_EXPORT MachPortRendezvousServerMac final
    : public MachPortRendezvousServerBase {
 public:
  // Returns the instance of the server. Upon the first call to this method,
  // the server is created, which registers an endpoint in the Mach bootstrap
  // namespace.
  static MachPortRendezvousServerMac* GetInstance();

  // Add feature state to an environment variable that will be used when
  // launching a child process. MachPortRendezvousClient is used during
  // feature list initialization so any state it uses must be passed
  // via a side channel.
  // TODO(crbug.com/362302761): Remove once enforcement is enabled by default.
  static void AddFeatureStateToEnvironment(EnvironmentMap& environment);

  MachPortRendezvousServerMac(const MachPortRendezvousServerMac&) = delete;
  MachPortRendezvousServerMac& operator=(const MachPortRendezvousServerMac&) =
      delete;

  // Registers a collection of Mach ports |ports| to be acquirable by the
  // process known by |pid|. This cannot be called again for the same |pid|
  // until the process known by |pid| has either acquired the ports or died.
  //
  // This must be called with the lock from GetLock() held.
  void RegisterPortsForPid(pid_t pid, const MachPortsForRendezvous& ports)
      EXCLUSIVE_LOCKS_REQUIRED(GetLock());

  // Sets the process requirement that `pid` must match before it
  // can acquire any Mach ports. This cannot be called again for the same `pid`
  // until the process known by `pid` has acquired the ports or died.
  //
  // This must be called with the lock from GetLock() held.
  void SetProcessRequirementForPid(pid_t pid,
                                   mac::ProcessRequirement requirement)
      EXCLUSIVE_LOCKS_REQUIRED(GetLock());

  // Returns a lock on the internal port registration map. The parent process
  // should hold this lock for the duration of launching a process, including
  // after calling RegisterPortsForPid(). This ensures that a child process
  // cannot race acquiring ports before they are registered. The lock should
  // be released after the child process is launched and the ports are
  // registered.
  Lock& GetLock() LOCK_RETURNED(lock_) { return lock_; }

  void ClearClientDataForTesting() EXCLUSIVE_LOCKS_REQUIRED(GetLock());

 protected:
  // Returns the registered collection of ports for the specified `audit_token`.
  // `std::nullopt` indicates that the client is not authorized to retrieve the
  // ports. This claims the collection of ports and removes the entry from
  // `client_data_`.
  std::optional<MachPortsForRendezvous> PortsForClient(
      audit_token_t audit_token) override;

  bool IsValidAdditionalMessageId(mach_msg_id_t) const override;
  std::vector<uint8_t> AdditionalDataForReply(
      mach_msg_id_t request) const override;

 private:
  friend class MachPortRendezvousServerTest;
  friend struct MachPortRendezvousFuzzer;

  MachPortRendezvousServerMac();
  ~MachPortRendezvousServerMac() override;

  struct ClientData;

  // Returns the `ClientData` for `pid`, creating it if necessary.
  // It will be cleaned up automatically when `pid` exits.
  ClientData& ClientDataForPid(int pid) EXCLUSIVE_LOCKS_REQUIRED(GetLock());

  // Called by the ClientData::exit_watcher dispatch sources when a process
  // for which ports have been registered exits. This releases port rights
  // that are strongly owned, in the event that the child has not claimed them.
  void OnClientExited(pid_t pid);

  Lock lock_;
  // Association of pid-to-ports.
  std::map<pid_t, ClientData> client_data_ GUARDED_BY(lock_);
};

#endif

// Client class for accessing the memory object exposed by the
// MachPortRendezvousServer.
class BASE_EXPORT MachPortRendezvousClient {
 public:
  MachPortRendezvousClient(const MachPortRendezvousClient&) = delete;
  MachPortRendezvousClient& operator=(const MachPortRendezvousClient&) = delete;

  // Connects to the MachPortRendezvousServer and requests any registered Mach
  // ports. This only performs the rendezvous once. Subsequent calls to this
  // method return the same instance. If the rendezvous fails, which can happen
  // if the server is not available or if the server fails the code signature
  // validation and requirement check, this returns null. Acquiring zero ports
  // from the exchange is not considered a failure.
  static MachPortRendezvousClient* GetInstance();

  // Returns the Mach send right that was registered with |key|. If no such
  // right exists, or it was already taken, returns an invalid right. Safe to
  // call from any thread. DCHECKs if the right referenced by |key| is not a
  // send or send-once right.
  apple::ScopedMachSendRight TakeSendRight(
      MachPortsForRendezvous::key_type key);

  // Returns the Mach receive right that was registered with |key|. If no such
  // right exists, or it was already taken, returns an invalid right. Safe to
  // call from any thread. DCHECKs if the right referenced by |key| is not a
  // receive right.
  apple::ScopedMachReceiveRight TakeReceiveRight(
      MachPortsForRendezvous::key_type key);

  // Returns the number of ports in the client. After PerformRendezvous(), this
  // reflects the number of ports acquired. But as rights are taken, this
  // only reflects the number of remaining rights.
  size_t GetPortCount();

 protected:
  MachPortRendezvousClient();
  virtual ~MachPortRendezvousClient();

  // Perform platform-specific validation on a received message and the peer
  // that sent it.
  virtual bool ValidateMessage(mach_msg_base_t* message,
                               BufferIterator<uint8_t> body) = 0;

  // Sends the actual IPC message to |server_port| and parses the reply.
  bool SendRequest(apple::ScopedMachSendRight server_port,
                   mach_msg_id_t request_id,
                   size_t additional_response_data_size = 0)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Returns a MachRendezvousPort for a given key and removes it from the
  // |ports_| map. If an entry does not exist for that key, then a
  // MachRendezvousPort with MACH_PORT_NULL is returned.
  MachRendezvousPort PortForKey(MachPortsForRendezvous::key_type key);

  Lock lock_;
  // The collection of ports that was acquired.
  MachPortsForRendezvous ports_ GUARDED_BY(lock_);
};

#if BUILDFLAG(IS_IOS)
BASE_EXPORT
class BASE_EXPORT MachPortRendezvousClientIOS final
    : public MachPortRendezvousClient {
 public:
  // Initialize the MacPortRendezvousClient using `server_port`.
  static bool Initialize(apple::ScopedMachSendRight server_port);

 protected:
  bool ValidateMessage(mach_msg_base_t* message,
                       BufferIterator<uint8_t> body) override;

 private:
  MachPortRendezvousClientIOS();
  ~MachPortRendezvousClientIOS() override;

  // Helper method to look up the server in the bootstrap namespace and send
  // the acquisition request message.
  bool AcquirePorts(apple::ScopedMachSendRight server_port);
};
#endif

#if BUILDFLAG(IS_MAC)

// Describes how the `ProcessRequirement` should be used during Mach port
// rendezvous. The active policy is derived from the feature flags in the
// browser process and is passed via an environment variable to child processes.
// TODO(crbug.com/362302761): Remove this policy once enforcement is enabled by
// default.
enum class MachPortRendezvousPeerValidationPolicy {
  // Do not validate the peer against a process requirement.
  kNoValidation,

  // Validate the peer against a process requirement, if specified, but do not
  // abort rendezvous if validation fails. Used to gather success metrics during
  // experiment rollout.
  kValidateOnly,

  // Validate the peer against a process requirement, if specified, and abort
  // rendezvous if the validation fails.
  kEnforce,
};

class BASE_EXPORT MachPortRendezvousClientMac final
    : public MachPortRendezvousClient {
 public:
  // Set a ProcessRequirement that the server should be validated
  // against before accepting any Mach ports from it.
  //
  // Must be called before `GetInstance` or this will have no effect.
  static void SetServerProcessRequirement(mac::ProcessRequirement requirement);

  // Get the peer validation policy that was derived from feature flags.
  static MachPortRendezvousPeerValidationPolicy
  PeerValidationPolicyForTesting();

 protected:
  // Validate the server against a process requirement if one was set via
  // `SetServerProcessRequirement`.
  bool ValidateMessage(mach_msg_base_t* message,
                       BufferIterator<uint8_t> body) override;

 private:
  friend class MachPortRendezvousClient;

  MachPortRendezvousClientMac();
  ~MachPortRendezvousClientMac() override;

  // Returns the name of the server to look up in the bootstrap namespace.
  static std::string GetBootstrapName();

  // Helper method to look up the server in the bootstrap namespace and send
  // the acquisition request message.
  bool AcquirePorts();

  // Take ownership of the server process requirement, if any.
  static std::optional<mac::ProcessRequirement>
  TakeServerCodeSigningRequirement();

  // Whether Info.plist data is needed from the server in order
  // to validate `server_requirement_`.
  bool NeedsInfoPlistData() const;

  std::optional<mac::ProcessRequirement> server_requirement_;
};

// Whether any peer process requirements should be validated.
BASE_EXPORT BASE_DECLARE_FEATURE(kMachPortRendezvousValidatePeerRequirements);

// Whether a failure to validate a peer process against a requirement
// should result in aborting the rendezvous.
BASE_EXPORT BASE_DECLARE_FEATURE(kMachPortRendezvousEnforcePeerRequirements);

#endif

}  // namespace base

#endif  // BASE_APPLE_MACH_PORT_RENDEZVOUS_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_FIELD_TRIAL_MEMORY_MAC_H_
#define BASE_METRICS_FIELD_TRIAL_MEMORY_MAC_H_

#include <mach/port.h>
#include <sys/types.h>

#include <memory>
#include <string>

#include "base/base_export.h"
#include "base/mac/dispatch_source_mach.h"
#include "base/mac/scoped_mach_port.h"
#include "base/macros.h"

namespace base {

// FieldTrialMemoryServer services requests for the FieldTrial shared memory
// region on Mac. Shared memory on Mac uses Mach ports, which cannot be
// transferred across process creation. Instead, this class publishes an
// endpoint in the bootstrap server. Child processes look up the server and then
// send requests to acquire the shared memory object. Only processes that are
// direct children of the process that is running this server are allowed to
// acquire the memory object send right.
class BASE_EXPORT FieldTrialMemoryServer {
 public:
  // Creates a server that will vend access to the passed |memory_object|.
  // This does not change the user refcount of the object. Start() must be
  // called before requests will be processed.
  explicit FieldTrialMemoryServer(mach_port_t memory_object);
  ~FieldTrialMemoryServer();

  // Starts processing requests for the server. Returns false if the server
  // could not be started and true on success.
  bool Start();

 private:
  friend class FieldTrialMemoryServerTest;

  // Exposed for testing.
  void set_server_pid(pid_t pid) { server_pid_ = pid; }

  // Returns the name of the server to publish in the bootstrap namespace.
  static std::string GetBootstrapName();

  // The server-side Mach message handler.
  void HandleRequest();

  mach_port_t memory_object_;  // weak
  pid_t server_pid_;           // PPID used for access control checks.
  mac::ScopedMachReceiveRight server_port_;
  std::unique_ptr<DispatchSourceMach> dispatch_source_;

  DISALLOW_COPY_AND_ASSIGN(FieldTrialMemoryServer);
};

// Client class for accessing the memory object exposed by the
// FieldTrialMemoryServer.
class BASE_EXPORT FieldTrialMemoryClient {
 public:
  // Called by children of the process running the FieldTrialMemoryServer, this
  // attempts to acquire the port for the |memory_object_|. Returns the port
  // on success or MACH_PORT_NULL on error or failure.
  static mac::ScopedMachSendRight AcquireMemoryObject();

  // Returns the name of the server to look up in the bootstrap namespace.
  static std::string GetBootstrapName();

 private:
  // Sends the Mach message to |server_port| to acquire the memory object.
  static mac::ScopedMachSendRight ChildSendRequest(
      mac::ScopedMachSendRight server_port);

  FieldTrialMemoryClient();
  ~FieldTrialMemoryClient();

  DISALLOW_COPY_AND_ASSIGN(FieldTrialMemoryClient);
};

}  // namespace base

#endif  // BASE_METRICS_FIELD_TRIAL_MEMORY_MAC_H_

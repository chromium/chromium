// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_
#define CHROME_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_

#include <stdint.h>

#include <memory>

#include "base/types/expected.h"

class PrefService;

class RemoteDebuggingServer {
 public:
  enum class NotStartedReason {
    // The server was not requested by either specifying the
    // --remote-debugging-port or --remote-debugging-pipe switch.
    kNotRequested,
    // Disabled by enterprise policy.
    kDisabledByPolicy,
    // Disabled as a default user data dir is being used.
    kDisabledByDefaultUserDataDir,
  };
  // Obtains an instance of the RemoteDebuggingServer if one was started by
  // either --remote-debugging-port or --remote-debugging-pipe being supplied on
  // the command line, or nullptr otherwise.
  static base::expected<std::unique_ptr<RemoteDebuggingServer>,
                        NotStartedReason>
  GetInstance(PrefService* local_state);

  static void EnableTetheringForDebug();

  RemoteDebuggingServer(const RemoteDebuggingServer&) = delete;
  RemoteDebuggingServer& operator=(const RemoteDebuggingServer&) = delete;

  virtual ~RemoteDebuggingServer();

 private:
  RemoteDebuggingServer();
};

#endif  // CHROME_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_

// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_
#define CHROME_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_

#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "content/public/browser/devtools_agent_host.h"

class PrefChangeRegistrar;
class PrefService;

namespace content {
class DevToolsSocketFactory;
}

class RemoteDebuggingServer {
 public:
  static constexpr int kDefaultDevToolsPort = 9222;

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Enables the default user data dir check even for non-Chrome branded builds,
  // for testing.
  static void EnableDefaultUserDataDirCheckForTesting();
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  // Returns the port number from the last session, or a default value.
  // Exposed for testing.
  static int GetPortFromUserDataDir(const base::FilePath& output_dir);

  RemoteDebuggingServer(const RemoteDebuggingServer&) = delete;
  RemoteDebuggingServer& operator=(const RemoteDebuggingServer&) = delete;

  virtual ~RemoteDebuggingServer();

  void StartHttpServerInApprovalMode(PrefService* local_state);

 protected:
  RemoteDebuggingServer();

  virtual void StartHttpServer(
      std::unique_ptr<content::DevToolsSocketFactory> factory,
      const base::FilePath& output_dir,
      const base::FilePath& debug_frontend_dir,
      content::DevToolsAgentHost::RemoteDebuggingServerMode mode);
  virtual void StopHttpServer();
  virtual void StartPipeHandler();
  virtual void StopPipeHandler();

 private:
  void MaybeStartOrStopServerForPrefChange();
  void StartHttpServerInApprovalModeWithPort(const base::FilePath& output_dir,
                                             int port);

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  // Used by MaybeStartOrStopServerForPrefChange to ensure that the server is
  // only started once.
  bool is_http_server_running_ = false;
  // Ensures that the server is not started twice at the same time
  // due to the async reading of the ActivePort file.
  bool is_http_server_being_started_ = false;

  base::WeakPtrFactory<RemoteDebuggingServer> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_

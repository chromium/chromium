// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_SESSION_MANAGER_CLIENT_LINUX_H_
#define CHROME_BROWSER_LIFETIME_SESSION_MANAGER_CLIENT_LINUX_H_

#include <memory>
#include <string>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

namespace base {
class CommandLine;
}  // namespace base

struct _SmcConn;

// Command-line switch passing the XSMP client ID assigned in the previous
// desktop session so the session manager can match the relaunched instance.
inline constexpr char kSmClientIdSwitch[] = "sm-client-id";

// Connects to the desktop session manager via the X11R6 Session Management
// Protocol (XSMP) when $SESSION_MANAGER is set, registers the restart and clone
// commands, and handles SaveYourself and Die messages during logout.
class SessionManagerClientLinux {
 public:
  // Connects to $SESSION_MANAGER, resuming the client ID passed via
  // `--sm-client-id` in `command_line` if any. Returns nullptr if
  // $SESSION_MANAGER is unset, libSM/libICE are not installed, or the
  // connection fails. `save_state_cb` runs on each non-global SaveYourself
  // request and `die_cb` runs once the session manager sends Die.
  static std::unique_ptr<SessionManagerClientLinux> Connect(
      const base::CommandLine& command_line,
      base::RepeatingClosure save_state_cb,
      base::OnceClosure die_cb);

  SessionManagerClientLinux(const SessionManagerClientLinux&) = delete;
  SessionManagerClientLinux& operator=(const SessionManagerClientLinux&) =
      delete;
  ~SessionManagerClientLinux();

  const std::string& client_id() const { return client_id_; }

 private:
  struct Callbacks;

  SessionManagerClientLinux(base::RepeatingClosure save_state_cb,
                            base::OnceClosure die_cb);

  void SetProperties(const base::CommandLine& command_line);
  void OnIceReadable();
  void OnSaveYourself(int save_type);
  void Disconnect(bool close_smc);

  raw_ptr<struct _SmcConn> smc_conn_ = nullptr;
  std::string client_id_;
  base::RepeatingClosure save_state_cb_;
  base::OnceClosure die_cb_;
  bool die_received_ = false;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> watch_controller_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<SessionManagerClientLinux> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_LIFETIME_SESSION_MANAGER_CLIENT_LINUX_H_

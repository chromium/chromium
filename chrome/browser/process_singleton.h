// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROCESS_SINGLETON_H_
#define CHROME_BROWSER_PROCESS_SINGLETON_H_

#include "base/sequence_checker.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif  // BUILDFLAG(IS_WIN)

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
#include "base/files/scoped_temp_dir.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/message_window.h"
#endif  // BUILDFLAG(IS_WIN)

namespace base {
class CommandLine;
}

// ProcessSingleton ----------------------------------------------------------
//
// This class allows different browser processes to communicate with
// each other.  It is named according to the user data directory, so
// we can be sure that no more than one copy of the application can be
// running at once with a given data directory.
//
// Implementation notes:
// - the Windows implementation uses an invisible global message window;
// - the Linux implementation uses a Unix domain socket in the user data dir.

class ProcessSingleton {
 public:
  // Used to send the reason of remote hang process termination as histogram.
  enum RemoteHungProcessTerminateReason {
#if BUILDFLAG(IS_WIN)
    USER_ACCEPTED_TERMINATION = 1,
    NO_VISIBLE_WINDOW_FOUND = 2,
#elif BUILDFLAG(IS_POSIX)
    NOTIFY_ATTEMPTS_EXCEEDED = 3,
    SOCKET_WRITE_FAILED = 4,
    SOCKET_READ_FAILED = 5,
#endif
    REMOTE_HUNG_PROCESS_TERMINATE_REASON_COUNT
  };

  // Used to send the result of interaction with remote process as histograms in
  // case when remote process influences on start.
  enum RemoteProcessInteractionResult {
    TERMINATE_SUCCEEDED = 0,
    TERMINATE_FAILED = 1,
    REMOTE_PROCESS_NOT_FOUND = 2,
#if BUILDFLAG(IS_WIN)
    TERMINATE_WAIT_TIMEOUT = 3,
    RUNNING_PROCESS_NOTIFY_ERROR = 4,
#elif BUILDFLAG(IS_POSIX)
    TERMINATE_NOT_ENOUGH_PERMISSIONS = 5,
    REMOTE_PROCESS_SHUTTING_DOWN = 6,
    PROFILE_UNLOCKED = 7,
    PROFILE_UNLOCKED_BEFORE_KILL = 8,
    SAME_BROWSER_INSTANCE = 9,
    SAME_BROWSER_INSTANCE_BEFORE_KILL = 10,
    FAILED_TO_EXTRACT_PID = 11,
    INVALID_LOCK_FILE = 12,
    ORPHANED_LOCK_FILE = 13,
#endif
    USER_REFUSED_TERMINATION = 14,
    REMOTE_PROCESS_INTERACTION_RESULT_COUNT
  };

  // Logged as histograms, do not modify these values.
  enum NotifyResult {
    PROCESS_NONE = 0,
    PROCESS_NOTIFIED = 1,
    PROFILE_IN_USE = 2,
    LOCK_ERROR = 3,
    LAST_VALUE = LOCK_ERROR
  };

  static constexpr int kNumNotifyResults = LAST_VALUE + 1;

  // Implement this callback to handle notifications from other processes. The
  // callback will receive the command line and directory with which the other
  // Chrome process was launched. Return true if the command line will be
  // handled within the current browser instance or false if the remote process
  // should handle it (i.e., because the current process is shutting down).
  using NotificationCallback =
      base::RepeatingCallback<bool(base::CommandLine command_line,
                                   const base::FilePath& current_directory)>;

  ProcessSingleton(const base::FilePath& user_data_dir,
                   const NotificationCallback& notification_callback);

  ProcessSingleton(const ProcessSingleton&) = delete;
  ProcessSingleton& operator=(const ProcessSingleton&) = delete;

  ~ProcessSingleton();

  // Notify another process, if available. Otherwise sets ourselves as the
  // singleton instance. Returns PROCESS_NONE if we became the singleton
  // instance. Callers are guaranteed to either have notified an existing
  // process or have grabbed the singleton (unless the profile is locked by an
  // unreachable process).
  // TODO(brettw): Make the implementation of this method non-platform-specific
  // by making Linux re-use the Windows implementation.
  NotifyResult NotifyOtherProcessOrCreate();

  // Sets ourself up as the singleton instance.  Returns true on success.  If
  // false is returned, we are not the singleton instance and the caller must
  // exit.
  // NOTE: Most callers should generally prefer NotifyOtherProcessOrCreate() to
  // this method, only callers for whom failure is preferred to notifying
  // another process should call this directly.
  bool Create();

  // Start watching for notifications from other processes.
  void StartWatching();

  // Clear any lock state during shutdown.
  void Cleanup();

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
  static void DisablePromptForTesting();
  static void SkipIsChromeProcessCheckForTesting(bool skip);
  static void SetUserOptedUnlockInUseProfileForTesting(bool set_unlock);
#endif
#if BUILDFLAG(IS_WIN)
  // Called to query whether to kill a hung browser process that has visible
  // windows. Return true to allow killing the hung process.
  using ShouldKillRemoteProcessCallback = base::RepeatingCallback<bool()>;
  void OverrideShouldKillRemoteProcessCallbackForTesting(
      const ShouldKillRemoteProcessCallback& display_dialog_callback);
#endif

 protected:
  // Notify another process, if available.
  // Returns true if another process was found and notified, false if we should
  // continue with the current process.
  // On Windows, Create() has to be called before this.
  NotifyResult NotifyOtherProcess();

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
  // Exposed for testing.  We use a timeout on Linux, and in tests we want
  // this timeout to be short.
  NotifyResult NotifyOtherProcessWithTimeout(
      const base::CommandLine& command_line,
      int retry_attempts,
      const base::TimeDelta& timeout,
      bool kill_unresponsive);
  NotifyResult NotifyOtherProcessWithTimeoutOrCreate(
      const base::CommandLine& command_line,
      int retry_attempts,
      const base::TimeDelta& timeout);
  void OverrideCurrentPidForTesting(base::ProcessId pid);
  void OverrideKillCallbackForTesting(
      const base::RepeatingCallback<void(int)>& callback);
#endif

 private:
  NotificationCallback notification_callback_;  // Handler for notifications.

#if BUILDFLAG(IS_WIN)
  bool EscapeVirtualization(const base::FilePath& user_data_dir);

  HWND remote_window_;  // The HWND_MESSAGE of another browser.
  base::win::MessageWindow window_;  // The message-only window.
  bool is_virtualized_;  // Stuck inside Microsoft Softricity VM environment.
  HANDLE lock_file_;
  base::FilePath user_data_dir_;
  ShouldKillRemoteProcessCallback should_kill_remote_process_callback_;
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
  // Return true if the given pid is one of our child processes.
  // Assumes that the current pid is the root of all pids of the current
  // instance.
  bool IsSameChromeInstance(pid_t pid);

  // Extract the process's pid from a symbol link path and if it is on
  // the same host or is_connected_to_socket is true, kill the process, unlink
  // the lock file and return true.
  // If the process is part of the same chrome instance, unlink the lock file
  // and return true without killing it.
  // If the process is on a different host and is_connected_to_socket is false,
  // display profile in use error dialog (on Linux). If user opted to unlock
  // profile (on Mac OS X by default), unlink the lock file and return true.
  // Otherwise return false.
  bool KillProcessByLockPath(bool is_connected_to_socket);

  // Default function to kill a process, overridable by tests.
  void KillProcess(int pid);

  // Allow overriding for tests.
  base::ProcessId current_pid_;

  // Function to call when the other process is hung and needs to be killed.
  // Allows overriding for tests.
  base::RepeatingCallback<void(int)> kill_callback_;

  // Path in file system to the socket.
  base::FilePath socket_path_;

  // Path in file system to the lock.
  base::FilePath lock_path_;

  // Path in file system to the cookie file.
  base::FilePath cookie_path_;

  // Temporary directory to hold the socket.
  base::ScopedTempDir socket_dir_;
  int sock_ = -1;

  // Helper class for linux specific messages.  LinuxWatcher is ref counted
  // because it posts messages between threads.
  class LinuxWatcher;
  scoped_refptr<LinuxWatcher> watcher_;
#endif

#if BUILDFLAG(IS_MAC)
  // macOS 10.13 tries to open a new Chrome instance if a user tries to
  // open an external link after Chrome has updated, but not relaunched.
  // This method extracts any waiting "open URL" AppleEvent and forwards
  // it to the running process. Returns true if an event was found and
  // forwarded.
  // crbug.com/777863
  bool WaitForAndForwardOpenURLEvent(pid_t event_destination_pid);
#endif

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_PROCESS_SINGLETON_H_

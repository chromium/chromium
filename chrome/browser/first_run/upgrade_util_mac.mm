// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/upgrade_util_mac.h"

#import <AppKit/AppKit.h>
#include <libproc.h>
#include <unistd.h>

#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/mac/authorization_util.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_authorizationref.h"
#import "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/mac/relauncher.h"
#include "chrome/common/mac/staging_watcher.h"
#include "chrome/grit/chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace upgrade_util {

namespace {

// Get the uid and executable path for a pid. Returns true iff successful.
// |path_buffer| must be of PROC_PIDPATHINFO_MAXSIZE length.
bool GetUIDAndPathOfPID(pid_t pid, char* path_buffer, uid_t* out_uid) {
  struct proc_bsdshortinfo info;
  int error = proc_pidinfo(pid, PROC_PIDT_SHORTBSDINFO, 0, &info, sizeof(info));
  if (error <= 0)
    return false;

  error = proc_pidpath(pid, path_buffer, PROC_PIDPATHINFO_MAXSIZE);
  if (error <= 0)
    return false;

  *out_uid = info.pbsi_uid;
  return true;
}

struct ThisAndOtherUserPids {
  std::set<pid_t> this_user;
  std::set<pid_t> other_user;
};

ThisAndOtherUserPids GetPidsOfOtherInstancesOfThisBinary() {
  ThisAndOtherUserPids pids;

  // Get list of all processes.

  int pid_array_size_needed = proc_listallpids(nullptr, 0);
  if (pid_array_size_needed <= 0)
    return pids;
  std::vector<pid_t> pid_array(pid_array_size_needed * 4);  // slack
  int pid_count = proc_listallpids(pid_array.data(),
                                   pid_array.size() * sizeof(pid_array[0]));
  if (pid_count <= 0)
    return pids;

  pid_array.resize(pid_count);

  // Get info about this process.

  const pid_t this_pid = getpid();
  uid_t this_uid;
  char this_path[PROC_PIDPATHINFO_MAXSIZE];
  if (!GetUIDAndPathOfPID(this_pid, this_path, &this_uid))
    return pids;

  // Compare all other processes to this one.

  for (pid_t pid : pid_array) {
    if (pid == this_pid)
      continue;

    uid_t uid;
    char path[PROC_PIDPATHINFO_MAXSIZE];
    if (!GetUIDAndPathOfPID(pid, path, &uid))
      continue;

    if (strcmp(path, this_path) != 0)
      continue;

    if (uid == this_uid)
      pids.this_user.insert(pid);
    else
      pids.other_user.insert(pid);
  }

  return pids;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// ShouldContinueToRelaunchForUpgrade() can complete in quite a few different
// ways. This allows analysis of what's happening in the field.
enum class OtherInstancesCheckResult {
  kOldStyleUpdate = 0,
  kNoOtherInstancesAtStart = 1,
  kUserDidNotAuthorize = 2,
  kInitialExecuteAndWaitFailed = 3,
  kSecondaryExecuteAndWaitFailed = 4,
  kShowedNoDialogsNoInstancesExistAtExit = 5,
  kShowedNoDialogsInstancesExistAtExit = 6,
  kShowedOtherUserDialogNoInstancesExistAtExit = 7,
  kShowedOtherUserDialogInstancesExistAtExit = 8,
  kShowedThisUserDialogNoInstancesExistAtExit = 9,
  kShowedThisUserDialogInstancesExistAtExit = 10,
  kShowedBothDialogsNoInstancesExistAtExit = 11,
  kShowedBothDialogsInstancesExistAtExit = 12,
  kMaxValue = kShowedBothDialogsInstancesExistAtExit,
};

void LogInstanceCheckResult(OtherInstancesCheckResult result) {
  UMA_HISTOGRAM_ENUMERATION("OSX.OtherInstancesCheckResult", result);
}

void LogInstanceCheckResult(bool showed_this_user_dialog,
                            bool showed_other_user_dialog,
                            bool other_instances_exist) {
  int uma_value = showed_this_user_dialog << 2 | showed_other_user_dialog << 1 |
                  other_instances_exist << 0;
  uma_value += static_cast<int>(
      OtherInstancesCheckResult::kShowedNoDialogsNoInstancesExistAtExit);
  LogInstanceCheckResult(static_cast<OtherInstancesCheckResult>(uma_value));
}

// Sends a termination signal to a set of processes. Returns true if the kill
// command was successfully executed. This says nothing about whether the kill
// command succeeded in actually killing anything.
enum class SignalType { kSoftKill, kHardKill };
bool SendKillSignalToPids(AuthorizationRef authorization,
                          SignalType signal_type,
                          std::set<pid_t> pids) {
  DCHECK(!pids.empty());

  // ExecuteWithPrivilegesAndWait() requires a helper tool that returns
  // its pid via stdout. Using the inherent abilities of /bin/sh
  // simplifies things.
  static constexpr char shell_path[] = "/bin/sh";
  static constexpr char shell_execute_command[] = "-c";

  std::string command_string = "echo $$; exec /bin/kill ";
  if (signal_type == SignalType::kSoftKill)
    command_string += "-TERM";
  else
    command_string += "-KILL";
  for (pid_t pid : pids) {
    command_string += " ";
    command_string += base::NumberToString(pid);
  }

  const char* command_string_c = command_string.c_str();
  const char* arguments[] = {shell_execute_command, command_string_c, nullptr};

  OSStatus status = base::mac::ExecuteWithPrivilegesAndWait(
      authorization, shell_path, kAuthorizationFlagDefaults, arguments,
      nullptr,   // pipe
      nullptr);  // exit_status; no point in checking, as the result is checked
                 // later by enumerating processes.
  return status == errAuthorizationSuccess;
}

}  // namespace

ThisAndOtherUserCounts GetCountOfOtherInstancesOfThisBinary() {
  ThisAndOtherUserPids pids = GetPidsOfOtherInstancesOfThisBinary();

  return ThisAndOtherUserCounts{pids.this_user.size(), pids.other_user.size()};
}

bool ShouldContinueToRelaunchForUpgrade() {
  // If this isn't a new-style update, and the staging key isn't set, don't
  // block at all.

  if (![CrStagingKeyWatcher isStagingKeySet]) {
    LogInstanceCheckResult(OtherInstancesCheckResult::kOldStyleUpdate);
    return true;
  }

  ThisAndOtherUserPids pids = GetPidsOfOtherInstancesOfThisBinary();

  if (pids.this_user.empty() && pids.other_user.empty()) {
    LogInstanceCheckResult(OtherInstancesCheckResult::kNoOtherInstancesAtStart);
    return true;
  }

  // If there are both other-user and same-user instances, start with the auth
  // dialog, as the user might not be able to auth. It would be weird to have
  // the user kill all their own instances, and then hit a dialog they can't
  // answer, and be left having quit a lot of stuff.
  bool showed_this_user_dialog = false;
  bool showed_other_user_dialog = false;

  if (!pids.other_user.empty()) {
    showed_other_user_dialog = true;

    NSString* prompt = l10n_util::GetNSString(
        IDS_UPDATE_OTHER_INSTANCES_OTHER_USER_AUTHENTICATION_PROMPT);
    base::mac::ScopedAuthorizationRef authorization(
        base::mac::AuthorizationCreateToRunAsRoot(
            base::mac::NSToCFCast(prompt)));

    if (authorization.get()) {
      // A simple kill -TERM works if no page has a beforeunload handler; kill
      // -KILL is required otherwise. Therefore, do a kill -TERM first, wait a
      // few seconds, and then kill -KILL stragglers.

      bool success = SendKillSignalToPids(authorization, SignalType::kSoftKill,
                                          pids.other_user);
      if (!success) {
        // Killing other instances failed; installation cannot proceed.
        LogInstanceCheckResult(
            OtherInstancesCheckResult::kInitialExecuteAndWaitFailed);
        return false;
      }

      // Three seconds is arbitrary; the intent is to give the other Chrome
      // instances time to close themselves down safely, if they are able to.
      // TODO(avi): Be smarter here; perhaps a kevent watcher (with timeout) on
      // the pids, as to not wait as long if they all close quickly?
      sleep(3);

      pids = GetPidsOfOtherInstancesOfThisBinary();
      if (!pids.other_user.empty()) {
        success = SendKillSignalToPids(authorization, SignalType::kHardKill,
                                       pids.other_user);
        if (!success) {
          // Killing other instances failed; installation cannot proceed.
          LogInstanceCheckResult(
              OtherInstancesCheckResult::kSecondaryExecuteAndWaitFailed);
          return false;
        }

        // Also arbitrary, though less time is needed for a hard kill.
        // TODO(avi): Use a watcher as above.
        sleep(1);
      }
    } else {
      // There was no auth to kill other-user instances. At this point,
      // installation can't proceed, so don't ask the users to kill their own
      // instances.
      LogInstanceCheckResult(OtherInstancesCheckResult::kUserDidNotAuthorize);
      return false;
    }
  }

  if (!pids.this_user.empty()) {
    showed_this_user_dialog = true;

    @autoreleasepool {
      NSString* title = l10n_util::GetNSString(
          IDS_UPDATE_OTHER_INSTANCES_SAME_USER_DIALOG_TITLE);
      NSString* error = l10n_util::GetNSString(
          IDS_UPDATE_OTHER_INSTANCES_SAME_USER_DIALOG_MESSAGE);
      NSString* ok = l10n_util::GetNSString(IDS_OK);

      NSAlert* alert = [[[NSAlert alloc] init] autorelease];

      [alert setAlertStyle:NSWarningAlertStyle];
      [alert setMessageText:title];
      [alert setInformativeText:error];
      [alert addButtonWithTitle:ok];

      [alert runModal];
    }
  }

  // Count the processes again, as the non-user processes may have been killed
  // (in the other-user case), or the user may have switched away and killed
  // their own processes while the same-user dialog was up.
  pids = GetPidsOfOtherInstancesOfThisBinary();
  bool other_instances_exist =
      !pids.this_user.empty() || !pids.other_user.empty();
  LogInstanceCheckResult(showed_this_user_dialog, showed_other_user_dialog,
                         other_instances_exist);

  return !other_instances_exist;
}

bool RelaunchChromeBrowserImpl(const base::CommandLine& command_line) {
  upgrade_util::ThisAndOtherUserCounts counts =
      upgrade_util::GetCountOfOtherInstancesOfThisBinary();
  const int other_instances = counts.this_user_count + counts.other_user_count;
  const bool wait_for_staged_update = (other_instances == 0);

  return mac_relauncher::RelaunchApp(command_line.argv(),
                                     wait_for_staged_update);
}

}  // namespace upgrade_util

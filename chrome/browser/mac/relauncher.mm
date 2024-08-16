// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/mac/relauncher.h"

#import <AppKit/AppKit.h>
#include <CoreFoundation/CoreFoundation.h>
#include <crt_externs.h>
#include <dlfcn.h>
#include <stddef.h>
#include <string.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "base/apple/bundle_locations.h"
#include "base/apple/osstatus_logging.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/mac/launch_application.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/mac/install_from_dmg.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"

namespace mac_relauncher {

namespace {

// The "magic" file descriptor that the relauncher process' write side of the
// pipe shows up on. Chosen to avoid conflicting with stdin, stdout, and
// stderr.
constexpr int kRelauncherSyncFD = STDERR_FILENO + 1;

// The argument separating arguments intended for the relauncher process from
// those intended for the relaunched process. "---" is chosen instead of "--"
// because CommandLine interprets "--" as meaning "end of switches", but
// for many purposes, the relauncher process' CommandLine ought to interpret
// arguments intended for the relaunched process, to get the correct settings
// for such things as logging and the user-data-dir in case it affects crash
// reporting.
constexpr char kRelauncherArgSeparator[] = "---";

// When this argument is supplied to the relauncher process, it will launch
// the relaunched process without bringing it to the foreground.
constexpr char kRelauncherBackgroundArg[] = "--background";

// The beginning of the "process serial number" argument that Launch Services
// sometimes inserts into command lines. A process serial number is only valid
// for a single process, so any PSN arguments will be stripped from command
// lines during relaunch to avoid confusion. When using the CommandLine
// interface, -psn_ may have been rewritten as --psn_. Look for both.
constexpr char kPSNArg[] = "-psn_";
constexpr char kAltPSNArg[] = "--psn_";

// Returns the "type" argument identifying a relauncher process
// ("--type=relauncher").
std::string RelauncherTypeArg() {
  return base::StringPrintf("--%s=%s",
                            switches::kProcessType,
                            switches::kRelauncherProcess);
}

}  // namespace

bool RelaunchApp(const std::vector<std::string>& args) {
  // Use the currently-running application's helper process. The automatic
  // update feature is careful to leave the currently-running version alone,
  // so this is safe even if the relaunch is the result of an update having
  // been applied. In fact, it's safer than using the updated version of the
  // helper process, because there's no guarantee that the updated version's
  // relauncher implementation will be compatible with the running version's.
  base::FilePath child_path;
  if (!base::PathService::Get(content::CHILD_PROCESS_EXE, &child_path)) {
    LOG(ERROR) << "No CHILD_PROCESS_EXE";
    return false;
  }

  std::vector<std::string> relauncher_args;
  return RelaunchAppAtPathWithHelper(child_path, base::apple::OuterBundlePath(),
                                     relauncher_args, args);
}

bool RelaunchAppAtPathWithHelper(
    const base::FilePath& helper,
    const base::FilePath& app_bundle,
    const std::vector<std::string>& relauncher_args,
    const std::vector<std::string>& args) {
  std::vector<std::string> relaunch_args;
  relaunch_args.reserve(relauncher_args.size() + args.size() + 4);
  relaunch_args.push_back(helper.value());
  relaunch_args.push_back(RelauncherTypeArg());

  // If this application isn't in the foreground, the relaunched one shouldn't
  // be either.
  if (!NSApp.active) {
    relaunch_args.push_back(kRelauncherBackgroundArg);
  }

  relaunch_args.insert(relaunch_args.end(),
                       relauncher_args.begin(), relauncher_args.end());

  relaunch_args.push_back(kRelauncherArgSeparator);

  // The relauncher uses base::mac::LaunchApplication, which requires a URL to
  // the bundle. Therefore, substitute in the bundle path as the first
  // "argument"; RelauncherMain is expecting it and will handle it specifically.
  relaunch_args.push_back(app_bundle.value());
  for (size_t i = 1; i < args.size(); ++i) {
    // Strip any PSN arguments, as they apply to a specific process.
    if (args[i].compare(0, strlen(kPSNArg), kPSNArg) != 0 &&
        args[i].compare(0, strlen(kAltPSNArg), kAltPSNArg) != 0) {
      relaunch_args.push_back(args[i]);
    }
  }

  int pipe_fds[2];
  if (HANDLE_EINTR(pipe(pipe_fds)) != 0) {
    PLOG(ERROR) << "pipe";
    return false;
  }

  // The parent process will only use pipe_read_fd as the read side of the
  // pipe. It can close the write side as soon as the relauncher process has
  // forked off. The relauncher process will only use pipe_write_fd as the
  // write side of the pipe. In that process, the read side will be closed by
  // base::LaunchApp because it won't be present in fds_to_remap, and the write
  // side will be remapped to kRelauncherSyncFD by fds_to_remap.
  base::ScopedFD pipe_read_fd(pipe_fds[0]);
  base::ScopedFD pipe_write_fd(pipe_fds[1]);

  // Make sure kRelauncherSyncFD is a safe value. base::LaunchProcess will
  // preserve these three FDs in forked processes, so kRelauncherSyncFD should
  // not conflict with them.
  static_assert(kRelauncherSyncFD != STDIN_FILENO &&
                kRelauncherSyncFD != STDOUT_FILENO &&
                kRelauncherSyncFD != STDERR_FILENO,
                "kRelauncherSyncFD must not conflict with stdio fds");

  base::LaunchOptions options;
  options.fds_to_remap.emplace_back(pipe_write_fd.get(), kRelauncherSyncFD);
  if (!base::LaunchProcess(relaunch_args, options).IsValid()) {
    LOG(ERROR) << "base::LaunchProcess failed";
    return false;
  }

  // The relauncher process is now starting up, or has started up. The
  // original parent process continues.

  pipe_write_fd.reset();  // close(pipe_fds[1]);

  // Synchronize with the relauncher process.
  char read_char;
  int read_result = HANDLE_EINTR(read(pipe_read_fd.get(), &read_char, 1));
  if (read_result != 1) {
    if (read_result < 0) {
      PLOG(ERROR) << "read";
    } else {
      LOG(ERROR) << "read: unexpected result " << read_result;
    }
    return false;
  }

  // Since a byte has been successfully read from the relauncher process, it's
  // guaranteed to have set up its kqueue monitoring this process for exit.
  // It's safe to exit now.
  return true;
}

namespace {

// In the relauncher process, performs the necessary synchronization steps
// with the parent by setting up a kqueue to watch for it to exit, writing a
// byte to the pipe, and then waiting for the exit notification on the kqueue.
// If anything fails, this logs a message and returns immediately. In those
// situations, it can be assumed that something went wrong with the parent
// process and the best recovery approach is to attempt relaunch anyway.
void RelauncherSynchronizeWithParent() {
  base::ScopedFD relauncher_sync_fd(kRelauncherSyncFD);

  int parent_pid = getppid();

  // PID 1 identifies init. launchd, that is. launchd never starts the
  // relauncher process directly, having this parent_pid means that the parent
  // already exited and launchd "inherited" the relauncher as its child.
  // There's no reason to synchronize with launchd.
  if (parent_pid == 1) {
    LOG(ERROR) << "unexpected parent_pid";
    return;
  }

  // Set up a kqueue to monitor the parent process for exit.
  base::ScopedFD kq(kqueue());
  if (!kq.is_valid()) {
    PLOG(ERROR) << "kqueue";
    return;
  }

  struct kevent change = { 0 };
  EV_SET(&change, parent_pid, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, NULL);
  if (kevent(kq.get(), &change, 1, nullptr, 0, nullptr) == -1) {
    PLOG(ERROR) << "kevent (add)";
    return;
  }

  // Write a '\0' character to the pipe.
  if (HANDLE_EINTR(write(relauncher_sync_fd.get(), "", 1)) != 1) {
    PLOG(ERROR) << "write";
    return;
  }

  // Up until now, the parent process was blocked in a read waiting for the
  // write above to complete. The parent process is now free to exit. Wait for
  // that to happen.
  struct kevent event;
  int events = kevent(kq.get(), nullptr, 0, &event, 1, nullptr);
  if (events != 1) {
    if (events < 0) {
      PLOG(ERROR) << "kevent (monitor)";
    } else {
      LOG(ERROR) << "kevent (monitor): unexpected result " << events;
    }
    return;
  }

  if (event.filter != EVFILT_PROC ||
      event.fflags != NOTE_EXIT ||
      event.ident != static_cast<uintptr_t>(parent_pid)) {
    LOG(ERROR) << "kevent (monitor): unexpected event, filter " << event.filter
               << ", fflags " << event.fflags << ", ident " << event.ident;
    return;
  }
}

}  // namespace

namespace internal {

int RelauncherMain(content::MainFunctionParams main_parameters) {
  @autoreleasepool {
    // CommandLine rearranges the order of the arguments returned by
    // main_parameters.argv(), rendering it impossible to determine which
    // arguments originally came before kRelauncherArgSeparator and which came
    // after. It's crucial to distinguish between these because only those
    // after the separator should be given to the relaunched process; it's also
    // important to not treat the path to the relaunched process as a "loose"
    // argument. NXArgc and NXArgv are pointers to the original argc and argv as
    // passed to main(), so use those. Access them through _NSGetArgc and
    // _NSGetArgv because NXArgc and NXArgv are normally only available to a
    // main executable via crt1.o and this code will run from a dylib, and
    // because of http://crbug.com/139902.
    const int* argcp = _NSGetArgc();
    if (!argcp) {
      NOTREACHED_IN_MIGRATION();
      return 1;
    }
    int argc = *argcp;

    const char* const* const* argvp = _NSGetArgv();
    if (!argvp) {
      NOTREACHED_IN_MIGRATION();
      return 1;
    }
    const char* const* argv = *argvp;

    if (argc < 4 || RelauncherTypeArg() != argv[1]) {
      LOG(ERROR) << "relauncher process invoked with unexpected arguments";
      return 1;
    }

    RelauncherSynchronizeWithParent();

    // The capacity for relaunch_args is 4 less than argc, because it won't
    // contain the argv[0] of the relauncher process, the RelauncherTypeArg() at
    // argv[1], kRelauncherArgSeparator, or the executable path of the process
    // to be launched.
    std::vector<std::string> relaunch_args;
    relaunch_args.reserve(argc - 4);

    // Figure out what to execute, what arguments to pass it, and whether to
    // start it activated.
    bool activate = true;
    bool in_relaunch_args = false;
    std::string dmg_bsd_device_name;
    bool seen_relaunch_executable = false;
    std::string relaunch_executable;
    const std::string relauncher_dmg_device_arg =
        base::StringPrintf("--%s=", switches::kRelauncherProcessDMGDevice);
    for (int argv_index = 2; argv_index < argc; ++argv_index) {
      const std::string arg(argv[argv_index]);

      // Strip any -psn_ arguments, as they apply to a specific process.
      if (arg.compare(0, strlen(kPSNArg), kPSNArg) == 0) {
        continue;
      }

      if (!in_relaunch_args) {
        if (arg == kRelauncherArgSeparator) {
          in_relaunch_args = true;
        } else if (arg == kRelauncherBackgroundArg) {
          activate = false;
        } else if (arg.compare(0, relauncher_dmg_device_arg.size(),
                               relauncher_dmg_device_arg) == 0) {
          dmg_bsd_device_name.assign(
              arg.substr(relauncher_dmg_device_arg.size()));
        }
      } else {
        if (!seen_relaunch_executable) {
          // The first argument after kRelauncherArgSeparator is the path to the
          // .app bundle directory. The Launch Services interface wants this
          // separate from the rest of the arguments. In the relaunched process,
          // the executable path will still be visible at argv[0].
          relaunch_executable.assign(arg);
          seen_relaunch_executable = true;
        } else {
          relaunch_args.push_back(arg);
        }
      }
    }

    if (!seen_relaunch_executable) {
      LOG(ERROR) << "nothing to relaunch";
      return 1;
    }

    base::mac::LaunchApplication(
        base::FilePath(relaunch_executable), relaunch_args, /*url_specs=*/{},
        {.activate = activate,
         .create_new_instance = true,
         .prompt_user_if_needed = true},
        base::BindOnce([](NSRunningApplication* app, NSError* error) {
          if (error) {
            LOG(ERROR) << "Failed to relaunch: "
                       << base::SysNSStringToUTF8(error.description);
          }

          CFRunLoopStop(CFRunLoopGetMain());
        }));

    // This is running the main thread.
    CFRunLoopRun();

    if (!dmg_bsd_device_name.empty()) {
      EjectAndTrashDiskImage(dmg_bsd_device_name);
    }

    return 0;
  }
}

}  // namespace internal

}  // namespace mac_relauncher

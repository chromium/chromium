// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The entry point for all Mac Chromium processes, including the outer app
// bundle (browser) and helper app (renderer, plugin, and friends).

#include <dlfcn.h>
#include <errno.h>
#include <libgen.h>
#include <mach-o/dyld.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <memory>

#include "chrome/common/chrome_version.h"

#if defined(HELPER_EXECUTABLE)
#include "sandbox/mac/seatbelt_exec.h"  // nogncheck
#endif  // defined(HELPER_EXECUTABLE)

extern "C" {
// abort_report_np() records the message in a special section that both the
// system CrashReporter and Crashpad collect in crash reports. Using a Crashpad
// Annotation would be preferable, but this executable cannot depend on
// Crashpad directly.
void abort_report_np(const char* fmt, ...);
}

namespace {

typedef int (*ChromeMainPtr)(int, char**);

[[noreturn]] void FatalError(const char* format, ...) {
  va_list valist;
  va_start(valist, format);
  char message[4096];
  if (vsnprintf(message, sizeof(message), format, valist) >= 0) {
    abort_report_np("%s", message);
  }
  va_end(valist);
  abort();
}

}  // namespace

static void (*gRecordReplayAttach)(const char* dispatchAddress, const char* buildId);
static void (*gRecordReplayRecordCommandLineArguments)(int*, char***);

template <typename Src, typename Dst>
static inline void CastPointer(const Src src, Dst* dst) {
  static_assert(sizeof(Src) == sizeof(uintptr_t), "bad size");
  static_assert(sizeof(Dst) == sizeof(uintptr_t), "bad size");
  memcpy((void*)dst, (const void*)&src, sizeof(uintptr_t));
}

template <typename T>
static void RecordReplayLoadSymbol(void* handle, const char* name, T& function) {
  void* sym = dlsym(handle, name);
  if (!sym) {
    fprintf(stderr, "Could not find %s in Record Replay driver.\n", name);
    return;
  }

  CastPointer(sym, &function);
}

static const char* gBuildId = "macOS-chromium-experimental";

static void RecordReplayAttach(int* pargc, char*** pargv) {
  const char* driver = getenv("RECORD_REPLAY_DRIVER");
  if (!driver) {
    // When not configured to record/replay, don't change anything.
    return;
  }

  // Figure out what type of process this is.
  char* type = nullptr;
  for (int i = 0; i < *pargc; i++) {
    if (!strncmp((*pargv)[i], "--type=", 7)) {
      type = (*pargv)[i] + 7;
      break;
    }
  }
  if (type) {
    // Only renderer processes are recorded/replayed.
    if (strcmp(type, "renderer")) {
      return;
    }
  } else {
    // If there is no type, this is the main process. Add a couple command line
    // arguments which are required to record/replay.
    char** nargv = new char*[*pargc + 3];
    memcpy(nargv, *pargv, *pargc * sizeof(char*));
    *pargv = nargv;

    // Recording processes currently need the sandbox disabled in order to
    // write out recording IDs to the specified path name.
    (*pargv)[*pargc] = strdup("--no-sandbox");

    // Recording/replaying currently requires software rendering.
    (*pargv)[*pargc + 1] = strdup("--disable-gpu");

    (*pargv)[*pargc + 2] = nullptr;
    *pargc += 2;
    return;
  }

  const char* dispatchAddress = getenv("RECORD_REPLAY_DISPATCH");
  if (!dispatchAddress) {
    fprintf(stderr, "RECORD_REPLAY_DISPATCH not set.\n");
    return;
  }

  void* handle = dlopen(driver, RTLD_LAZY);
  if (!handle) {
    fprintf(stderr, "Loading Record Replay driver failed.\n");
    return;
  }

  RecordReplayLoadSymbol(handle, "RecordReplayAttach", gRecordReplayAttach);
  RecordReplayLoadSymbol(handle, "RecordReplayRecordCommandLineArguments",
                         gRecordReplayRecordCommandLineArguments);

  if (gRecordReplayAttach) {
    gRecordReplayAttach(dispatchAddress, gBuildId);
    gRecordReplayRecordCommandLineArguments(pargc, pargv);
  }
}

__attribute__((visibility("default"))) int main(int argc, char* argv[]) {
  RecordReplayAttach(&argc, &argv);

  uint32_t exec_path_size = 0;
  int rv = _NSGetExecutablePath(NULL, &exec_path_size);
  if (rv != -1) {
    FatalError("_NSGetExecutablePath: get length failed.");
  }

  std::unique_ptr<char[]> exec_path(new char[exec_path_size]);
  rv = _NSGetExecutablePath(exec_path.get(), &exec_path_size);
  if (rv != 0) {
    FatalError("_NSGetExecutablePath: get path failed.");
  }

#if defined(HELPER_EXECUTABLE)
  sandbox::SeatbeltExecServer::CreateFromArgumentsResult seatbelt =
      sandbox::SeatbeltExecServer::CreateFromArguments(exec_path.get(), argc,
                                                       argv);
  if (seatbelt.sandbox_required) {
    if (!seatbelt.server) {
      FatalError("Failed to create seatbelt sandbox server.");
    }
    if (!seatbelt.server->InitializeSandbox()) {
      FatalError("Failed to initialize sandbox.");
    }
  }

  // The helper lives within the versioned framework directory, so simply
  // go up to find the main dylib.
  const char rel_path[] = "../../../../" PRODUCT_FULLNAME_STRING " Framework";
#else
  const char rel_path[] = "../Frameworks/" PRODUCT_FULLNAME_STRING
                          " Framework.framework/Versions/" CHROME_VERSION_STRING
                          "/" PRODUCT_FULLNAME_STRING " Framework";
#endif  // defined(HELPER_EXECUTABLE)

  // Slice off the last part of the main executable path, and append the
  // version framework information.
  const char* parent_dir = dirname(exec_path.get());
  if (!parent_dir) {
    FatalError("dirname %s: %s.", exec_path.get(), strerror(errno));
  }

  const size_t parent_dir_len = strlen(parent_dir);
  const size_t rel_path_len = strlen(rel_path);
  // 2 accounts for a trailing NUL byte and the '/' in the middle of the paths.
  const size_t framework_path_size = parent_dir_len + rel_path_len + 2;
  std::unique_ptr<char[]> framework_path(new char[framework_path_size]);
  snprintf(framework_path.get(), framework_path_size, "%s/%s", parent_dir,
           rel_path);

  void* library =
      dlopen(framework_path.get(), RTLD_LAZY | RTLD_LOCAL | RTLD_FIRST);
  if (!library) {
    FatalError("dlopen %s: %s.", framework_path.get(), dlerror());
  }

  const ChromeMainPtr chrome_main =
      reinterpret_cast<ChromeMainPtr>(dlsym(library, "ChromeMain"));
  if (!chrome_main) {
    FatalError("dlsym ChromeMain: %s.", dlerror());
  }
  rv = chrome_main(argc, argv);

  // exit, don't return from main, to avoid the apparent removal of main from
  // stack backtraces under tail call optimization.
  exit(rv);
}

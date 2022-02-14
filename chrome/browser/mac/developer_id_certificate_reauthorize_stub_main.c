// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

#include "chrome/common/chrome_version.h"

// abort_report_np() records the message in a special section that both the
// system CrashReporter and Crashpad collect in crash reports. Using a Crashpad
// Annotation would be preferable, but this executable cannot depend on Crashpad
// directly.
void abort_report_np(const char* fmt, ...);

__attribute__((noreturn)) static void FatalError(const char* format, ...) {
  va_list valist;
  va_start(valist, format);
  char message[4096];
  int rv = vsnprintf(message, sizeof(message), format, valist);
  va_end(valist);
  if (rv >= 0) {
    fprintf(stderr, "%s\n", message);
    fflush(stderr);
    abort_report_np("%s", message);
  }
  abort();
}

__attribute__((visibility("default"))) int main(int argc, char* argv[]) {
  uint32_t exec_path_size = 0;
  int rv = _NSGetExecutablePath(NULL, &exec_path_size);
  if (rv != -1) {
    FatalError("_NSGetExecutablePath: get length failed");
  }

  char* exec_path = malloc(exec_path_size);
  if (!exec_path) {
    FatalError("malloc %u: %s", exec_path_size, strerror(errno));
  }
  rv = _NSGetExecutablePath(exec_path, &exec_path_size);
  if (rv != 0) {
    FatalError("_NSGetExecutablePath: get path failed");
  }

  // The Developer ID certificate reauthorizer stub lives within the Helpers
  // directory of the versioned framework directory, but is not itself bundled,
  // so it's only one level deeper than the dylib.
  const char rel_path[] = "../" PRODUCT_FULLNAME_STRING " Framework";

  // Slice off the last part of the main executable path, and append the path to
  // the framework dylib.
  const char* parent_dir = dirname(exec_path);
  if (!parent_dir) {
    FatalError("dirname %s: %s", exec_path, strerror(errno));
  }

  free(exec_path);

  const size_t parent_dir_len = strlen(parent_dir);
  const size_t rel_path_len = strlen(rel_path);

  // 2 accounts for a trailing NUL byte and the '/' in the middle of the paths.
  const size_t framework_path_size = parent_dir_len + rel_path_len + 2;
  char* framework_path = malloc(framework_path_size);
  if (!framework_path) {
    FatalError("malloc %zu: %s", framework_path_size, strerror(errno));
  }
  snprintf(framework_path, framework_path_size, "%s/%s", parent_dir, rel_path);

  void* library = dlopen(framework_path, RTLD_LAZY | RTLD_LOCAL | RTLD_FIRST);
  if (!library) {
    FatalError("dlopen %s: %s", framework_path, dlerror());
  }

  free(framework_path);

  static const char kMainSymbol[] = "DeveloperIDCertificateReauthorizeFromStub";

  typedef int (*MainFunctionType)(int, const char* const*);
  const MainFunctionType main_function = dlsym(library, kMainSymbol);
  if (!main_function) {
    FatalError("dlsym %s: %s", kMainSymbol, dlerror());
  }
  rv = main_function(argc, (const char* const*)argv);

  // exit, don't return from main, to avoid the apparent removal of main from
  // stack backtraces under tail call optimization.
  exit(rv);
}

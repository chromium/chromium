// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

// This file implements BSD-style setproctitle() for Linux.
// It is written such that it can easily be compiled outside Chromium.
//
// The Linux kernel sets up two locations in memory to pass arguments and
// environment variables to processes. First, there are two char* arrays stored
// one after another: argv and environ. A pointer to argv is passed to main(),
// while glibc sets the global variable |environ| to point at the latter. Both
// of these arrays are terminated by a null pointer; the environment array is
// also followed by some empty space to allow additional variables to be added.
//
// These arrays contain pointers to a second location in memory, where the
// strings themselves are stored one after another: first all the arguments,
// then the environment variables.
//
// When the kernel reads the command line arguments for a process, it looks at
// the range of memory that it initially used for the argument list. If the
// terminating '\0' character is still where it expects, nothing further is
// done. If it has been overwritten, the kernel will scan up to the size of
// a page looking for another.
//
// Thus to change the process title, we must move any environment variables out
// of the way to make room for a potentially longer title, and then overwrite
// the memory pointed to by argv[0] with a single replacement string, making
// sure its size does not exceed the available space.
//
// See the following kernel commit for the details of the contract between
// kernel and setproctitle:
// https://github.com/torvalds/linux/commit/2954152298c37804dab49d630aa959625b50cf64
//
// It is perhaps worth noting that patches to add a system call to Linux for
// this, like in BSD, have never made it in: this is the "official" way to do
// this on Linux. Presumably it is not in glibc due to some disagreement over
// this position within the glibc project, leaving applications caught in the
// middle. (Also, only a very few applications need or want this anyway.)

#include "base/process/set_process_title_linux.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"

extern char** environ;

// g_orig_argv0 is the original process name found in argv[0].
// It is set to a copy of argv[0] in setproctitle_init. It is nullptr if
// setproctitle_init was unsuccessful or not called.
static const char* g_orig_argv0 = nullptr;

// Following pointers hold the initial argv/envp memory range.
// They are initialized in setproctitle_init and are used to overwrite the
// argv/envp memory range with a new process title to be read by the kernel.
// They are nullptr if setproctitle_init was unsuccessful or not called.
// Note that g_envp_start is not necessary because it is the same as g_argv_end.
static char* g_argv_start = nullptr;
static char* g_argv_end = nullptr;
static char* g_envp_end = nullptr;

void setproctitle(const char* fmt, ...) {
  va_list ap;

  // Sanity check before we try and set the process title.
  // The BSD version allows a null fmt to restore the original title.
  if (!g_orig_argv0 || !fmt)
    return;

  // The title can be up to the end of envp.
  const size_t avail_size =
      base::checked_cast<size_t>(g_envp_end - g_argv_start - 1);

  // Linux 4.18--5.2 have a bug where we can never set a process title
  // shorter than the initial argv. Check if the bug exists in the current
  // kernel on the first call of setproctitle.
  static const bool buggy_kernel = [avail_size] {
    // Attempt to set an empty title. This will set cmdline to:
    // ""                   (on Linux --4.17)
    // "\0\0\0...\0\0\0.\0" (on Linux 4.18--5.2)
    // "\0"                 (on Linux 5.3--)
    memset(g_argv_start, 0, avail_size + 1);
    g_argv_end[-1] = '.';

    std::string cmdline;
    if (!base::ReadFileToString(base::FilePath("/proc/self/cmdline"),
                                &cmdline)) {
      return false;
    }
    return cmdline.size() >= 2;
  }();

  memset(g_argv_start, 0, avail_size + 1);

  size_t size;
  va_start(ap, fmt);
  if (fmt[0] == '-') {
    size = base::checked_cast<size_t>(
        vsnprintf(g_argv_start, avail_size, &fmt[1], ap));
  } else {
    size = base::checked_cast<size_t>(
        snprintf(g_argv_start, avail_size, "%s ", g_orig_argv0));
    if (size < avail_size)
      size += base::checked_cast<size_t>(
          vsnprintf(&g_argv_start[size], avail_size - size, fmt, ap));
  }
  va_end(ap);

  // Kernel looks for a null terminator instead of the initial argv space
  // when the end of the space is not terminated with a null.
  // https://github.com/torvalds/linux/commit/d26d0cd97c88eb1a5704b42e41ab443406807810
  //
  // If the length of the new title is shorter than the original argv space,
  // set the last byte of the space to an arbitrary non-null character to tell
  // the kernel that setproctitle was called.
  //
  // On buggy kernels we can never make the process title shorter than the
  // initial argv. In that case, just leave the remaining bytes filled with
  // null characters.
  const size_t argv_size =
      base::checked_cast<size_t>(g_argv_end - g_argv_start - 1);
  if (!buggy_kernel && size < argv_size)
    g_argv_end[-1] = '.';
}

// A version of this built into glibc would not need this function, since
// it could stash the argv pointer in __libc_start_main(). But we need it.
void setproctitle_init(const char** main_argv) {
  static bool init_called = false;
  if (init_called)
    return;
  init_called = true;

  if (!main_argv)
    return;

  // Verify that the memory layout matches expectation.
  char** argv = const_cast<char**>(main_argv);
  char* argv_start = argv[0];
  char* p = argv_start;
  for (size_t i = 0; argv[i]; ++i) {
    if (p != argv[i])
      return;
    p += strlen(p) + 1;
  }
  char* argv_end = p;
  size_t environ_size = 0;
  for (size_t i = 0; environ[i]; ++i, ++environ_size) {
    if (p != environ[i])
      return;
    p += strlen(p) + 1;
  }
  char* envp_end = p;

  // Move the environment out of the way. Note that we are moving the values,
  // not the environment array itself. Also note that we preallocate the entire
  // vector, because a string's underlying data pointer is not stable under
  // move operations, which could otherwise occur if building up the vector
  // incrementally.
  static base::NoDestructor<std::vector<std::string>> environ_copy(
      environ_size);
  for (size_t i = 0; environ[i]; ++i) {
    (*environ_copy)[i] = environ[i];
    environ[i] = &(*environ_copy)[i][0];
  }

  if (!argv[0])
    return;

  static base::NoDestructor<std::string> argv0_storage(argv[0]);
  g_orig_argv0 = argv0_storage->data();
  g_argv_start = argv_start;
  g_argv_end = argv_end;
  g_envp_end = envp_end;
}

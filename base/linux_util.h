// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LINUX_UTIL_H_
#define BASE_LINUX_UTIL_H_

#include <stdint.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include "base/base_export.h"

namespace base {

// This is declared here so the crash reporter can access the memory directly
// in compromised context without going through the standard library.
BASE_EXPORT extern char g_linux_distro[];

// Get the Linux Distro if we can, or return "Unknown".
BASE_EXPORT std::string GetLinuxDistro();

#if defined(UNIT_TEST)
// Get the value of given key from the given input (content of the
// /etc/os-release file. Exposed for testing.
BASE_EXPORT std::string GetKeyValueFromOSReleaseFileForTesting(
    const std::string& input,
    const char* key);
#endif  // defined(UNIT_TEST)

// Set the Linux Distro string.
BASE_EXPORT void SetLinuxDistro(const std::string& distro);

// For a given process |pid|, get a list of all its threads. On success, returns
// true and appends the list of threads to |tids|. Otherwise, returns false.
BASE_EXPORT bool GetThreadsForProcess(pid_t pid, std::vector<pid_t>* tids);

// Get a list of all threads for the current process. On success, returns true
// and appends the list of threads to |tids|. Otherwise, returns false.
// Unlike the function above, this function reads /proc/self/tasks, not
// /proc/<pid>/tasks. On Android, the former should always be accessible to
// GPU and Browser processes, while the latter may or may not be accessible
// depending on the system and the app configuration.
BASE_EXPORT bool GetThreadsForCurrentProcess(std::vector<pid_t>* tids);

// For a given process |pid|, look through all its threads and find the first
// thread with /proc/[pid]/task/[thread_id]/syscall whose first N bytes matches
// |expected_data|, where N is the length of |expected_data|.
// Returns the thread id or -1 on error.  If |syscall_supported| is
// set to false the kernel does not support syscall in procfs.
BASE_EXPORT pid_t FindThreadIDWithSyscall(pid_t pid,
                                          const std::string& expected_data,
                                          bool* syscall_supported);

// For a given process `pid` and one of its threads `tid` (both as seen from the
// current PID namespace), reads /proc/[pid]/task/[tid]/status and returns the
// thread's id in the innermost PID namespace of `pid`, i.e. what gettid()
// returns in that thread. Returns -1 if `tid` is not (or no longer) a thread of
// `pid`, or if the kernel does not report NSpid in procfs.
BASE_EXPORT pid_t GetNamespaceThreadId(pid_t pid, pid_t tid);

}  // namespace base

#endif  // BASE_LINUX_UTIL_H_

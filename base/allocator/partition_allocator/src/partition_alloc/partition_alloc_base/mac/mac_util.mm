// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/mac/mac_util.h"

#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include <cstddef>
#include <cstring>

#include "partition_alloc/partition_alloc_base/check.h"
#include "partition_alloc/partition_alloc_base/logging.h"

// This is a simplified version of base::mac. Because
// "base/strings/string_split.h" is unavailable, only provide access to the
// macOS major version number via direct string work on the Darwin version.

namespace partition_alloc::internal::base::mac {

namespace {

// Returns the running system's Darwin major version. Don't call this, it's an
// implementation detail and its result is meant to be cached by
// MacOSMajorVersion().
int DarwinMajorVersion() {
  // base::OperatingSystemVersionNumbers() at one time called Gestalt(), which
  // was observed to be able to spawn threads (see https://crbug.com/53200).
  // Nowadays that function calls -[NSProcessInfo operatingSystemVersion], whose
  // current implementation does things like hit the file system, which is
  // possibly a blocking operation. Either way, it's overkill for what needs to
  // be done here.
  //
  // uname, on the other hand, is implemented as a simple series of sysctl
  // system calls to obtain the relevant data from the kernel. The data is
  // compiled right into the kernel, so no threads or blocking or other
  // funny business is necessary.

  struct utsname uname_info;
  if (uname(&uname_info) != 0) {
    PA_DPLOG(ERROR) << "uname";
    return 0;
  }

  if (strcmp(uname_info.sysname, "Darwin") != 0) {
    PA_DLOG(ERROR) << "unexpected uname sysname " << uname_info.sysname;
    return 0;
  }

  const char* dot = strchr(uname_info.release, '.');
  if (!dot || uname_info.release == dot ||
      // Darwin version should be 1 or 2 digits, it's unlikely to be more than
      // 4 digits.
      dot - uname_info.release > 4) {
    PA_DLOG(ERROR) << "could not parse uname release " << uname_info.release;
    return 0;
  }

  int darwin_major_version = 0;
  constexpr int base = 10;
  for (const char* p = uname_info.release; p < dot; ++p) {
    if (!('0' <= *p && *p < '0' + base)) {
      PA_DLOG(ERROR) << "could not parse uname release " << uname_info.release;
      return 0;
    }

    // Since we checked the number of digits is 4 at most (see above), there is
    // no chance to overflow.
    darwin_major_version *= base;
    darwin_major_version += *p - '0';
  }

  return darwin_major_version;
}

}  // namespace

int MacOSMajorVersion() {
  static int macos_major_version = [] {
    int darwin_major_version = DarwinMajorVersion();

    // Darwin major versions 6 through 19 corresponded to macOS versions 10.2
    // through 10.15.
    PA_BASE_CHECK(darwin_major_version >= 6);
    if (darwin_major_version <= 19) {
      return 10;
    }

    // Darwin major version 20 corresponds to macOS version 11.0. Assume a
    // correspondence between Darwin's major version numbers and macOS major
    // version numbers.
    return darwin_major_version - 9;
  }();
  return macos_major_version;
}

}  // namespace partition_alloc::internal::base::mac

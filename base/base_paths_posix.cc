// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines base::PathProviderPosix, default path provider on POSIX OSes that
// don't have their own base_paths_OS.cc implementation (i.e. all but Mac and
// Android).

#include "base/base_paths.h"

#include <limits.h>
#include <stddef.h>

#include <memory>
#include <ostream>
#include <string>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/posix/sysctl.h"
#include "base/process/process_metrics.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_FREEBSD)
#include <sys/param.h>
#include <sys/sysctl.h>
#elif BUILDFLAG(IS_SOLARIS) || BUILDFLAG(IS_AIX)
#include <stdlib.h>
#endif

namespace base {

bool PathProviderPosix(int key, FilePath* result) {
  switch (key) {
    case FILE_EXE:
    case FILE_MODULE: {  // TODO(evanm): is this correct?
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
      FilePath bin_dir;
      if (!ReadSymbolicLink(FilePath(kProcSelfExe), &bin_dir)) {
        NOTREACHED() << "Unable to resolve " << kProcSelfExe << ".";
      }
      *result = bin_dir;
      return true;
#elif BUILDFLAG(IS_FREEBSD)
      int name[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
      std::optional<std::string> bin_dir = StringSysctl(name, std::size(name));
      if (!bin_dir.has_value() || bin_dir.value().length() <= 1) {
        NOTREACHED() << "Unable to resolve path.";
      }
      *result = FilePath(bin_dir.value());
      return true;
#elif BUILDFLAG(IS_SOLARIS)
      char bin_dir[PATH_MAX + 1];
      if (realpath(getexecname(), bin_dir) == NULL) {
        NOTREACHED() << "Unable to resolve " << getexecname() << ".";
      }
      *result = FilePath(bin_dir);
      return true;
#elif BUILDFLAG(IS_OPENBSD) || BUILDFLAG(IS_AIX)
      // There is currently no way to get the executable path on OpenBSD
      char* cpath;
      if ((cpath = getenv("CHROME_EXE_PATH")) != NULL)
        *result = FilePath(cpath);
      else
        *result = FilePath("/usr/local/chrome/chrome");
      return true;
#endif
    }
    case DIR_SRC_TEST_DATA_ROOT: {
      FilePath path;
      // On POSIX, unit tests execute two levels deep from the source root.
      // For example:  out/{Debug|Release}/net_unittest
      if (PathService::Get(DIR_EXE, &path)) {
        *result = path.DirName().DirName();
        return true;
      }

      DLOG(ERROR) << "Couldn't find your source root.  "
                  << "Try running from your chromium/src directory.";
      return false;
    }
    case DIR_USER_DESKTOP:
      *result = nix::GetXDGUserDirectory("DESKTOP", "Desktop");
      return true;
    case DIR_CACHE: {
      std::unique_ptr<Environment> env(Environment::Create());
      FilePath cache_dir(
          nix::GetXDGDirectory(env.get(), "XDG_CACHE_HOME", ".cache"));
      *result = cache_dir;
      return true;
    }
  }
  return false;
}

}  // namespace base

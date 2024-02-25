// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines base::PathProviderAndroid which replaces base::PathProviderPosix for
// Android in base/path_service.cc.

#include <limits.h>
#include <unistd.h>

#include <ostream>

#include "base/android/jni_android.h"
#include "base/android/path_utils.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/process/process_metrics.h"

namespace base {

bool PathProviderAndroid(int key, FilePath* result) {
  switch (key) {
    case base::FILE_EXE: {
      FilePath bin_dir;
      if (!ReadSymbolicLink(FilePath(kProcSelfExe), &bin_dir)) {
        // This fails for some devices (maybe custom OEM selinux policy?)
        // https://crbug.com/1416753
        LOG(ERROR) << "Unable to resolve " << kProcSelfExe << ".";
        return false;
      }
      *result = bin_dir;
      return true;
    }
    case base::FILE_MODULE:
      // dladdr didn't work in Android as only the file name was returned.
      NOTIMPLEMENTED();
      return false;
    case base::DIR_MODULE:
      return base::android::GetNativeLibraryDirectory(result);
    case base::DIR_SRC_TEST_DATA_ROOT:
    case base::DIR_OUT_TEST_DATA_ROOT:
      // These are only used by tests. In that context, they are overridden by
      // PathProviders in //base/test/test_support_android.cc.
      NOTIMPLEMENTED();
      return false;
    case base::DIR_USER_DESKTOP:
      // Android doesn't support GetUserDesktop.
      NOTIMPLEMENTED();
      return false;
    case base::DIR_CACHE:
      return base::android::GetCacheDirectory(result);
    case base::DIR_ASSETS:
      // On Android assets are normally loaded from the APK using
      // base::android::OpenApkAsset(). In tests, since the assets are no
      // packaged, DIR_ASSETS is overridden to point to the build directory.
      return false;
    case base::DIR_ANDROID_APP_DATA:
      return base::android::GetDataDirectory(result);
    case base::DIR_ANDROID_EXTERNAL_STORAGE:
      return base::android::GetExternalStorageDirectory(result);
  }

  // For all other keys, let the PathService fall back to a default, if defined.
  return false;
}

}  // namespace base

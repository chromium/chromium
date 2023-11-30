// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/file_system_access_features.h"

#include "base/feature_list.h"

namespace features {

#if BUILDFLAG(IS_WIN)
// Enables blocking local UNC path on Windows for the File System Access API.
BASE_FEATURE(kFileSystemAccessLocalUNCPathBlock,
             "kFileSystemAccessLocalUNCPathBlock",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Enables persistent permissions for the File System Access API.
// TODO(crbug.com/1467574): Remove `kFileSystemAccessPersistentPermissions`
// flag after FSA Persistent Permissions feature launch.
BASE_FEATURE(kFileSystemAccessPersistentPermissions,
             "kFileSystemAccessPersistentPermissions",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

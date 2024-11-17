// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/file_system_access_features.h"

#include "base/feature_list.h"

namespace features {

// Enables persistent permissions for the File System Access API.
BASE_FEATURE(kFileSystemAccessPersistentPermissions,
             "kFileSystemAccessPersistentPermissions",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Enables performing the blocklist check on a full absolute path, which
// resolves any symbolic link.
BASE_FEATURE(kFileSystemAccessSymbolicLinkCheck,
             "FileSystemAccessSymbolicLinkCheck",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features

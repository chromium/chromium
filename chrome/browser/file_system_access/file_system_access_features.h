// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FEATURES_H_
#define CHROME_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FEATURES_H_

#include "base/feature_list.h"

namespace features {

// Alphabetical:

BASE_DECLARE_FEATURE(kFileSystemAccessPersistentPermissions);

BASE_DECLARE_FEATURE(kFileSystemAccessSymbolicLinkCheck);
}  // namespace features

#endif  // CHROME_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FEATURES_H_

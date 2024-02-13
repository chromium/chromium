// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_LOCAL_USER_FILES_POLICY_UTILS_H_
#define CHROME_BROWSER_ASH_POLICY_LOCAL_USER_FILES_POLICY_UTILS_H_

namespace policy::local_user_files {

// Returns whether local user files are enabled on the device by the flag and
// policy.
bool LocalUserFilesAllowed();

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_ASH_POLICY_LOCAL_USER_FILES_POLICY_UTILS_H_

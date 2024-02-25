// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_FILE_UTIL_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_FILE_UTIL_H_

#include <string>

namespace policy {

// Returns the name to an unique sub-directory based on the `account_id`.
// The returned directory is guaranteed to always be the same when invoked
// with the same `account_id`, even across system reboots.
std::string GetUniqueSubDirectoryForAccountID(const std::string& account_id);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_FILE_UTIL_H_

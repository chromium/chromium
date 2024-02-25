// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_FILES_POLICY_STRING_UTIL_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_FILES_POLICY_STRING_UTIL_H_

#include <string>

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"

namespace policy::files_string_util {
// Returns the title for notification or dialog informing the user that the
// `action` on `file_count` files is blocked due to policy.
std::u16string GetBlockTitle(dlp::FileAction action, size_t file_count);

// Returns the title for notification or dialog informing the user that the
// file `action` is not advised due to policy.
std::u16string GetWarnTitle(dlp::FileAction action);

// Returns the "proceed" button label for notification or dialog informing the
// user that the file `action` is not advised due to policy.
std::u16string GetContinueAnywayButton(dlp::FileAction action);

// Returns the message for notification or dialog informing the user that the
// file action on `file_count` files is blocked due to `reason`.
std::u16string GetBlockReasonMessage(FilesPolicyDialog::BlockReason reason,
                                     size_t file_count);

// Returns the message for notification or dialog informing the user that the
// `first_file` is blocked due to `reason`.
std::u16string GetBlockReasonMessage(FilesPolicyDialog::BlockReason reason,
                                     const std::u16string& first_file);

}  // namespace policy::files_string_util

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_FILES_POLICY_STRING_UTIL_H_

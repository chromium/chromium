// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/files_policy_string_util.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy::files_string_util {

std::u16string GetBlockTitle(dlp::FileAction action, size_t file_count) {
  int message_id;
  std::u16string message;
  switch (action) {
    case dlp::FileAction::kDownload:
      message_id = IDS_POLICY_DLP_FILES_DOWNLOAD_BLOCKED_TITLE;
      break;
    case dlp::FileAction::kUpload:
      message_id = IDS_POLICY_DLP_FILES_UPLOAD_BLOCKED_TITLE;
      break;
    case dlp::FileAction::kCopy:
      message_id = IDS_POLICY_DLP_FILES_COPY_BLOCKED_TITLE;
      break;
    case dlp::FileAction::kMove:
      message_id = IDS_POLICY_DLP_FILES_MOVE_BLOCKED_TITLE;
      break;
    case dlp::FileAction::kOpen:
    case dlp::FileAction::kShare:
      message_id = IDS_POLICY_DLP_FILES_OPEN_BLOCKED_TITLE;
      break;
    case dlp::FileAction::kUnknown:
    // kUnknown is used for internal checks - treat as kTransfer.
    case dlp::FileAction::kTransfer:
      message_id = IDS_POLICY_DLP_FILES_TRANSFER_BLOCKED_TITLE;
      break;
  }
  message = l10n_util::GetPluralStringFUTF16(message_id, file_count);
  return file_count == 1 ? message
                         : base::ReplaceStringPlaceholders(
                               message, base::NumberToString16(file_count),
                               /*offset=*/nullptr);
}

std::u16string GetWarnTitle(dlp::FileAction action) {
  switch (action) {
    case dlp::FileAction::kDownload:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_DOWNLOAD_REVIEW_TITLE);
    case dlp::FileAction::kUpload:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_UPLOAD_REVIEW_TITLE);
    case dlp::FileAction::kCopy:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_COPY_REVIEW_TITLE);
    case dlp::FileAction::kMove:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_MOVE_REVIEW_TITLE);
    case dlp::FileAction::kOpen:
    case dlp::FileAction::kShare:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_OPEN_REVIEW_TITLE);
    case dlp::FileAction::kUnknown:
    // kUnknown is used for internal checks - treat as kTransfer.
    case dlp::FileAction::kTransfer:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_TRANSFER_REVIEW_TITLE);
  }
}

std::u16string GetContinueAnywayButton(dlp::FileAction action) {
  switch (action) {
    case dlp::FileAction::kDownload:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_DOWNLOAD_WARN_CONTINUE_BUTTON);
    case dlp::FileAction::kUpload:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_UPLOAD_WARN_CONTINUE_BUTTON);
    case dlp::FileAction::kCopy:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_COPY_WARN_CONTINUE_BUTTON);
    case dlp::FileAction::kMove:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_MOVE_WARN_CONTINUE_BUTTON);
    case dlp::FileAction::kOpen:
    case dlp::FileAction::kShare:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_OPEN_WARN_CONTINUE_BUTTON);
    case dlp::FileAction::kUnknown:
    // kUnknown is used for internal checks - treat as kTransfer.
    case dlp::FileAction::kTransfer:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_TRANSFER_WARN_CONTINUE_BUTTON);
  }
}

std::u16string GetBlockReasonMessage(Policy policy,
                                     size_t file_count,
                                     const std::u16string& first_file) {
  int message_id;
  const std::u16string placeholder_value =
      file_count == 1 ? first_file : base::NumberToString16(file_count);
  switch (policy) {
    case Policy::kDlp:
      message_id = IDS_POLICY_DLP_FILES_POLICY_BLOCK_MESSAGE;
      break;
    case Policy::kEnterpriseConnectors:
      message_id = IDS_POLICY_DLP_FILES_CONTENT_BLOCK_MESSAGE;
      break;
  }
  return base::ReplaceStringPlaceholders(
      l10n_util::GetPluralStringFUTF16(message_id, file_count),
      placeholder_value,
      /*offset=*/nullptr);
}
}  // namespace policy::files_string_util

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/camera_notification_util.h"

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/mime_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy::skyvault_ui_utils {

namespace {

constexpr auto kCameraSigninTitleMap =
    base::MakeFixedFlatMap<std::string, TitleAndMessageIds>(
        {{"image/jpeg",
          {IDS_POLICY_SKYVAULT_CAMERA_SIGN_IN_TITLE_PHOTO,
           IDS_POLICY_SKYVAULT_CAMERA_SIGN_IN_MESSAGE_PHOTO}},
         {"image/gif",
          {IDS_POLICY_SKYVAULT_CAMERA_SIGN_IN_TITLE_VIDEO,
           IDS_POLICY_SKYVAULT_CAMERA_SIGN_IN_MESSAGE_VIDEO}},
         {"video/mp4",
          {IDS_POLICY_SKYVAULT_CAMERA_SIGN_IN_TITLE_VIDEO,
           IDS_POLICY_SKYVAULT_CAMERA_SIGN_IN_MESSAGE_VIDEO}},
         {"application/pdf",
          {IDS_POLICY_SKYVAULT_CAMERA_SIGN_IN_TITLE_SCAN,
           IDS_POLICY_SKYVAULT_CAMERA_SIGN_IN_MESSAGE_SCAN}}});

}  // namespace

TitleAndMessageIds GetCameraSignInStringsFromFilename(
    const base::FilePath& file) {
  std::string mime_type;
  if (net::GetWellKnownMimeTypeFromFile(file, &mime_type)) {
    if (const auto it = kCameraSigninTitleMap.find(mime_type);
        it != kCameraSigninTitleMap.end()) {
      return it->second;
    }
  }
  DLOG(FATAL) << "Unsupported extension: " << file.Extension();
  // Use "photo" as fallback.
  return {IDS_POLICY_SKYVAULT_CAMERA_SIGN_IN_TITLE_PHOTO,
          IDS_POLICY_SKYVAULT_CAMERA_SIGN_IN_MESSAGE_PHOTO};
}

}  // namespace policy::skyvault_ui_utils

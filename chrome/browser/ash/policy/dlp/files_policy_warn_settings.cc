// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/files_policy_warn_settings.h"

namespace policy {

FilesPolicyWarnSettings::FilesPolicyWarnSettings()
    : bypass_requires_justification(false) {}

FilesPolicyWarnSettings::~FilesPolicyWarnSettings() = default;

FilesPolicyWarnSettings::FilesPolicyWarnSettings(
    const FilesPolicyWarnSettings& other) = default;

FilesPolicyWarnSettings& FilesPolicyWarnSettings::operator=(
    FilesPolicyWarnSettings&& other) = default;

bool FilesPolicyWarnSettings::operator==(
    const FilesPolicyWarnSettings& other) const {
  return bypass_requires_justification == other.bypass_requires_justification &&
         warning_message == other.warning_message &&
         learn_more_url == other.learn_more_url;
}

bool FilesPolicyWarnSettings::operator!=(
    const FilesPolicyWarnSettings& other) const {
  return !(*this == other);
}

}  // namespace policy

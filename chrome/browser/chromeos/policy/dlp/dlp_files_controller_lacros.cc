// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller_lacros.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths_lacros.h"

namespace policy {

DlpFilesControllerLacros::DlpFilesControllerLacros(
    const DlpRulesManager& rules_manager)
    : DlpFilesController(rules_manager) {}
DlpFilesControllerLacros::~DlpFilesControllerLacros() = default;

// TODO(b/283764626): Add OneDrive component
absl::optional<data_controls::Component>
DlpFilesControllerLacros::MapFilePathtoPolicyComponent(
    Profile* profile,
    const base::FilePath& file_path) {
  base::FilePath reference;

  if (chrome::GetAndroidFilesPath(&reference) &&
      reference.IsParent(file_path)) {
    return data_controls::Component::kArc;
  }

  if (chrome::GetRemovableMediaPath(&reference) &&
      reference.IsParent(file_path)) {
    return data_controls::Component::kUsb;
  }

  if (chrome::GetDriveFsMountPointPath(&reference) &&
      reference.IsParent(file_path)) {
    return data_controls::Component::kDrive;
  }

  if (chrome::GetLinuxFilesPath(&reference) &&
      (reference == file_path || reference.IsParent(file_path))) {
    return data_controls::Component::kCrostini;
  }

  return {};
}

}  // namespace policy

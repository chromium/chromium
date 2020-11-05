// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scanning/scanning_paths_provider_impl.h"

#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

constexpr char kRoot[] = "root";

}  // namespace

ScanningPathsProviderImpl::ScanningPathsProviderImpl() = default;
ScanningPathsProviderImpl::~ScanningPathsProviderImpl() = default;

std::string ScanningPathsProviderImpl::GetBaseNameFromPath(
    content::WebUI* web_ui,
    const base::FilePath& path) {
  Profile* profile = Profile::FromWebUI(web_ui);
  auto* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile);
  bool drive_available = integration_service &&
                         integration_service->is_enabled() &&
                         integration_service->IsMounted();

  if (drive_available &&
      integration_service->GetMountPointPath().Append(kRoot) == path)
    return l10n_util::GetStringUTF8(IDS_SCANNING_APP_MY_DRIVE);

  if (file_manager::util::GetMyFilesFolderForProfile(profile) == path)
    return l10n_util::GetStringUTF8(IDS_SCANNING_APP_MY_FILES_SELECT_OPTION);

  return path.BaseName().value();
}

}  // namespace chromeos

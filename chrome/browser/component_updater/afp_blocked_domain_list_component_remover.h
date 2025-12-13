// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_AFP_BLOCKED_DOMAIN_LIST_COMPONENT_REMOVER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_AFP_BLOCKED_DOMAIN_LIST_COMPONENT_REMOVER_H_

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"

namespace component_updater {

class ComponentUpdateService;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(InstallationResult)
enum class InstallationResult {
  // kSuccess = 0,
  // kMissingBlocklistFileError = 1,
  // kRulesetFormatError = 2,
  // The component directory was found and successfully deleted.
  kDeletionSuccess = 3,
  // The component directory was found but could not be deleted.
  kDeletionFailure = 4,
  // The component directory was not found. This is the result that all clients
  // are expected to converge to.
  kDeletionDirDoesNotExist = 5,
  kMaxValue = kDeletionDirDoesNotExist,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:FingerprintingProtectionComponentInstallationResult)

inline constexpr base::FilePath::StringViewType kComponentBaseInstallDir =
    FILE_PATH_LITERAL("Fingerprinting Protection Filter");

// Unregisters the component and deletes all component files from user data.
//
// TODO(crbug.com/456488732): Delete this function in M156.
void UnregisterAntiFingerprintingBlockedDomainListComponent(
    ComponentUpdateService* cus,
    const base::FilePath& user_data_dir,
    base::OnceClosure on_complete = base::DoNothing());

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_AFP_BLOCKED_DOMAIN_LIST_COMPONENT_REMOVER_H_

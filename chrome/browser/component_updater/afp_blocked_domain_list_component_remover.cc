// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/afp_blocked_domain_list_component_remover.h"

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/thread_pool.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"

namespace component_updater {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The CRX ID is: kgdbnmlfakkebekbaceapiaenjgmlhan.
const uint8_t kAfpBlockedDomainListPublicKeySHA256[32] = {
    0xa6, 0x31, 0xdc, 0xb5, 0x0a, 0xa4, 0x14, 0xa1, 0x02, 0x40, 0xf8,
    0x04, 0xd9, 0x6c, 0xb7, 0x0d, 0x7b, 0xbd, 0x63, 0xf9, 0xc8, 0x65,
    0x6e, 0x9b, 0x83, 0x7a, 0x3a, 0xfd, 0xd1, 0xc8, 0x40, 0xe3};

void UnregisterAntiFingerprintingBlockedDomainListComponent(
    ComponentUpdateService* cus,
    const base::FilePath& user_data_dir,
    base::OnceClosure on_complete) {
  VLOG(1) << "Unregistering Anti-Fingerprinting Blocked Domain List Component.";
  cus->UnregisterComponent(crx_file::id_util::GenerateIdFromHash(
      kAfpBlockedDomainListPublicKeySHA256));

  // Delete the entire component directory, which includes indexed rulesets that
  // were previously handled by the ruleset service as well as unindexed
  // rulesets installed by the component installer.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(
          [](const base::FilePath& user_data_dir) {
            base::FilePath base_install_dir =
                user_data_dir.Append(kComponentBaseInstallDir);
            InstallationResult result;
            if (!base::PathExists(base_install_dir)) {
              result = InstallationResult::kDeletionDirDoesNotExist;
            } else {
              result = base::DeletePathRecursively(base_install_dir)
                           ? InstallationResult::kDeletionSuccess
                           : InstallationResult::kDeletionFailure;
            }

            UMA_HISTOGRAM_ENUMERATION(
                "FingerprintingProtection.BlockedDomainListComponent."
                "InstallationResult",
                result);
          },
          user_data_dir),
      std::move(on_complete));
}

}  // namespace component_updater

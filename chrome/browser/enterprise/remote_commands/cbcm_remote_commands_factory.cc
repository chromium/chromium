// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/cbcm_remote_commands_factory.h"

#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/remote_commands/clear_browsing_data_job.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/enterprise/remote_commands/rotate_attestation_credential_job.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

namespace enterprise_commands {

std::unique_ptr<policy::RemoteCommandJob>
CBCMRemoteCommandsFactory::BuildJobForType(
    enterprise_management::RemoteCommand_Type type,
    policy::RemoteCommandsService* service) {
  if (type ==
      enterprise_management::RemoteCommand_Type_BROWSER_CLEAR_BROWSING_DATA) {
    return std::make_unique<ClearBrowsingDataJob>(
        g_browser_process->profile_manager());
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (type == enterprise_management::
                  RemoteCommand_Type_BROWSER_ROTATE_ATTESTATION_CREDENTIAL) {
    return std::make_unique<RotateAttestationCredentialJob>(
        g_browser_process->browser_policy_connector()
            ->chrome_browser_cloud_management_controller()
            ->GetDeviceTrustKeyManager());
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

  NOTREACHED_IN_MIGRATION()
      << "Received an unsupported remote command type: " << type;
  return nullptr;
}

}  // namespace enterprise_commands

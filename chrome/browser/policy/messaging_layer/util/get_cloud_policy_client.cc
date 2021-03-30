// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/get_cloud_policy_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#else
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#endif

namespace reporting {
namespace {

// policy::CloudPolicyClient is retrieved in two different ways for ChromeOS and
// non-ChromeOS browsers. This function should be called on the UI thread, and
// if it isn't will recall itself to do so.
// TODO(chromium:1078512) Wrap CloudPolicyClient in a new object so that its
// methods and retrieval are accessed on the correct thread.
void GetCloudPolicyClient(
    base::OnceCallback<void(StatusOr<policy::CloudPolicyClient*>)>
        get_client_cb) {
  if (!content::GetUIThreadTaskRunner({})->RunsTasksInCurrentSequence()) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&GetCloudPolicyClient, std::move(get_client_cb)));
    return;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy::CloudPolicyManager* const cloud_policy_manager =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetDeviceCloudPolicyManager();
#elif defined(OS_ANDROID)
  // Android doesn't have access to a device level CloudPolicyClient, so get the
  // PrimaryUserProfile CloudPolicyClient.
  policy::CloudPolicyManager* const cloud_policy_manager =
      ProfileManager::GetPrimaryUserProfile()->GetUserCloudPolicyManager();
#else
  policy::CloudPolicyManager* const cloud_policy_manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();
#endif
  if (cloud_policy_manager == nullptr) {
    std::move(get_client_cb)
        .Run(Status(error::FAILED_PRECONDITION,
                    "This is not a managed device or browser."));
    return;
  }
  auto* cloud_policy_client = cloud_policy_manager->core()->client();
  if (cloud_policy_client == nullptr) {
    std::move(get_client_cb)
        .Run(Status(error::FAILED_PRECONDITION,
                    "CloudPolicyClient is not available"));
    return;
  }
  std::move(get_client_cb).Run(cloud_policy_client);
}
}  // namespace

base::OnceCallback<void(CloudPolicyClientResultCb)> GetCloudPolicyClientCb() {
  return base::BindOnce(&GetCloudPolicyClient);
}

}  // namespace reporting

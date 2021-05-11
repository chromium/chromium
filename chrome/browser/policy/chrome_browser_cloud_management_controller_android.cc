// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_browser_cloud_management_controller_android.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/browser_dm_token_storage_android.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

ChromeBrowserCloudManagementControllerAndroid::
    ChromeBrowserCloudManagementControllerAndroid() = default;
ChromeBrowserCloudManagementControllerAndroid::
    ~ChromeBrowserCloudManagementControllerAndroid() = default;

void ChromeBrowserCloudManagementControllerAndroid::
    SetDMTokenStorageDelegate() {
  BrowserDMTokenStorage::SetDelegate(
      std::make_unique<BrowserDMTokenStorageAndroid>());
}

int ChromeBrowserCloudManagementControllerAndroid::GetUserDataDirKey() {
  return chrome::DIR_USER_DATA;
}

base::FilePath
ChromeBrowserCloudManagementControllerAndroid::GetExternalPolicyDir() {
  // External policies are not supported on Android.
  return base::FilePath();
}

ChromeBrowserCloudManagementController::Delegate::NetworkConnectionTrackerGetter
ChromeBrowserCloudManagementControllerAndroid::
    CreateNetworkConnectionTrackerGetter() {
  return base::BindRepeating(&content::GetNetworkConnectionTracker);
}

void ChromeBrowserCloudManagementControllerAndroid::InitializeOAuthTokenFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* local_state) {
  // Policy invalidations aren't currently supported on Android.
}

void ChromeBrowserCloudManagementControllerAndroid::StartWatchingRegistration(
    ChromeBrowserCloudManagementController* controller) {
  // Enrollment isn't blocking or mandatory on Android.
}

bool ChromeBrowserCloudManagementControllerAndroid::
    IsEnterpriseStartupDialogShowing() {
  // There is no enterprise startup dialog on Android.
  return false;
}

bool ChromeBrowserCloudManagementControllerAndroid::
    WaitUntilPolicyEnrollmentFinished() {
  // Enrollment currently isn't blocking or mandatory on Android, so this method
  // isn't used. Always report success.
  return true;
}

void ChromeBrowserCloudManagementControllerAndroid::OnServiceAccountSet(
    CloudPolicyClient* client,
    const std::string& account_email) {
  // Policy invalidations aren't currently supported on Android.
}

void ChromeBrowserCloudManagementControllerAndroid::ShutDown() {
  // No additional shutdown to perform on Android.
}

MachineLevelUserCloudPolicyManager*
ChromeBrowserCloudManagementControllerAndroid::
    GetMachineLevelUserCloudPolicyManager() {
  return g_browser_process->browser_policy_connector()
      ->machine_level_user_cloud_policy_manager();
}

DeviceManagementService*
ChromeBrowserCloudManagementControllerAndroid::GetDeviceManagementService() {
  return g_browser_process->browser_policy_connector()
      ->device_management_service();
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeBrowserCloudManagementControllerAndroid::GetSharedURLLoaderFactory() {
  return g_browser_process->system_network_context_manager()
      ->GetSharedURLLoaderFactory();
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeBrowserCloudManagementControllerAndroid::GetBestEffortTaskRunner() {
  // ChromeBrowserCloudManagementControllerAndroid is bound to BrowserThread::UI
  // and so must its best-effort task runner.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT});
}

void ChromeBrowserCloudManagementControllerAndroid::SetGaiaURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  // Policy invalidations aren't currently supported on Android.
}

}  // namespace policy

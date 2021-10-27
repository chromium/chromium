// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_browser_cloud_management_controller_android.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_android.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/browser_dm_token_storage_android.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/client_data_delegate_android.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

namespace {

// Responsible for triggering initialization once it can be determined if an
// enrollment token is set by non-CBCM policy providers.
class DeferredInitializationRunner : public PolicyService::Observer {
 public:
  DeferredInitializationRunner(PolicyService* policy_service,
                               base::OnceClosure callback);
  DeferredInitializationRunner(const DeferredInitializationRunner&) = delete;
  DeferredInitializationRunner& operator=(const DeferredInitializationRunner&) =
      delete;
  ~DeferredInitializationRunner() override;

  // PolicyService::Observer implementation:
  void OnPolicyUpdated(const PolicyNamespace& ns,
                       const PolicyMap& previous,
                       const PolicyMap& current) override;
  void OnPolicyServiceInitialized(PolicyDomain domain) override;
  void OnFirstPoliciesLoaded(PolicyDomain domain) override;

 private:
  PolicyService* policy_service_ = nullptr;
  // If set, a callback to be invoked by |OnPolicyServiceInitialized|.
  base::OnceClosure callback_;
};

DeferredInitializationRunner::DeferredInitializationRunner(
    PolicyService* policy_service,
    base::OnceClosure callback)
    : policy_service_(policy_service), callback_(std::move(callback)) {
  policy_service_->AddObserver(POLICY_DOMAIN_CHROME, this);
}

DeferredInitializationRunner::~DeferredInitializationRunner() {
  if (policy_service_)
    policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
}

void DeferredInitializationRunner::OnPolicyUpdated(const PolicyNamespace& ns,
                                                   const PolicyMap& previous,
                                                   const PolicyMap& current) {}

void DeferredInitializationRunner::OnPolicyServiceInitialized(
    PolicyDomain domain) {
  if (callback_)
    std::move(callback_).Run();
  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
  policy_service_ = nullptr;
}

void DeferredInitializationRunner::OnFirstPoliciesLoaded(PolicyDomain domain) {}

}  // namespace

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

std::unique_ptr<enterprise_reporting::ReportingDelegateFactory>
ChromeBrowserCloudManagementControllerAndroid::GetReportingDelegateFactory() {
  return std::make_unique<
      enterprise_reporting::ReportingDelegateFactoryAndroid>();
}

void ChromeBrowserCloudManagementControllerAndroid::SetGaiaURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  // Policy invalidations aren't currently supported on Android.
}

bool ChromeBrowserCloudManagementControllerAndroid::
    ReadyToCreatePolicyManager() {
  // On Android, policy manager creation can happen if either:
  //  - a DM token was cached in Shared Preferences by a previous browser run;
  //  - an enrollment token is available via platform policies.
  //
  // If a DM token is available, then the policy manager can be created right
  // away.
  //
  // Otherwise, creation needs to be postponed until the PolicyService has been
  // initialized. Since this method can be called during PolicyService creation,
  // additional checks (such as if the g_browser_process variable is set) are
  // needed. When postponed, policy manager creation will happen during
  // controller initialization, when it's guaranteed that the PolicyService
  // exists and is initialized.
  //
  // TODO(http://crbug.com/1200522): Allow policy manager to be created
  // immediately if a DM token is cached in Shared Preferences.
  return g_browser_process && g_browser_process->browser_policy_connector() &&
         g_browser_process->browser_policy_connector()->HasPolicyService() &&
         ReadyToInit();
}

bool ChromeBrowserCloudManagementControllerAndroid::ReadyToInit() {
  DCHECK(g_browser_process);
  DCHECK(g_browser_process->browser_policy_connector());

  return g_browser_process->browser_policy_connector()
      ->GetPolicyService()
      ->IsInitializationComplete(POLICY_DOMAIN_CHROME);
}

std::unique_ptr<ClientDataDelegate>
ChromeBrowserCloudManagementControllerAndroid::CreateClientDataDelegate() {
  return std::make_unique<ClientDataDelegateAndroid>();
}

void ChromeBrowserCloudManagementControllerAndroid::DeferInitialization(
    base::OnceClosure callback) {
  DCHECK(g_browser_process);
  DCHECK(g_browser_process->browser_policy_connector());

  policy_service_observer_ = std::make_unique<DeferredInitializationRunner>(
      g_browser_process->browser_policy_connector()->GetPolicyService(),
      std::move(callback));
}

}  // namespace policy

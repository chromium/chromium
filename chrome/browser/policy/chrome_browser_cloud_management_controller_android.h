// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_POLICY_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_ANDROID_H_

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "components/enterprise/client_certificates/core/certificate_store.h"
#include "components/enterprise/client_certificates/core/prefs_certificate_store.h"

namespace policy {

// Android implementation of the platform-specific operations of CBCMController.
class ChromeBrowserCloudManagementControllerAndroid
    : public ChromeBrowserCloudManagementController::Delegate {
 public:
  ChromeBrowserCloudManagementControllerAndroid();
  ChromeBrowserCloudManagementControllerAndroid(
      const ChromeBrowserCloudManagementControllerAndroid&) = delete;
  ChromeBrowserCloudManagementControllerAndroid& operator=(
      const ChromeBrowserCloudManagementControllerAndroid&) = delete;

  ~ChromeBrowserCloudManagementControllerAndroid() override;

  // ChromeBrowserCloudManagementController::Delegate implementation.
  void SetDMTokenStorageDelegate() override;
  int GetUserDataDirKey() override;
  base::FilePath GetExternalPolicyDir() override;
  NetworkConnectionTrackerGetter CreateNetworkConnectionTrackerGetter()
      override;
  void InitializeOAuthTokenFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* local_state) override;
  void StartWatchingRegistration(
      ChromeBrowserCloudManagementController* controller) override;
  bool WaitUntilPolicyEnrollmentFinished() override;
  bool IsEnterpriseStartupDialogShowing() override;
  void OnServiceAccountSet(CloudPolicyClient* client,
                           const std::string& account_email) override;
  void ShutDown() override;
  MachineLevelUserCloudPolicyManager* GetMachineLevelUserCloudPolicyManager()
      override;
  DeviceManagementService* GetDeviceManagementService() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory()
      override;
  scoped_refptr<base::SingleThreadTaskRunner> GetBestEffortTaskRunner()
      override;
  std::unique_ptr<enterprise_reporting::ReportingDelegateFactory>
  GetReportingDelegateFactory() override;
  void SetGaiaURLLoaderFactory(scoped_refptr<network::SharedURLLoaderFactory>
                                   url_loader_factory) override;
  bool ReadyToCreatePolicyManager() override;
  bool ReadyToInit() override;
  std::unique_ptr<ClientDataDelegate> CreateClientDataDelegate() override;
  void DeferInitialization(base::OnceClosure callback) override;
  std::unique_ptr<client_certificates::CertificateProvisioningService>
  CreateCertificateProvisioningService() override;

 private:
  // Active while it can't be determined if enrollment token is set by non-CBCM
  // policies.
  std::unique_ptr<PolicyService::ProviderUpdateObserver>
      provider_update_observer_;
  // Responsible for storing and retrieving browser-level managed identities.
  std::unique_ptr<client_certificates::CertificateStore> certificate_store_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_ANDROID_H_

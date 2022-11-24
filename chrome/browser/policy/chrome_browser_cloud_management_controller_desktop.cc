// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_browser_cloud_management_controller_desktop.h"

#include <utility>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_identity/device_identity_provider.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/enterprise/remote_commands/cbcm_remote_commands_factory.h"
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/chrome_browser_cloud_management_register_watcher.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/client_data_delegate_desktop.h"
#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"
#include "chrome/browser/policy/cloud/remote_commands_invalidator_impl.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/impl/fcm_invalidation_service.h"
#include "components/invalidation/impl/fcm_network_handler.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "base/base_paths_win.h"
#include "chrome/install_static/install_modes.h"
#else
#include "chrome/common/chrome_switches.h"
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/app_controller_mac.h"
#include "chrome/browser/policy/browser_dm_token_storage_mac.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/policy/browser_dm_token_storage_linux.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/policy/browser_dm_token_storage_win.h"
#include "chrome/install_static/install_util.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/device_trust_key_manager_impl.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_FUCHSIA)
#include "chrome/browser/policy/browser_dm_token_storage_fuchsia.h"
#endif  // BUILDFLAG(IS_FUCHSIA)

namespace policy {

namespace {

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr base::FilePath::StringPieceType kCachedPolicyDirname =
    FILE_PATH_LITERAL("Policies");
#endif

}  // namespace

ChromeBrowserCloudManagementControllerDesktop::
    ChromeBrowserCloudManagementControllerDesktop()
    : invalidations_initializer_(this) {}
ChromeBrowserCloudManagementControllerDesktop::
    ~ChromeBrowserCloudManagementControllerDesktop() = default;

void ChromeBrowserCloudManagementControllerDesktop::
    SetDMTokenStorageDelegate() {
  std::unique_ptr<BrowserDMTokenStorage::Delegate> storage_delegate;

#if BUILDFLAG(IS_MAC)
  storage_delegate = std::make_unique<BrowserDMTokenStorageMac>();
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  storage_delegate = std::make_unique<BrowserDMTokenStorageLinux>();
#elif BUILDFLAG(IS_WIN)
  storage_delegate = std::make_unique<BrowserDMTokenStorageWin>();
#elif BUILDFLAG(IS_FUCHSIA)
  storage_delegate = std::make_unique<BrowserDMTokenStorageFuchsia>();
#else
  NOTREACHED();
#endif

  BrowserDMTokenStorage::SetDelegate(std::move(storage_delegate));
}

int ChromeBrowserCloudManagementControllerDesktop::GetUserDataDirKey() {
  return chrome::DIR_USER_DATA;
}

base::FilePath
ChromeBrowserCloudManagementControllerDesktop::GetExternalPolicyDir() {
  base::FilePath external_policy_path;
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  base::PathService::Get(base::DIR_PROGRAM_FILESX86, &external_policy_path);

  external_policy_path =
      external_policy_path.Append(install_static::kCompanyPathName)
          .Append(kCachedPolicyDirname);
#endif

  return external_policy_path;
}

ChromeBrowserCloudManagementController::Delegate::NetworkConnectionTrackerGetter
ChromeBrowserCloudManagementControllerDesktop::
    CreateNetworkConnectionTrackerGetter() {
  return base::BindRepeating(&content::GetNetworkConnectionTracker);
}

void ChromeBrowserCloudManagementControllerDesktop::InitializeOAuthTokenFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* local_state) {
  DeviceOAuth2TokenServiceFactory::Initialize(url_loader_factory, local_state);
}

void ChromeBrowserCloudManagementControllerDesktop::StartWatchingRegistration(
    ChromeBrowserCloudManagementController* controller) {
  cloud_management_register_watcher_ =
      std::make_unique<ChromeBrowserCloudManagementRegisterWatcher>(controller);
}

bool ChromeBrowserCloudManagementControllerDesktop::
    WaitUntilPolicyEnrollmentFinished() {
  if (cloud_management_register_watcher_) {
    switch (cloud_management_register_watcher_
                ->WaitUntilCloudPolicyEnrollmentFinished()) {
      case ChromeBrowserCloudManagementController::RegisterResult::
          kNoEnrollmentNeeded:
      case ChromeBrowserCloudManagementController::RegisterResult::
          kEnrollmentSuccessBeforeDialogDisplayed:
      case ChromeBrowserCloudManagementController::RegisterResult::
          kEnrollmentFailedSilentlyBeforeDialogDisplayed:
        return true;
      case ChromeBrowserCloudManagementController::RegisterResult::
          kEnrollmentSuccess:
      case ChromeBrowserCloudManagementController::RegisterResult::
          kEnrollmentFailedSilently:
#if BUILDFLAG(IS_MAC)
        app_controller_mac::EnterpriseStartupDialogClosed();
#endif
        return true;
      case ChromeBrowserCloudManagementController::RegisterResult::
          kRestartDueToFailure:
        chrome::AttemptRestart();
        return false;
      case ChromeBrowserCloudManagementController::RegisterResult::
          kQuitDueToFailure:
        chrome::AttemptExit();
        return false;
    }
  }
  return true;
}

bool ChromeBrowserCloudManagementControllerDesktop::
    IsEnterpriseStartupDialogShowing() {
  return cloud_management_register_watcher_ &&
         cloud_management_register_watcher_->IsDialogShowing();
}

void ChromeBrowserCloudManagementControllerDesktop::OnServiceAccountSet(
    CloudPolicyClient* client,
    const std::string& account_email) {
  invalidations_initializer_.OnServiceAccountSet(client, account_email);
}

void ChromeBrowserCloudManagementControllerDesktop::ShutDown() {
  if (policy_invalidator_)
    policy_invalidator_->Shutdown();
  if (commands_invalidator_)
    commands_invalidator_->Shutdown();
}

MachineLevelUserCloudPolicyManager*
ChromeBrowserCloudManagementControllerDesktop::
    GetMachineLevelUserCloudPolicyManager() {
  return g_browser_process->browser_policy_connector()
      ->machine_level_user_cloud_policy_manager();
}

DeviceManagementService*
ChromeBrowserCloudManagementControllerDesktop::GetDeviceManagementService() {
  return g_browser_process->browser_policy_connector()
      ->device_management_service();
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeBrowserCloudManagementControllerDesktop::GetSharedURLLoaderFactory() {
  return g_browser_process->system_network_context_manager()
      ->GetSharedURLLoaderFactory();
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeBrowserCloudManagementControllerDesktop::GetBestEffortTaskRunner() {
  // ChromeBrowserCloudManagementControllerDesktop is bound to BrowserThread::UI
  // and so must its best-effort task runner.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT});
}

std::unique_ptr<enterprise_reporting::ReportingDelegateFactory>
ChromeBrowserCloudManagementControllerDesktop::GetReportingDelegateFactory() {
  return std::make_unique<
      enterprise_reporting::ReportingDelegateFactoryDesktop>();
}

void ChromeBrowserCloudManagementControllerDesktop::SetGaiaURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  gaia_url_loader_factory_ = url_loader_factory;
}

bool ChromeBrowserCloudManagementControllerDesktop::
    ReadyToCreatePolicyManager() {
  return true;
}

bool ChromeBrowserCloudManagementControllerDesktop::ReadyToInit() {
  return true;
}

std::unique_ptr<ClientDataDelegate>
ChromeBrowserCloudManagementControllerDesktop::CreateClientDataDelegate() {
  return std::make_unique<ClientDataDelegateDesktop>();
}

std::unique_ptr<enterprise_connectors::DeviceTrustKeyManager>
ChromeBrowserCloudManagementControllerDesktop::CreateDeviceTrustKeyManager() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  if (enterprise_connectors::IsDeviceTrustConnectorFeatureEnabled()) {
    auto key_rotation_launcher =
        enterprise_connectors::KeyRotationLauncher::Create(
            BrowserDMTokenStorage::Get(), GetDeviceManagementService(),
            GetSharedURLLoaderFactory(), g_browser_process->local_state());
    return std::make_unique<enterprise_connectors::DeviceTrustKeyManagerImpl>(
        std::move(key_rotation_launcher));
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  return nullptr;
}

void ChromeBrowserCloudManagementControllerDesktop::StartInvalidations() {
  if (invalidation_service_) {
    NOTREACHED() << "Trying to start an invalidation service when there's "
                    "already one. Please see crbug.com/1186159.";
    return;
  }

  identity_provider_ = std::make_unique<DeviceIdentityProvider>(
      DeviceOAuth2TokenServiceFactory::Get());
  device_instance_id_driver_ = std::make_unique<instance_id::InstanceIDDriver>(
      g_browser_process->gcm_driver());

  invalidation_service_ =
      std::make_unique<invalidation::FCMInvalidationService>(
          identity_provider_.get(),
          base::BindRepeating(&invalidation::FCMNetworkHandler::Create,
                              g_browser_process->gcm_driver(),
                              device_instance_id_driver_.get()),
          base::BindRepeating(
              &invalidation::PerUserTopicSubscriptionManager::Create,
              identity_provider_.get(), g_browser_process->local_state(),
              base::RetainedRef(
                  g_browser_process->shared_url_loader_factory())),
          device_instance_id_driver_.get(), g_browser_process->local_state(),
          policy::kPolicyFCMInvalidationSenderID);
  invalidation_service_->Init();

  policy_invalidator_ = std::make_unique<CloudPolicyInvalidator>(
      PolicyInvalidationScope::kCBCM,
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager()
          ->core(),
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::DefaultClock::GetInstance(),
      0 /* highest_handled_invalidation_version */);
  policy_invalidator_->Initialize(invalidation_service_.get());

  g_browser_process->browser_policy_connector()
      ->machine_level_user_cloud_policy_manager()
      ->core()
      ->StartRemoteCommandsService(
          std::make_unique<enterprise_commands::CBCMRemoteCommandsFactory>(),
          PolicyInvalidationScope::kCBCM);

  commands_invalidator_ = std::make_unique<RemoteCommandsInvalidatorImpl>(
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager()
          ->core(),
      base::DefaultClock::GetInstance(), PolicyInvalidationScope::kCBCM);
  commands_invalidator_->Initialize(invalidation_service_.get());
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeBrowserCloudManagementControllerDesktop::GetURLLoaderFactory() {
  return gaia_url_loader_factory_;
}

bool ChromeBrowserCloudManagementControllerDesktop::
    IsInvalidationsServiceStarted() const {
  // This object is created when StartInvalidations is called, and stays alive
  // thereafter.
  return !!invalidation_service_;
}

}  // namespace policy

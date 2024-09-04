// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_paths.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/notifications/adb_sideloading_policy_change_notification.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chrome/browser/ash/policy/enrollment/device_cloud_policy_initializer.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/policy/external_data/device_policy_cloud_external_data_manager.h"
#include "chrome/browser/ash/policy/external_data/handlers/crostini_ansible_playbook_external_data_handler.h"
#include "chrome/browser/ash/policy/external_data/handlers/device_print_servers_external_data_handler.h"
#include "chrome/browser/ash/policy/external_data/handlers/device_printers_external_data_handler.h"
#include "chrome/browser/ash/policy/external_data/handlers/device_wallpaper_image_external_data_handler.h"
#include "chrome/browser/ash/policy/external_data/handlers/preconfigured_desk_templates_external_data_handler.h"
#include "chrome/browser/ash/policy/external_data/handlers/print_servers_external_data_handler.h"
#include "chrome/browser/ash/policy/external_data/handlers/printers_external_data_handler.h"
#include "chrome/browser/ash/policy/external_data/handlers/user_avatar_image_external_data_handler.h"
#include "chrome/browser/ash/policy/external_data/handlers/wallpaper_image_external_data_handler.h"
#include "chrome/browser/ash/policy/handlers/adb_sideloading_allowance_mode_policy_handler.h"
#include "chrome/browser/ash/policy/handlers/bluetooth_policy_handler.h"
#include "chrome/browser/ash/policy/handlers/device_dlc_predownload_list_policy_handler.h"
#include "chrome/browser/ash/policy/handlers/device_dock_mac_address_source_handler.h"
#include "chrome/browser/ash/policy/handlers/device_name_policy_handler_impl.h"
#include "chrome/browser/ash/policy/handlers/device_wifi_allowed_handler.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler_delegate_impl.h"
#include "chrome/browser/ash/policy/handlers/system_proxy_handler.h"
#include "chrome/browser/ash/policy/handlers/tpm_auto_update_mode_policy_handler.h"
#include "chrome/browser/ash/policy/invalidation/affiliated_cloud_policy_invalidator.h"
#include "chrome/browser/ash/policy/invalidation/affiliated_invalidation_service_provider.h"
#include "chrome/browser/ash/policy/invalidation/affiliated_invalidation_service_provider_impl.h"
#include "chrome/browser/ash/policy/remote_commands/affiliated_remote_commands_invalidator.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_admin_session_controller.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/device_scheduled_reboot_handler.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/device_scheduled_update_checker.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/reboot_notifications_scheduler.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_executor_impl.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chrome/browser/ash/printing/enterprise/bulk_printers_calculator_factory.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"
#include "chrome/browser/policy/cloud/fm_registration_token_uploader.h"
#include "chrome/browser/policy/device_management_service_configuration.h"
#include "chrome/browser/policy/networking/device_network_configuration_updater_ash.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/onc/onc_certificate_importer_impl.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/invalidation_factory.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/resource_cache.h"
#include "components/policy/core/common/proxy_policy_provider.h"
#include "components/policy/core/common/remote_commands/remote_commands_invalidator_impl.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

namespace {

namespace em = ::enterprise_management;

// Used by `InvalidationListener` in logs to distinguish instances.
constexpr char kInvalidationListenerLogPrefix[] = "BrowserPolicyConnectorAsh";

MarketSegment TranslateMarketSegment(
    em::PolicyData::MarketSegment market_segment) {
  switch (market_segment) {
    case em::PolicyData::MARKET_SEGMENT_UNSPECIFIED:
      return MarketSegment::UNKNOWN;
    case em::PolicyData::ENROLLED_EDUCATION:
      return MarketSegment::EDUCATION;
    case em::PolicyData::ENROLLED_ENTERPRISE:
      return MarketSegment::ENTERPRISE;
  }
  NOTREACHED_IN_MIGRATION();
  return MarketSegment::UNKNOWN;
}

std::variant<std::unique_ptr<AffiliatedInvalidationServiceProvider>,
             std::unique_ptr<invalidation::InvalidationListener>>
CreateServiceProviderOrListener(
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver) {
  if (base::FeatureList::IsEnabled(
          invalidation::kInvalidationsWithDirectMessages)) {
    auto listener = invalidation::CreateInvalidationServiceOrListener(
        /*identity_provider=*/nullptr, gcm_driver, instance_id_driver,
        /*url_loader_factory=*/{}, /*pref_service=*/nullptr, /*sender_id=*/"",
        invalidation::InvalidationListener::kProjectNumberEnterprise,
        kInvalidationListenerLogPrefix);
    CHECK(std::holds_alternative<
          std::unique_ptr<invalidation::InvalidationListener>>(listener))
        << "InvalidationListener is not created in InvalidationListener setup";

    return std::move(
        std::get<std::unique_ptr<invalidation::InvalidationListener>>(
            listener));
  }

  return std::make_unique<AffiliatedInvalidationServiceProviderImpl>();
}

}  // namespace

// static
scoped_refptr<base::SequencedTaskRunner>
BrowserPolicyConnectorAsh::CreateBackgroundTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

BrowserPolicyConnectorAsh::BrowserPolicyConnectorAsh() {
  DCHECK(ash::InstallAttributes::IsInitialized());

  crd_admin_session_controller_ = std::make_unique<CrdAdminSessionController>();

  // DBusThreadManager or DeviceSettingsService may be
  // uninitialized on unit tests.
  if (ash::DBusThreadManager::IsInitialized() &&
      ash::DeviceSettingsService::IsInitialized()) {
    std::unique_ptr<DeviceCloudPolicyStoreAsh> device_cloud_policy_store =
        std::make_unique<DeviceCloudPolicyStoreAsh>(
            ash::DeviceSettingsService::Get(), ash::InstallAttributes::Get(),
            CreateBackgroundTaskRunner());

    state_keys_broker_ = std::make_unique<ServerBackedStateKeysBroker>(
        ash::SessionManagerClient::Get());

    const base::FilePath device_policy_external_data_path =
        base::PathService::CheckedGet(ash::DIR_DEVICE_POLICY_EXTERNAL_DATA);

    auto external_data_manager =
        std::make_unique<DevicePolicyCloudExternalDataManager>(
            base::BindRepeating(&GetChromePolicyDetails),
            CreateBackgroundTaskRunner(), device_policy_external_data_path,
            device_cloud_policy_store.get());

    device_cloud_policy_manager_ = new DeviceCloudPolicyManagerAsh(
        std::move(device_cloud_policy_store), std::move(external_data_manager),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        state_keys_broker_.get(), crd_admin_session_controller_->GetDelegate());
    providers_for_init_.push_back(base::WrapUnique<ConfigurationPolicyProvider>(
        device_cloud_policy_manager_.get()));
  }

  global_user_cloud_policy_provider_ = new ProxyPolicyProvider();
  providers_for_init_.push_back(std::unique_ptr<ConfigurationPolicyProvider>(
      global_user_cloud_policy_provider_));
}

BrowserPolicyConnectorAsh::~BrowserPolicyConnectorAsh() = default;

void BrowserPolicyConnectorAsh::Init(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  local_state_ = local_state;
  ChromeBrowserPolicyConnector::Init(local_state, url_loader_factory);

  instance_id_driver_ = std::make_unique<instance_id::InstanceIDDriver>(
      g_browser_process->gcm_driver());

  invalidation_service_provider_or_listener_ = CreateServiceProviderOrListener(
      g_browser_process->gcm_driver(), instance_id_driver_.get());

  if (device_cloud_policy_manager_) {
    // Note: for now the |device_cloud_policy_manager_| is using the global
    // schema registry. Eventually it will have its own registry, once device
    // cloud policy for extensions is introduced. That means it'd have to be
    // initialized from here instead of BrowserPolicyConnector::Init().

    device_cloud_policy_manager_->Initialize(local_state);
    EnrollmentRequisitionManager::Initialize();
    device_cloud_policy_manager_->AddDeviceCloudPolicyManagerObserver(this);
    RestartDeviceCloudPolicyInitializer();
  }

  device_local_account_policy_service_ =
      std::make_unique<DeviceLocalAccountPolicyService>(
          ash::SessionManagerClient::Get(), ash::DeviceSettingsService::Get(),
          ash::CrosSettings::Get(),
          invalidation::UniquePointerVariantToPointer(
              invalidation_service_provider_or_listener_),
          CreateBackgroundTaskRunner(), CreateBackgroundTaskRunner(),
          CreateBackgroundTaskRunner(), url_loader_factory);
  device_local_account_policy_service_->Connect(device_management_service());

  if (device_cloud_policy_manager_) {
    std::visit(base::Overloaded{
                   [this](AffiliatedInvalidationServiceProvider* provider) {
                     device_cloud_policy_invalidator_ =
                         std::make_unique<AffiliatedCloudPolicyInvalidator>(
                             PolicyInvalidationScope::kDevice,
                             device_cloud_policy_manager_->core(), provider);
                     device_remote_commands_invalidator_ =
                         std::make_unique<AffiliatedRemoteCommandsInvalidator>(
                             device_cloud_policy_manager_->core(), provider,
                             PolicyInvalidationScope::kDevice);
                   },
                   [this](invalidation::InvalidationListener* listener) {
                     auto policy_invalidator =
                         std::make_unique<CloudPolicyInvalidator>(
                             PolicyInvalidationScope::kDevice,
                             device_cloud_policy_manager_->core(),
                             base::SingleThreadTaskRunner::GetCurrentDefault(),
                             base::DefaultClock::GetInstance(),
                             /*highest_handled_invalidation_version=*/0,
                             /*device_local_account_id=*/"");
                     policy_invalidator->Initialize(listener);
                     device_cloud_policy_invalidator_ =
                         std::move(policy_invalidator);

                     auto commands_invalidator =
                         std::make_unique<RemoteCommandsInvalidatorImpl>(
                             device_cloud_policy_manager_->core(),
                             base::DefaultClock::GetInstance(),
                             PolicyInvalidationScope::kDevice);
                     commands_invalidator->Initialize(listener);
                     device_remote_commands_invalidator_ =
                         std::move(commands_invalidator);

                     device_fm_registration_token_uploader_ =
                         std::make_unique<FmRegistrationTokenUploader>(
                             PolicyInvalidationScope::kDevice, listener,
                             device_cloud_policy_manager_->core());
                   }},
               invalidation::UniquePointerVariantToPointer(
                   invalidation_service_provider_or_listener_));
  }

  SetTimezoneIfPolicyAvailable();

  device_network_configuration_updater_ =
      DeviceNetworkConfigurationUpdaterAsh::CreateForDevicePolicy(
          GetPolicyService(),
          ash::NetworkHandler::Get()->managed_network_configuration_handler(),
          ash::NetworkHandler::Get()->network_device_handler(),
          ash::CrosSettings::Get(),
          DeviceNetworkConfigurationUpdaterAsh::DeviceAssetIDFetcher());
  // NetworkCertLoader may be not initialized in tests.
  if (ash::NetworkCertLoader::IsInitialized()) {
    ash::NetworkCertLoader::Get()->SetDevicePolicyCertificateProvider(
        device_network_configuration_updater_.get());
  }

  bluetooth_policy_handler_ =
      std::make_unique<BluetoothPolicyHandler>(ash::CrosSettings::Get());

  device_name_policy_handler_ =
      std::make_unique<DeviceNamePolicyHandlerImpl>(ash::CrosSettings::Get());

  minimum_version_policy_handler_delegate_ =
      std::make_unique<MinimumVersionPolicyHandlerDelegateImpl>();

  minimum_version_policy_handler_ =
      std::make_unique<MinimumVersionPolicyHandler>(
          minimum_version_policy_handler_delegate_.get(),
          ash::CrosSettings::Get());

  device_dock_mac_address_source_handler_ =
      std::make_unique<DeviceDockMacAddressHandler>(
          ash::CrosSettings::Get(),
          ash::NetworkHandler::Get()->network_device_handler());

  device_wifi_allowed_handler_ =
      std::make_unique<DeviceWiFiAllowedHandler>(ash::CrosSettings::Get());

  tpm_auto_update_mode_policy_handler_ =
      std::make_unique<TPMAutoUpdateModePolicyHandler>(ash::CrosSettings::Get(),
                                                       local_state);

  device_scheduled_update_checker_ =
      std::make_unique<DeviceScheduledUpdateChecker>(
          ash::CrosSettings::Get(),
          ash::NetworkHandler::Get()->network_state_handler(),
          std::make_unique<ScheduledTaskExecutorImpl>(
              update_checker_internal::kUpdateCheckTimerTag));

  ash::BulkPrintersCalculatorFactory* calculator_factory =
      ash::BulkPrintersCalculatorFactory::Get();
  DCHECK(calculator_factory)
      << "Policy connector initialized before the bulk printers factory";
  device_cloud_external_data_policy_handlers_.push_back(
      std::make_unique<DevicePrintersExternalDataHandler>(
          GetPolicyService(), calculator_factory->GetForDevice()));
  device_cloud_external_data_policy_handlers_.push_back(
      std::make_unique<DevicePrintServersExternalDataHandler>(
          GetPolicyService()));

  device_cloud_external_data_policy_handlers_.push_back(
      std::make_unique<DeviceWallpaperImageExternalDataHandler>(
          local_state, GetPolicyService()));
  system_proxy_handler_ =
      std::make_unique<SystemProxyHandler>(ash::CrosSettings::Get());

  adb_sideloading_allowance_mode_policy_handler_ =
      std::make_unique<AdbSideloadingAllowanceModePolicyHandler>(
          ash::CrosSettings::Get(), local_state,
          chromeos::PowerManagerClient::Get(),
          new ash::AdbSideloadingPolicyChangeNotification());

  reboot_notifications_scheduler_ =
      std::make_unique<RebootNotificationsScheduler>();

  device_scheduled_reboot_handler_ =
      std::make_unique<DeviceScheduledRebootHandler>(
          ash::CrosSettings::Get(),
          std::make_unique<ScheduledTaskExecutorImpl>(
              DeviceScheduledRebootHandler::kRebootTimerTag),
          reboot_notifications_scheduler_.get());

  device_dlc_predownload_list_policy_handler_ =
      DeviceDlcPredownloadListPolicyHandler::Create();
}

void BrowserPolicyConnectorAsh::OnBrowserStarted() {
  ChromeBrowserPolicyConnector::OnBrowserStarted();

  // `ash::Shell` is not available when `BrowserPolicyConnectorAsh::Init` is
  // invoked, so we must delay this initialization until now.
  crd_admin_session_controller_->Init(
      local_state_,
      CHECK_DEREF(ash::Shell::Get()).security_curtain_controller());
}

void BrowserPolicyConnectorAsh::PreShutdown() {
  // Let the |affiliated_invalidation_service_provider_| unregister itself as an
  // observer of per-Profile InvalidationServices and the device-global
  // invalidation::InvalidationService it may have created as an observer of
  // the DeviceOAuth2TokenService that is destroyed before Shutdown() is called.
  //
  // TODO(b/308427142) The comment above is hard to grok, as is the code it
  // describes. We should clean this up.
  if (auto* provider =
          std::get_if<std::unique_ptr<AffiliatedInvalidationServiceProvider>>(
              &invalidation_service_provider_or_listener_);
      provider && *provider) {
    (*provider)->Shutdown();
  }

  // This controller depends on the `SecurityCurtainController` which will be
  // destroyed before `BrowserPolicyConnectorAsh::Shutdown` is invoked.
  crd_admin_session_controller_->Shutdown();
}

void BrowserPolicyConnectorAsh::Shutdown() {
  device_cert_provisioning_scheduler_.reset();
  system_proxy_handler_.reset();

  // NetworkCertLoader may be not initialized in tests.
  if (ash::NetworkCertLoader::IsInitialized()) {
    ash::NetworkCertLoader::Get()->SetDevicePolicyCertificateProvider(nullptr);
  }
  device_network_configuration_updater_.reset();

  if (device_local_account_policy_service_) {
    device_local_account_policy_service_->Shutdown();
  }

  if (device_cloud_policy_initializer_) {
    device_cloud_policy_initializer_->Shutdown();
  }

  if (device_cloud_policy_manager_) {
    device_cloud_policy_manager_->RemoveDeviceCloudPolicyManagerObserver(this);
  }

  if (auto* invalidator = std::get_if<std::unique_ptr<CloudPolicyInvalidator>>(
          &device_cloud_policy_invalidator_);
      invalidator && *invalidator) {
    (*invalidator)->Shutdown();
  }

  if (auto* invalidator =
          std::get_if<std::unique_ptr<RemoteCommandsInvalidator>>(
              &device_remote_commands_invalidator_);
      invalidator && *invalidator) {
    (*invalidator)->Shutdown();
  }

  device_fm_registration_token_uploader_.reset();

  // `InvalidationListener` must be destroyed after its dependants
  // (`device_cert_provisioning_scheduler_`,
  // `device_local_account_policy_service_`, `device_cloud_policy_invalidator_`,
  // `device_remote_commands_invalidator_`, and
  // `device_fm_registration_token_uploader_`) but before it's dependencies
  // (`GCMDriver`).
  if (auto* listener =
          std::get_if<std::unique_ptr<invalidation::InvalidationListener>>(
              &invalidation_service_provider_or_listener_);
      listener && *listener) {
    listener->reset();
  }

  device_scheduled_update_checker_.reset();

  device_scheduled_reboot_handler_.reset();

  device_dlc_predownload_list_policy_handler_.reset();

  reboot_notifications_scheduler_.reset();

  // The policy handler is registered as an observer to BuildState which gets
  // destructed before BrowserPolicyConnectorAsh. So destruct the policy
  // handler here so that it can de-register itself as an observer.
  minimum_version_policy_handler_.reset();

  if (device_name_policy_handler_) {
    device_name_policy_handler_.reset();
  }

  for (auto& device_cloud_external_data_policy_handler :
       device_cloud_external_data_policy_handlers_) {
    device_cloud_external_data_policy_handler->Shutdown();
  }

  adb_sideloading_allowance_mode_policy_handler_.reset();

  ChromeBrowserPolicyConnector::Shutdown();
}

bool BrowserPolicyConnectorAsh::IsDeviceEnterpriseManaged() const {
  return ash::InstallAttributes::Get()->IsEnterpriseManaged();
}

bool BrowserPolicyConnectorAsh::HasMachineLevelPolicies() {
  NOTREACHED_IN_MIGRATION() << "This method is only defined for desktop Chrome";
  return false;
}

bool BrowserPolicyConnectorAsh::IsCloudManaged() const {
  return ash::InstallAttributes::Get()->IsCloudManaged();
}

std::string BrowserPolicyConnectorAsh::GetEnterpriseEnrollmentDomain() const {
  return ash::InstallAttributes::Get()->GetDomain();
}

std::string BrowserPolicyConnectorAsh::GetEnterpriseDomainManager() const {
  const em::PolicyData* policy = GetDevicePolicy();
  if (policy && policy->has_managed_by()) {
    return policy->managed_by();
  }
  if (policy && policy->has_display_domain()) {
    return policy->display_domain();
  }
  return GetEnterpriseEnrollmentDomain();
}

std::string BrowserPolicyConnectorAsh::GetSSOProfile() const {
  const em::PolicyData* policy = GetDevicePolicy();
  if (policy && policy->has_sso_profile()) {
    return policy->sso_profile();
  }
  return std::string();
}

std::string BrowserPolicyConnectorAsh::GetDeviceAssetID() const {
  const em::PolicyData* policy = GetDevicePolicy();
  if (policy && policy->has_annotated_asset_id()) {
    return policy->annotated_asset_id();
  }
  return std::string();
}

std::string BrowserPolicyConnectorAsh::GetMachineName() const {
  const em::PolicyData* policy = GetDevicePolicy();
  if (policy && policy->has_machine_name()) {
    return policy->machine_name();
  }
  return std::string();
}

std::string BrowserPolicyConnectorAsh::GetDeviceAnnotatedLocation() const {
  const em::PolicyData* policy = GetDevicePolicy();
  if (policy && policy->has_annotated_location()) {
    return policy->annotated_location();
  }
  return std::string();
}

std::string BrowserPolicyConnectorAsh::GetDirectoryApiID() const {
  const em::PolicyData* policy = GetDevicePolicy();
  if (policy && policy->has_directory_api_id()) {
    return policy->directory_api_id();
  }
  return std::string();
}

std::string BrowserPolicyConnectorAsh::GetObfuscatedCustomerID() const {
  const em::PolicyData* policy = GetDevicePolicy();
  if (policy && policy->has_obfuscated_customer_id()) {
    return policy->obfuscated_customer_id();
  }
  return std::string();
}

bool BrowserPolicyConnectorAsh::IsKioskEnrolled() const {
  const em::PolicyData* policy = GetDevicePolicy();
  if (policy && policy->has_license_sku()) {
    return policy->license_sku() == kKioskSkuName;
  }
  return false;
}

std::string BrowserPolicyConnectorAsh::GetCustomerLogoURL() const {
  const em::PolicyData* policy = GetDevicePolicy();
  if (policy && policy->has_customer_logo()) {
    return policy->customer_logo().logo_url();
  }
  return std::string();
}

DeviceMode BrowserPolicyConnectorAsh::GetDeviceMode() const {
  return ash::InstallAttributes::Get()->GetMode();
}

ash::InstallAttributes* BrowserPolicyConnectorAsh::GetInstallAttributes()
    const {
  return ash::InstallAttributes::Get();
}

MarketSegment BrowserPolicyConnectorAsh::GetEnterpriseMarketSegment() const {
  const em::PolicyData* policy = GetDevicePolicy();
  if (policy && policy->has_market_segment()) {
    return TranslateMarketSegment(policy->market_segment());
  }
  return MarketSegment::UNKNOWN;
}

ProxyPolicyProvider*
BrowserPolicyConnectorAsh::GetGlobalUserCloudPolicyProvider() {
  return global_user_cloud_policy_provider_;
}

// static
void BrowserPolicyConnectorAsh::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kDevicePolicyRefreshRate,
      CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs);
}

void BrowserPolicyConnectorAsh::OnUserManagerCreated(
    user_manager::UserManager* user_manager) {
  auto* cros_settings = ash::CrosSettings::Get();
  cloud_external_data_policy_observers_.push_back(
      std::make_unique<policy::CloudExternalDataPolicyObserver>(
          cros_settings, device_local_account_policy_service_.get(),
          policy::key::kUserAvatarImage, user_manager,
          std::make_unique<policy::UserAvatarImageExternalDataHandler>()));
  cloud_external_data_policy_observers_.push_back(
      std::make_unique<policy::CloudExternalDataPolicyObserver>(
          cros_settings, device_local_account_policy_service_.get(),
          policy::key::kWallpaperImage, user_manager,
          std::make_unique<policy::WallpaperImageExternalDataHandler>()));
  cloud_external_data_policy_observers_.push_back(
      std::make_unique<policy::CloudExternalDataPolicyObserver>(
          cros_settings, device_local_account_policy_service_.get(),
          policy::key::kPrintersBulkConfiguration, user_manager,
          std::make_unique<policy::PrintersExternalDataHandler>()));
  cloud_external_data_policy_observers_.push_back(
      std::make_unique<policy::CloudExternalDataPolicyObserver>(
          cros_settings, device_local_account_policy_service_.get(),
          policy::key::kExternalPrintServers, user_manager,
          std::make_unique<policy::PrintServersExternalDataHandler>()));
  cloud_external_data_policy_observers_.push_back(
      std::make_unique<policy::CloudExternalDataPolicyObserver>(
          cros_settings, device_local_account_policy_service_.get(),
          policy::key::kCrostiniAnsiblePlaybook, user_manager,
          std::make_unique<
              policy::CrostiniAnsiblePlaybookExternalDataHandler>()));
  cloud_external_data_policy_observers_.push_back(
      std::make_unique<policy::CloudExternalDataPolicyObserver>(
          cros_settings, device_local_account_policy_service_.get(),
          policy::key::kPreconfiguredDeskTemplates, user_manager,
          std::make_unique<
              policy::PreconfiguredDeskTemplatesExternalDataHandler>()));
  for (auto& observer : cloud_external_data_policy_observers_) {
    observer->Init();
  }

  if (device_cloud_policy_manager_) {
    device_cloud_policy_manager_->OnUserManagerCreated(user_manager);
  }
}

void BrowserPolicyConnectorAsh::OnUserManagerShutdown() {
  cloud_external_data_policy_observers_.clear();
}

void BrowserPolicyConnectorAsh::OnUserManagerWillBeDestroyed() {
  if (device_cloud_policy_manager_) {
    device_cloud_policy_manager_->OnUserManagerWillBeDestroyed();
  }
}

void BrowserPolicyConnectorAsh::OnDeviceCloudPolicyManagerConnected() {
  CHECK(device_cloud_policy_initializer_);

  // DeviceCloudPolicyInitializer might still be on the call stack, so we
  // should delete the initializer after this function returns.
  device_cloud_policy_initializer_->Shutdown();
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(device_cloud_policy_initializer_));

  if (!device_cert_provisioning_scheduler_) {
    // CertProvisioningScheduler depends on the device-wide CloudPolicyClient to
    // be available so it can only be created when the CloudPolicyManager is
    // connected.
    // |device_cloud_policy_manager_| and its CloudPolicyClient are guaranteed
    // to be non-null when this observer function has been called.
    CloudPolicyClient* cloud_policy_client =
        device_cloud_policy_manager_->core()->client();

    device_cert_provisioning_scheduler_ = ash::cert_provisioning::
        CertProvisioningSchedulerImpl::CreateDeviceCertProvisioningScheduler(
            cloud_policy_client,
            invalidation::UniquePointerVariantToPointer(
                invalidation_service_provider_or_listener_));
  }
}

void BrowserPolicyConnectorAsh::OnDeviceCloudPolicyManagerGotRegistry() {
  // Do nothing.
}

bool BrowserPolicyConnectorAsh::IsCommandLineSwitchSupported() const {
  return true;
}

std::vector<std::unique_ptr<ConfigurationPolicyProvider>>
BrowserPolicyConnectorAsh::CreatePolicyProviders() {
  auto providers = ChromeBrowserPolicyConnector::CreatePolicyProviders();
  for (auto& provider_ptr : providers_for_init_) {
    providers.push_back(std::move(provider_ptr));
  }
  providers_for_init_.clear();
  return providers;
}

void BrowserPolicyConnectorAsh::SetTimezoneIfPolicyAvailable() {
  typedef ash::CrosSettingsProvider Provider;
  Provider::TrustedStatus result =
      ash::CrosSettings::Get()->PrepareTrustedValues(base::BindOnce(
          &BrowserPolicyConnectorAsh::SetTimezoneIfPolicyAvailable,
          weak_ptr_factory_.GetWeakPtr()));

  if (result != Provider::TRUSTED) {
    return;
  }

  std::string timezone;
  if (ash::CrosSettings::Get()->GetString(ash::kSystemTimezonePolicy,
                                          &timezone) &&
      !timezone.empty()) {
    ash::system::SetSystemAndSigninScreenTimezone(timezone);
  }
}

void BrowserPolicyConnectorAsh::RestartDeviceCloudPolicyInitializer() {
  device_cloud_policy_initializer_ =
      std::make_unique<DeviceCloudPolicyInitializer>(
          device_management_service(), ash::InstallAttributes::Get(),
          state_keys_broker_.get(),
          device_cloud_policy_manager_->device_store(),
          device_cloud_policy_manager_,
          ash::system::StatisticsProvider::GetInstance());
  device_cloud_policy_initializer_->Init();
}

base::flat_set<std::string> BrowserPolicyConnectorAsh::device_affiliation_ids()
    const {
  if (!device_affiliation_ids_for_testing_.empty()) {
    return device_affiliation_ids_for_testing_;
  }

  const em::PolicyData* policy = GetDevicePolicy();
  if (policy) {
    const auto& ids = policy->device_affiliation_ids();
    return {ids.begin(), ids.end()};
  }
  return {};
}

const em::PolicyData* BrowserPolicyConnectorAsh::GetDevicePolicy() const {
  if (device_cloud_policy_manager_) {
    return device_cloud_policy_manager_->device_store()->policy();
  }

  return nullptr;
}

}  // namespace policy

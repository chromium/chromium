// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"

#include <keyhi.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/ash/ownership/owner_key_loader.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/ownership/ownership_histograms.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/about_flags.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/device_settings_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/tpm/tpm_token_loader.h"
#include "components/ownership/owner_key_util.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "crypto/nss_key_util.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/signature_creator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace em = enterprise_management;

using content::BrowserThread;
using google::protobuf::RepeatedPtrField;
using ownership::OwnerKeyUtil;
using ownership::PrivateKey;
using ownership::PublicKey;

namespace ash {

namespace {

using ReloadKeyCallback =
    base::OnceCallback<void(scoped_refptr<PublicKey> public_key,
                            scoped_refptr<PrivateKey> private_key)>;

bool IsOwnerInTests(const std::string& user_id) {
  if (user_id.empty() ||
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kTestType) ||
      !CrosSettings::IsInitialized()) {
    return false;
  }
  const base::Value* value = CrosSettings::Get()->GetPref(kDeviceOwner);
  if (!value || !value->is_string())
    return false;
  return value->GetString() == user_id;
}

void LoadPrivateKeyByPublicKeyOnWorkerThread(
    const scoped_refptr<OwnerKeyUtil>& owner_key_util,
    crypto::ScopedPK11Slot public_slot,
    crypto::ScopedPK11Slot private_slot,
    ReloadKeyCallback callback) {
  scoped_refptr<PublicKey> public_key = owner_key_util->ImportPublicKey();
  if (!public_key) {
    scoped_refptr<PrivateKey> private_key;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), public_key, private_key));
    return;
  }

  // If private slot is already available, this will check it. If not, we'll get
  // called again later when the TPM Token is ready, and the slot will be
  // available then. FindPrivateKeyInSlot internally checks for a null slot if
  // needbe.
  //
  // TODO(davidben): The null check should be in the caller rather than
  // internally in the OwnerKeyUtil implementation. The tests currently get a
  // null private_slot and expect the mock OwnerKeyUtil to still be called.
  scoped_refptr<PrivateKey> private_key(
      new PrivateKey(owner_key_util->FindPrivateKeyInSlot(public_key->data(),
                                                          private_slot.get())));
  if (!private_key->key()) {
    private_key = new PrivateKey(owner_key_util->FindPrivateKeyInSlot(
        public_key->data(), public_slot.get()));
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), public_key, private_key));
}

void ContinueLoadPrivateKeyOnIOThread(
    const scoped_refptr<OwnerKeyUtil>& owner_key_util,
    const std::string username_hash,
    ReloadKeyCallback callback,
    crypto::ScopedPK11Slot private_slot) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(eseckler): It seems loading the key is important for the UsersPrivate
  // extension API to work correctly during startup, which is why we cannot
  // currently use the BEST_EFFORT TaskPriority here.
  scoped_refptr<base::TaskRunner> task_runner =
      base::ThreadPool::CreateTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&LoadPrivateKeyByPublicKeyOnWorkerThread, owner_key_util,
                     crypto::GetPublicSlotForChromeOSUser(username_hash),
                     std::move(private_slot), std::move(callback)));
}

void LoadPrivateKeyOnIOThread(const scoped_refptr<OwnerKeyUtil>& owner_key_util,
                              const std::string username_hash,
                              ReloadKeyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  crypto::EnsureNSSInit();

  // GetPrivateSlotForChromeOSUser() will only invoke the callback if the
  // private slot has not already been loaded. Split it here so we can invoke
  // the callback separately if the private slot has already been loaded.
  auto callback_split = base::SplitOnceCallback(
      base::BindOnce(&ContinueLoadPrivateKeyOnIOThread, owner_key_util,
                     username_hash, std::move(callback)));
  crypto::ScopedPK11Slot private_slot = crypto::GetPrivateSlotForChromeOSUser(
      username_hash, std::move(callback_split.first));
  if (private_slot) {
    std::move(callback_split.second).Run(std::move(private_slot));
  }
}

bool DoesPrivateKeyExistAsyncHelper(
    const scoped_refptr<OwnerKeyUtil>& owner_key_util) {
  scoped_refptr<PublicKey> public_key = owner_key_util->ImportPublicKey();
  if (!public_key)
    return false;
  crypto::ScopedSECKEYPrivateKey key =
      crypto::FindNSSKeyFromPublicKeyInfo(public_key->data());
  return key && SECKEY_GetPrivateKeyType(key.get()) == rsaKey;
}

// Checks whether NSS slots with private key are mounted or
// not. Responds via |callback|.
void DoesPrivateKeyExistAsync(
    const scoped_refptr<OwnerKeyUtil>& owner_key_util,
    OwnerSettingsServiceAsh::IsOwnerCallback callback) {
  if (!owner_key_util.get()) {
    std::move(callback).Run(false);
    return;
  }
  scoped_refptr<base::TaskRunner> task_runner =
      base::ThreadPool::CreateTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DoesPrivateKeyExistAsyncHelper, owner_key_util),
      std::move(callback));
}

void OnTPMTokenReadyOnIOThread(
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    base::OnceClosure ready_callback,
    bool /*is_tpm_token_enabled*/) {
  original_task_runner->PostTask(FROM_HERE, std::move(ready_callback));
}

}  // namespace

OwnerSettingsServiceAsh::ManagementSettings::ManagementSettings() = default;

OwnerSettingsServiceAsh::ManagementSettings::~ManagementSettings() = default;

OwnerSettingsServiceAsh::OwnerSettingsServiceAsh(
    DeviceSettingsService* device_settings_service,
    Profile* profile,
    const scoped_refptr<OwnerKeyUtil>& owner_key_util)
    : ownership::OwnerSettingsService(owner_key_util),
      device_settings_service_(device_settings_service),
      profile_(profile) {
  if (SessionManagerClient::Get())
    SessionManagerClient::Get()->AddObserver(this);

  if (device_settings_service_)
    device_settings_service_->AddObserver(this);

  if (!user_manager::UserManager::IsInitialized()) {
    // interactive_ui_tests does not set user manager.
    return;
  }

  // The ProfileManager may be null in unit tests.
  if (ProfileManager* profile_manager = g_browser_process->profile_manager())
    profile_manager_observation_.Observe(profile_manager);

  auto ready_callback = base::BindOnce(
      &OwnerSettingsServiceAsh::OnTPMTokenReady, weak_factory_.GetWeakPtr());
  waiting_for_tpm_token_ = true;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &crypto::IsTPMTokenEnabled,
          base::BindOnce(OnTPMTokenReadyOnIOThread,
                         base::SequencedTaskRunner::GetCurrentDefault(),
                         std::move(ready_callback))));
}

OwnerSettingsServiceAsh::~OwnerSettingsServiceAsh() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_settings_service_)
    device_settings_service_->RemoveObserver(this);

  if (SessionManagerClient::Get())
    SessionManagerClient::Get()->RemoveObserver(this);
}

OwnerSettingsServiceAsh* OwnerSettingsServiceAsh::FromWebUI(
    content::WebUI* web_ui) {
  if (!web_ui)
    return nullptr;
  Profile* profile = Profile::FromWebUI(web_ui);
  if (!profile)
    return nullptr;
  return OwnerSettingsServiceAshFactory::GetForBrowserContext(profile);
}

void OwnerSettingsServiceAsh::OnTPMTokenReady() {
  DCHECK(thread_checker_.CalledOnValidThread());
  waiting_for_tpm_token_ = false;

  // TPMTokenLoader initializes the TPM and NSS database which is necessary to
  // determine ownership. Force a reload once we know these are initialized.
  ReloadKeypair();
}

bool OwnerSettingsServiceAsh::HasPendingChanges() const {
  return !pending_changes_.empty() || tentative_settings_.get() ||
         has_pending_fixups_;
}

bool OwnerSettingsServiceAsh::IsOwner() {
  if (InstallAttributes::Get()->IsEnterpriseManaged()) {
    return false;
  }
  return OwnerSettingsService::IsOwner();
}

void OwnerSettingsServiceAsh::IsOwnerAsync(IsOwnerCallback callback) {
  if (InstallAttributes::Get()->IsEnterpriseManaged()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  OwnerSettingsService::IsOwnerAsync(std::move(callback));
}

bool OwnerSettingsServiceAsh::HandlesSetting(const std::string& setting) {
  return DeviceSettingsProvider::IsDeviceSetting(setting);
}

bool OwnerSettingsServiceAsh::Set(const std::string& setting,
                                  const base::Value& value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!IsOwner() && !IsOwnerInTests(user_id_))
    return false;

  pending_changes_[setting] = base::Value::ToUniquePtrValue(value.Clone());

  em::ChromeDeviceSettingsProto settings;
  if (tentative_settings_.get()) {
    settings = *tentative_settings_;
  } else if (device_settings_service_->status() ==
                 DeviceSettingsService::STORE_SUCCESS &&
             device_settings_service_->device_settings()) {
    settings = *device_settings_service_->device_settings();
  }
  UpdateDeviceSettings(setting, value, settings);
  em::PolicyData policy_data;
  policy_data.set_username(user_id_);
  CHECK(settings.SerializeToString(policy_data.mutable_policy_value()));
  for (auto& observer : observers_)
    observer.OnTentativeChangesInPolicy(policy_data);
  StorePendingChanges();
  return true;
}

bool OwnerSettingsServiceAsh::AppendToList(const std::string& setting,
                                           const base::Value& value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const base::Value::List* old_value;
  if (!CrosSettings::Get()->GetList(setting, &old_value)) {
    return false;
  }

  base::Value::List new_value =
      old_value ? old_value->Clone() : base::Value::List();

  new_value.Append(value.Clone());
  return Set(setting, base::Value(std::move(new_value)));
}

bool OwnerSettingsServiceAsh::RemoveFromList(const std::string& setting,
                                             const base::Value& value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const base::Value* old_value = CrosSettings::Get()->GetPref(setting);
  if (old_value && !old_value->is_list())
    return false;
  base::Value::List new_value;
  if (old_value)
    new_value = old_value->GetList().Clone();
  new_value.EraseValue(value);
  return Set(setting, base::Value(std::move(new_value)));
}

bool OwnerSettingsServiceAsh::CommitTentativeDeviceSettings(
    std::unique_ptr<enterprise_management::PolicyData> policy) {
  if (!IsOwner() && !IsOwnerInTests(user_id_))
    return false;
  if (policy->username() != user_id_) {
    LOG(ERROR) << "Username mismatch: " << policy->username() << " vs. "
               << user_id_;
    return false;
  }
  tentative_settings_ = std::make_unique<em::ChromeDeviceSettingsProto>();
  CHECK(tentative_settings_->ParseFromString(policy->policy_value()));
  StorePendingChanges();
  return true;
}

void OwnerSettingsServiceAsh::OnProfileAdded(Profile* profile) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (profile != profile_)
    return;

  profile_manager_observation_.Reset();
  ReloadKeypair();
}

void OwnerSettingsServiceAsh::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

void OwnerSettingsServiceAsh::OwnerKeySet(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (base::FeatureList::IsEnabled(ownership::kChromeSideOwnerKeyGeneration)) {
    // OwnerKeySet notification is used to reload the owner key in Chrome when
    // session manager generates it. If Chrome is responsible for generating the
    // owner key, the notification is not useful.
    return;
  }

  if (success)
    ReloadKeypair();
}

void OwnerSettingsServiceAsh::OwnershipStatusChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  StorePendingChanges();
}

void OwnerSettingsServiceAsh::DeviceSettingsUpdated() {
  DCHECK(thread_checker_.CalledOnValidThread());
  StorePendingChanges();
}

void OwnerSettingsServiceAsh::OnDeviceSettingsServiceShutdown() {
  device_settings_service_ = nullptr;
}

// static
void OwnerSettingsServiceAsh::IsOwnerForSafeModeAsync(
    const std::string& user_hash,
    const scoped_refptr<OwnerKeyUtil>& owner_key_util,
    IsOwnerCallback callback) {
  CHECK(LoginState::Get()->IsInSafeMode());

  // Make sure NSS is initialized and NSS DB is loaded for the user before
  // searching for the owner key.
  content::GetIOThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&crypto::InitializeNSSForChromeOSUser),
                     user_hash,
                     ProfileHelper::GetProfilePathByUserIdHash(user_hash)),
      base::BindOnce(&DoesPrivateKeyExistAsync, owner_key_util,
                     std::move(callback)));
}

// static
std::unique_ptr<em::PolicyData> OwnerSettingsServiceAsh::AssemblePolicy(
    const std::string& user_id,
    const em::PolicyData* policy_data,
    em::ChromeDeviceSettingsProto* settings) {
  std::unique_ptr<em::PolicyData> policy(new em::PolicyData());
  if (policy_data) {
    // Preserve management settings.
    if (policy_data->has_management_mode())
      policy->set_management_mode(policy_data->management_mode());
    if (policy_data->has_request_token())
      policy->set_request_token(policy_data->request_token());
    if (policy_data->has_device_id())
      policy->set_device_id(policy_data->device_id());
  } else {
    // If there's no previous policy data, this is the first time the device
    // setting is set. We set the management mode to LOCAL_OWNER initially.
    policy->set_management_mode(em::PolicyData::LOCAL_OWNER);
  }
  policy->set_policy_type(policy::dm_protocol::kChromeDevicePolicyType);
  policy->set_timestamp(
      (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds());
  policy->set_username(user_id);
  if (policy->management_mode() == em::PolicyData::LOCAL_OWNER)
    FixupLocalOwnerPolicy(user_id, settings);
  if (!settings->SerializeToString(policy->mutable_policy_value()))
    return nullptr;

  return policy;
}

// static
void OwnerSettingsServiceAsh::FixupLocalOwnerPolicy(
    const std::string& user_id,
    enterprise_management::ChromeDeviceSettingsProto* settings) {
  if (!settings->has_allow_new_users())
    settings->mutable_allow_new_users()->set_allow_new_users(true);

  // Only add the owner id to the whitelist if the allowlist doesn't exist.
  // Otherwise, use the allowlist.
  if (settings->has_user_whitelist() && !settings->has_user_allowlist()) {
    em::UserWhitelistProto* whitelist_proto =
        settings->mutable_user_whitelist();
    if (!base::Contains(whitelist_proto->user_whitelist(), user_id))
      whitelist_proto->add_user_whitelist(user_id);
  } else {
    em::UserAllowlistProto* allowlist_proto =
        settings->mutable_user_allowlist();
    if (!base::Contains(allowlist_proto->user_allowlist(), user_id))
      allowlist_proto->add_user_allowlist(user_id);
  }
}

// static
void OwnerSettingsServiceAsh::UpdateDeviceSettings(
    const std::string& path,
    const base::Value& value,
    enterprise_management::ChromeDeviceSettingsProto& settings) {
  if (path == kAccountsPrefAllowNewUser) {
    em::AllowNewUsersProto* allow = settings.mutable_allow_new_users();
    if (value.is_bool()) {
      allow->set_allow_new_users(value.GetBool());
    } else {
      NOTREACHED();
    }
  } else if (path == kAccountsPrefAllowGuest) {
    em::GuestModeEnabledProto* guest = settings.mutable_guest_mode_enabled();
    if (value.is_bool())
      guest->set_guest_mode_enabled(value.GetBool());
    else
      NOTREACHED();
  } else if (path == kAccountsPrefShowUserNamesOnSignIn) {
    em::ShowUserNamesOnSigninProto* show = settings.mutable_show_user_names();
    if (value.is_bool())
      show->set_show_user_names(value.GetBool());
    else
      NOTREACHED();
  } else if (path == kAccountsPrefDeviceLocalAccounts) {
    em::DeviceLocalAccountsProto* device_local_accounts =
        settings.mutable_device_local_accounts();
    device_local_accounts->clear_account();
    if (value.is_list()) {
      for (const auto& entry : value.GetList()) {
        if (entry.is_dict()) {
          const base::Value::Dict& entry_dict = entry.GetDict();
          em::DeviceLocalAccountInfoProto* account =
              device_local_accounts->add_account();
          const std::string* account_id =
              entry_dict.FindString(kAccountsPrefDeviceLocalAccountsKeyId);
          if (account_id)
            account->set_account_id(*account_id);

          absl::optional<int> type =
              entry_dict.FindInt(kAccountsPrefDeviceLocalAccountsKeyType);
          if (type.has_value()) {
            account->set_type(
                static_cast<em::DeviceLocalAccountInfoProto::AccountType>(
                    type.value()));
          }
          const std::string* kiosk_app_id = entry_dict.FindString(
              kAccountsPrefDeviceLocalAccountsKeyKioskAppId);
          if (kiosk_app_id)
            account->mutable_kiosk_app()->set_app_id(*kiosk_app_id);

          const std::string* kiosk_app_update_url = entry_dict.FindString(
              kAccountsPrefDeviceLocalAccountsKeyKioskAppUpdateURL);
          if (kiosk_app_update_url)
            account->mutable_kiosk_app()->set_update_url(*kiosk_app_update_url);
        } else {
          NOTREACHED();
        }
      }
    } else {
      NOTREACHED();
    }
  } else if (path == kAccountsPrefDeviceLocalAccountAutoLoginId) {
    em::DeviceLocalAccountsProto* device_local_accounts =
        settings.mutable_device_local_accounts();
    if (value.is_string())
      device_local_accounts->set_auto_login_id(value.GetString());
    else
      NOTREACHED();
  } else if (path == kAccountsPrefDeviceLocalAccountAutoLoginDelay) {
    em::DeviceLocalAccountsProto* device_local_accounts =
        settings.mutable_device_local_accounts();
    if (value.is_int())
      device_local_accounts->set_auto_login_delay(value.GetInt());
    else
      NOTREACHED();
  } else if (path == kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled) {
    em::DeviceLocalAccountsProto* device_local_accounts =
        settings.mutable_device_local_accounts();
    if (value.is_bool())
      device_local_accounts->set_enable_auto_login_bailout(value.GetBool());
    else
      NOTREACHED();
  } else if (path ==
             kAccountsPrefDeviceLocalAccountPromptForNetworkWhenOffline) {
    em::DeviceLocalAccountsProto* device_local_accounts =
        settings.mutable_device_local_accounts();
    if (value.is_bool())
      device_local_accounts->set_prompt_for_network_when_offline(
          value.GetBool());
    else
      NOTREACHED();
  } else if (path == kSignedDataRoamingEnabled) {
    em::DataRoamingEnabledProto* roam = settings.mutable_data_roaming_enabled();
    if (value.is_bool())
      roam->set_data_roaming_enabled(value.GetBool());
    else
      NOTREACHED();
  } else if (path == kReleaseChannel) {
    em::ReleaseChannelProto* release_channel =
        settings.mutable_release_channel();
    std::string channel_value;
    if (value.is_string())
      release_channel->set_release_channel(value.GetString());
    else
      NOTREACHED();
  } else if (path == kStatsReportingPref) {
    em::MetricsEnabledProto* metrics = settings.mutable_metrics_enabled();
    if (value.is_bool())
      metrics->set_metrics_enabled(value.GetBool());
    else
      NOTREACHED();
  } else if (path == kAccountsPrefUsers) {
    RepeatedPtrField<std::string>* list = nullptr;
    // Only use the whitelist if the allowlist isn't being used.
    if (settings.has_user_whitelist() && !settings.has_user_allowlist()) {
      list = settings.mutable_user_whitelist()->mutable_user_whitelist();
    } else {
      // Clear the whitelist when using the allowlist
      settings.mutable_user_whitelist()->clear_user_whitelist();
      list = settings.mutable_user_allowlist()->mutable_user_allowlist();
    }
    DCHECK(list);
    list->Clear();
    for (const auto& user : value.GetList()) {
      if (user.is_string()) {
        list->Add(std::string(user.GetString()));
      }
    }
  } else if (path == kAllowRedeemChromeOsRegistrationOffers) {
    em::AllowRedeemChromeOsRegistrationOffersProto* allow_redeem_offers =
        settings.mutable_allow_redeem_offers();
    if (value.is_bool()) {
      allow_redeem_offers->set_allow_redeem_offers(value.GetBool());
    } else {
      NOTREACHED();
    }
  } else if (path == kFeatureFlags) {
    em::FeatureFlagsProto* feature_flags = settings.mutable_feature_flags();
    feature_flags->Clear();
    if (value.is_list()) {
      for (const auto& flag : value.GetList()) {
        if (flag.is_string())
          feature_flags->add_feature_flags(flag.GetString());
      }
    }
  } else if (path == kSystemUse24HourClock) {
    em::SystemUse24HourClockProto* use_24hour_clock_proto =
        settings.mutable_use_24hour_clock();
    use_24hour_clock_proto->Clear();
    if (value.is_bool()) {
      use_24hour_clock_proto->set_use_24hour_clock(value.GetBool());
    } else {
      NOTREACHED();
    }
  } else if (path == kAttestationForContentProtectionEnabled) {
    em::AttestationSettingsProto* attestation_settings =
        settings.mutable_attestation_settings();
    if (value.is_bool()) {
      attestation_settings->set_content_protection_enabled(value.GetBool());
    } else {
      NOTREACHED();
    }
  } else if (path == kDevicePeripheralDataAccessEnabled) {
    em::DevicePciPeripheralDataAccessEnabledProtoV2*
        peripheral_data_access_proto =
            settings.mutable_device_pci_peripheral_data_access_enabled_v2();
    if (value.is_bool()) {
      peripheral_data_access_proto->set_enabled(value.GetBool());
    } else {
      NOTREACHED();
    }
  } else if (path == kRevenEnableDeviceHWDataUsage) {
    em::RevenDeviceHWDataUsageEnabledProto* hw_data_usage =
        settings.mutable_hardware_data_usage_enabled();
    if (value.is_bool())
      hw_data_usage->set_hardware_data_usage_enabled(value.GetBool());
    else
      NOTREACHED();
  } else {
    // The remaining settings don't support Set(), since they are not
    // intended to be customizable by the user:
    //   kAccountsPrefEphemeralUsersEnabled
    //   kAccountsPrefFamilyLinkAccountsAllowed
    //   kAccountsPrefTransferSAMLCookies
    //   kDeviceAttestationEnabled
    //   kDeviceOwner
    //   kDeviceReportRuntimeCounters
    //   kDeviceReportXDREvents
    //   kHeartbeatEnabled
    //   kHeartbeatFrequency
    //   kReleaseChannelDelegated
    //   kReportDeviceActivityTimes
    //   kReportDeviceAudioStatus
    //   KReportDeviceBacklightInfo
    //   kReportDeviceBluetoothInfo
    //   kReportDeviceBoardStatus
    //   kReportDeviceBootMode
    //   kReportDeviceCpuInfo
    //   kReportDeviceFanInfo
    //   kReportDeviceHardwareStatus
    //   kReportDeviceLocation
    //   kReportDeviceMemoryInfo
    //   kReportDeviceNetworkInterfaces
    //   kReportDeviceNetworkConfiguration
    //   kReportDeviceNetworkStatus
    //   kReportDevicePeripherals
    //   kReportDevicePowerStatus
    //   kReportDeviceStorageStatus
    //   kReportDeviceSecurityStatus
    //   kReportDeviceSessionStatus
    //   kReportDeviceGraphicsStatus
    //   kReportDeviceCrashReportInfoStatus
    //   kReportDeviceVersionInfo
    //   kReportDeviceVpdInfo
    //   kReportDeviceUsers
    //   kReportDeviceAppInfo
    //   kReportDeviceSystemInfo
    //   kReportDevicePrintJobs
    //   kReportDeviceLoginLogout
    //   kReportCRDSessions
    //   kServiceAccountIdentity
    //   kSystemTimezonePolicy
    //   kVariationsRestrictParameter
    //   kDeviceDisabled
    //   kDeviceDisabledMessage
    //   DeviceReportRuntimeCountersCheckingRateMs
    //   ReportDeviceNetworkTelemetryCollectionRateMs
    //   ReportDeviceNetworkTelemetryEventCheckingRateMs
    //   ReportDeviceAudioStatusCheckingRateMs

    LOG(FATAL) << "Device setting " << path << " is read-only.";
  }
}

void OwnerSettingsServiceAsh::OnPostKeypairLoadedActions() {
  DCHECK(thread_checker_.CalledOnValidThread());

  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile_);
  user_id_ = user ? user->GetAccountId().GetUserEmail() : std::string();

  const bool is_owner = IsOwner() || IsOwnerInTests(user_id_);
  if (is_owner && device_settings_service_)
    device_settings_service_->InitOwner(user_id_, weak_factory_.GetWeakPtr());

  has_pending_fixups_ = true;
}

void OwnerSettingsServiceAsh::ReloadKeypairImpl(
    base::OnceCallback<void(scoped_refptr<PublicKey>,
                            scoped_refptr<PrivateKey>)> callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // The profile may not be fully created yet: abort, and wait till it is. The
  // ProfileManager may be null in unit tests, in which case we can assume the
  // profile is valid.
  if (g_browser_process->profile_manager() &&
      !g_browser_process->profile_manager()->IsValidProfile(profile_)) {
    return;
  }

  if (waiting_for_tpm_token_) {
    return;
  }

  if (base::FeatureList::IsEnabled(ownership::kChromeSideOwnerKeyGeneration)) {
    const bool is_enterprise_managed = g_browser_process->platform_part()
                                           ->browser_policy_connector_ash()
                                           ->IsDeviceEnterpriseManaged();

    auto cb = base::BindOnce(&OwnerSettingsServiceAsh::OnReloadedKeypairImpl,
                             weak_factory_.GetWeakPtr(), std::move(callback));
    owner_key_loader_ = std::make_unique<OwnerKeyLoader>(
        profile_, device_settings_service_, owner_key_util_,
        is_enterprise_managed, std::move(cb));
    return owner_key_loader_->Run();
  }

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&LoadPrivateKeyOnIOThread, owner_key_util_,
                     ProfileHelper::GetUserIdHashFromProfile(profile_),
                     std::move(callback)));
}

void OwnerSettingsServiceAsh::OnReloadedKeypairImpl(
    base::OnceCallback<void(scoped_refptr<PublicKey>,
                            scoped_refptr<PrivateKey>)> callback,
    scoped_refptr<PublicKey> public_key,
    scoped_refptr<PrivateKey> private_key) {
  std::move(callback).Run(std::move(public_key), std::move(private_key));
  owner_key_loader_.reset();
}

void OwnerSettingsServiceAsh::StorePendingChanges() {
  if (!HasPendingChanges() || store_settings_factory_.HasWeakPtrs() ||
      !device_settings_service_ || user_id_.empty() || !IsOwner()) {
    return;
  }

  em::ChromeDeviceSettingsProto settings;
  if (tentative_settings_.get()) {
    settings.Swap(tentative_settings_.get());
    tentative_settings_.reset();
  } else if (device_settings_service_->status() ==
                 DeviceSettingsService::STORE_SUCCESS &&
             device_settings_service_->device_settings()) {
    settings = *device_settings_service_->device_settings();
    MigrateFeatureFlags(&settings);
  } else if (base::FeatureList::IsEnabled(
                 ownership::kChromeSideOwnerKeyGeneration) &&
             public_key_ && !public_key_->is_persisted()) {
    // A new owner key was generated and is not stored yet. Proceed to send it
    // to session manager.
  } else {
    return;
  }

  for (const auto& change : pending_changes_)
    UpdateDeviceSettings(change.first, *change.second.get(), settings);
  pending_changes_.clear();

  std::unique_ptr<em::PolicyData> policy = AssemblePolicy(
      user_id_, device_settings_service_->policy_data(), &settings);
  has_pending_fixups_ = false;

  scoped_refptr<base::TaskRunner> task_runner =
      base::ThreadPool::CreateTaskRunner({base::MayBlock()});
  bool rv = AssembleAndSignPolicyAsync(
      task_runner.get(), std::move(policy),
      base::BindOnce(&OwnerSettingsServiceAsh::OnPolicyAssembledAndSigned,
                     store_settings_factory_.GetWeakPtr()));
  RecordOwnerKeyEvent(OwnerKeyEvent::kStartSigningPolicy, /*success=*/rv);
  if (!rv)
    ReportStatusAndContinueStoring(false /* success */);
}

void OwnerSettingsServiceAsh::OnPolicyAssembledAndSigned(
    scoped_refptr<ownership::PublicKey> public_key,
    std::unique_ptr<em::PolicyFetchResponse> policy_response) {
  RecordOwnerKeyEvent(OwnerKeyEvent::kSignedPolicy,
                      /*success=*/policy_response.get());

  if (!policy_response.get() || !device_settings_service_) {
    ReportStatusAndContinueStoring(false /* success */);
    return;
  }
  device_settings_service_->Store(
      std::move(policy_response),
      base::BindOnce(&OwnerSettingsServiceAsh::OnSignedPolicyStored,
                     store_settings_factory_.GetWeakPtr(),
                     std::move(public_key), /*success=*/true));
}

void OwnerSettingsServiceAsh::OnSignedPolicyStored(
    scoped_refptr<ownership::PublicKey> public_key,
    bool success) {
  RecordOwnerKeyEvent(OwnerKeyEvent::kStoredPolicy, success);
  if (success) {
    public_key->mark_persisted();
  }

  CHECK(device_settings_service_);
  ReportStatusAndContinueStoring(success &&
                                 device_settings_service_->status() ==
                                     DeviceSettingsService::STORE_SUCCESS);
}

void OwnerSettingsServiceAsh::ReportStatusAndContinueStoring(bool success) {
  store_settings_factory_.InvalidateWeakPtrs();
  for (auto& observer : observers_)
    observer.OnSignedPolicyStored(success);
  StorePendingChanges();
}

void OwnerSettingsServiceAsh::MigrateFeatureFlags(
    enterprise_management::ChromeDeviceSettingsProto* settings) {
  DCHECK(IsOwner() || IsOwnerInTests(user_id_));

  if (settings->feature_flags().switches_size() == 0) {
    return;
  }

  em::FeatureFlagsProto* feature_flags = settings->mutable_feature_flags();
  if (feature_flags->feature_flags_size() != 0) {
    // Both old and new settings. This shouldn't happen in practice, but if it
    // does the most probable explanation is that we already migrated, so get
    // rid of the raw switches.
    feature_flags->clear_switches();
    return;
  }

  about_flags::OwnerFlagsStorage flags_storage(profile_->GetPrefs(), this);
  std::set<std::string> flags = flags_storage.GetFlags();
  for (const auto& flag : flags) {
    feature_flags->add_feature_flags(flag);
  }
  feature_flags->clear_switches();
}

}  // namespace ash

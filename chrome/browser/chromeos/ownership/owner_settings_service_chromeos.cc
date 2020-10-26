// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"

#include <keyhi.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/tpm/install_attributes.h"
#include "chromeos/tpm/tpm_token_loader.h"
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

namespace em = enterprise_management;

using content::BrowserThread;
using google::protobuf::RepeatedPtrField;
using ownership::OwnerKeyUtil;
using ownership::PrivateKey;
using ownership::PublicKey;

namespace chromeos {

namespace {

using ReloadKeyCallback =
    base::Callback<void(const scoped_refptr<PublicKey>& public_key,
                        const scoped_refptr<PrivateKey>& private_key)>;

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
    const ReloadKeyCallback& callback) {
  std::vector<uint8_t> public_key_data;
  scoped_refptr<PublicKey> public_key;
  if (!owner_key_util->ImportPublicKey(&public_key_data)) {
    scoped_refptr<PrivateKey> private_key;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(callback, public_key, private_key));
    return;
  }
  public_key = new PublicKey();
  public_key->data().swap(public_key_data);

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
      FROM_HERE, base::BindOnce(callback, public_key, private_key));
}

void ContinueLoadPrivateKeyOnIOThread(
    const scoped_refptr<OwnerKeyUtil>& owner_key_util,
    const std::string username_hash,
    const ReloadKeyCallback& callback,
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
                     std::move(private_slot), callback));
}

void LoadPrivateKeyOnIOThread(const scoped_refptr<OwnerKeyUtil>& owner_key_util,
                              const std::string username_hash,
                              const ReloadKeyCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  crypto::EnsureNSSInit();

  auto continue_load_private_key_callback =
      base::Bind(&ContinueLoadPrivateKeyOnIOThread, owner_key_util,
                 username_hash, callback);

  crypto::ScopedPK11Slot private_slot = crypto::GetPrivateSlotForChromeOSUser(
      username_hash, continue_load_private_key_callback);
  if (private_slot)
    continue_load_private_key_callback.Run(std::move(private_slot));
}

bool DoesPrivateKeyExistAsyncHelper(
    const scoped_refptr<OwnerKeyUtil>& owner_key_util) {
  std::vector<uint8_t> public_key;
  if (!owner_key_util->ImportPublicKey(&public_key))
    return false;
  crypto::ScopedSECKEYPrivateKey key =
      crypto::FindNSSKeyFromPublicKeyInfo(public_key);
  return key && SECKEY_GetPrivateKeyType(key.get()) == rsaKey;
}

// Checks whether NSS slots with private key are mounted or
// not. Responds via |callback|.
void DoesPrivateKeyExistAsync(
    const scoped_refptr<OwnerKeyUtil>& owner_key_util,
    OwnerSettingsServiceChromeOS::IsOwnerCallback callback) {
  if (!owner_key_util.get()) {
    std::move(callback).Run(false);
    return;
  }
  scoped_refptr<base::TaskRunner> task_runner =
      base::ThreadPool::CreateTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  base::PostTaskAndReplyWithResult(
      task_runner.get(), FROM_HERE,
      base::BindOnce(&DoesPrivateKeyExistAsyncHelper, owner_key_util),
      std::move(callback));
}

}  // namespace

OwnerSettingsServiceChromeOS::ManagementSettings::ManagementSettings() {
}

OwnerSettingsServiceChromeOS::ManagementSettings::~ManagementSettings() {
}

OwnerSettingsServiceChromeOS::OwnerSettingsServiceChromeOS(
    DeviceSettingsService* device_settings_service,
    Profile* profile,
    const scoped_refptr<OwnerKeyUtil>& owner_key_util)
    : ownership::OwnerSettingsService(owner_key_util),
      device_settings_service_(device_settings_service),
      profile_(profile) {
  if (TPMTokenLoader::IsInitialized()) {
    TPMTokenLoader::TPMTokenStatus tpm_token_status =
        TPMTokenLoader::Get()->IsTPMTokenEnabled(
            base::BindOnce(&OwnerSettingsServiceChromeOS::OnTPMTokenReady,
                           weak_factory_.GetWeakPtr()));
    waiting_for_tpm_token_ =
        tpm_token_status == TPMTokenLoader::TPM_TOKEN_STATUS_UNDETERMINED;
  }

  if (SessionManagerClient::Get())
    SessionManagerClient::Get()->AddObserver(this);

  if (device_settings_service_)
    device_settings_service_->AddObserver(this);

  if (!user_manager::UserManager::IsInitialized()) {
    // interactive_ui_tests does not set user manager.
    waiting_for_easy_unlock_operation_finshed_ = false;
    return;
  }

  UserSessionManager::GetInstance()->WaitForEasyUnlockKeyOpsFinished(
      base::BindOnce(&OwnerSettingsServiceChromeOS::OnEasyUnlockKeyOpsFinished,
                     weak_factory_.GetWeakPtr()));
  // The ProfileManager may be null in unit tests.
  if (g_browser_process->profile_manager())
    g_browser_process->profile_manager()->AddObserver(this);
}

OwnerSettingsServiceChromeOS::~OwnerSettingsServiceChromeOS() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // The ProfileManager may be null in unit tests.
  if (g_browser_process->profile_manager())
    g_browser_process->profile_manager()->RemoveObserver(this);

  if (device_settings_service_)
    device_settings_service_->RemoveObserver(this);

  if (SessionManagerClient::Get())
    SessionManagerClient::Get()->RemoveObserver(this);
}

OwnerSettingsServiceChromeOS* OwnerSettingsServiceChromeOS::FromWebUI(
    content::WebUI* web_ui) {
  if (!web_ui)
    return nullptr;
  Profile* profile = Profile::FromWebUI(web_ui);
  if (!profile)
    return nullptr;
  return OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(profile);
}

void OwnerSettingsServiceChromeOS::OnTPMTokenReady(
    bool /* tpm_token_enabled */) {
  DCHECK(thread_checker_.CalledOnValidThread());
  waiting_for_tpm_token_ = false;

  // TPMTokenLoader initializes the TPM and NSS database which is necessary to
  // determine ownership. Force a reload once we know these are initialized.
  ReloadKeypair();
}

void OwnerSettingsServiceChromeOS::OnEasyUnlockKeyOpsFinished() {
  DCHECK(thread_checker_.CalledOnValidThread());
  waiting_for_easy_unlock_operation_finshed_ = false;

  ReloadKeypair();
}

bool OwnerSettingsServiceChromeOS::HasPendingChanges() const {
  return !pending_changes_.empty() || tentative_settings_.get() ||
         has_pending_fixups_;
}

bool OwnerSettingsServiceChromeOS::IsOwner() {
  if (InstallAttributes::Get()->IsEnterpriseManaged()) {
    return false;
  }
  return OwnerSettingsService::IsOwner();
}

void OwnerSettingsServiceChromeOS::IsOwnerAsync(IsOwnerCallback callback) {
  if (InstallAttributes::Get()->IsEnterpriseManaged()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  OwnerSettingsService::IsOwnerAsync(std::move(callback));
}

bool OwnerSettingsServiceChromeOS::HandlesSetting(const std::string& setting) {
  return DeviceSettingsProvider::IsDeviceSetting(setting);
}

bool OwnerSettingsServiceChromeOS::Set(const std::string& setting,
                                       const base::Value& value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!IsOwner() && !IsOwnerInTests(user_id_))
    return false;

  pending_changes_[setting] = base::WrapUnique(value.DeepCopy());

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

bool OwnerSettingsServiceChromeOS::AppendToList(const std::string& setting,
                                                const base::Value& value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const base::Value* old_value = CrosSettings::Get()->GetPref(setting);
  if (old_value && !old_value->is_list())
    return false;
  std::unique_ptr<base::ListValue> new_value(
      old_value ? static_cast<const base::ListValue*>(old_value)->DeepCopy()
                : new base::ListValue());
  new_value->Append(value.CreateDeepCopy());
  return Set(setting, *new_value);
}

bool OwnerSettingsServiceChromeOS::RemoveFromList(const std::string& setting,
                                                  const base::Value& value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const base::Value* old_value = CrosSettings::Get()->GetPref(setting);
  if (old_value && !old_value->is_list())
    return false;
  std::unique_ptr<base::ListValue> new_value(
      old_value ? static_cast<const base::ListValue*>(old_value)->DeepCopy()
                : new base::ListValue());
  new_value->Remove(value, nullptr);
  return Set(setting, *new_value);
}

bool OwnerSettingsServiceChromeOS::CommitTentativeDeviceSettings(
    std::unique_ptr<enterprise_management::PolicyData> policy) {
  if (!IsOwner() && !IsOwnerInTests(user_id_))
    return false;
  if (policy->username() != user_id_) {
    LOG(ERROR) << "Username mismatch: " << policy->username() << " vs. "
               << user_id_;
    return false;
  }
  tentative_settings_.reset(new em::ChromeDeviceSettingsProto);
  CHECK(tentative_settings_->ParseFromString(policy->policy_value()));
  StorePendingChanges();
  return true;
}

void OwnerSettingsServiceChromeOS::OnProfileAdded(Profile* profile) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (profile != profile_)
    return;

  g_browser_process->profile_manager()->RemoveObserver(this);
  ReloadKeypair();
}

void OwnerSettingsServiceChromeOS::OwnerKeySet(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (success)
    ReloadKeypair();
}

void OwnerSettingsServiceChromeOS::OwnershipStatusChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  StorePendingChanges();
}

void OwnerSettingsServiceChromeOS::DeviceSettingsUpdated() {
  DCHECK(thread_checker_.CalledOnValidThread());
  StorePendingChanges();
}

void OwnerSettingsServiceChromeOS::OnDeviceSettingsServiceShutdown() {
  device_settings_service_ = nullptr;
}

// static
void OwnerSettingsServiceChromeOS::IsOwnerForSafeModeAsync(
    const std::string& user_hash,
    const scoped_refptr<OwnerKeyUtil>& owner_key_util,
    IsOwnerCallback callback) {
  CHECK(chromeos::LoginState::Get()->IsInSafeMode());

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
std::unique_ptr<em::PolicyData> OwnerSettingsServiceChromeOS::AssemblePolicy(
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
    return std::unique_ptr<em::PolicyData>();

  return policy;
}

// static
void OwnerSettingsServiceChromeOS::FixupLocalOwnerPolicy(
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
void OwnerSettingsServiceChromeOS::UpdateDeviceSettings(
    const std::string& path,
    const base::Value& value,
    enterprise_management::ChromeDeviceSettingsProto& settings) {
  if (path == kAccountsPrefAllowNewUser) {
    em::AllowNewUsersProto* allow = settings.mutable_allow_new_users();
    bool allow_value;
    if (value.GetAsBoolean(&allow_value)) {
      allow->set_allow_new_users(allow_value);
    } else {
      NOTREACHED();
    }
  } else if (path == kAccountsPrefAllowGuest) {
    em::GuestModeEnabledProto* guest = settings.mutable_guest_mode_enabled();
    bool guest_value;
    if (value.GetAsBoolean(&guest_value))
      guest->set_guest_mode_enabled(guest_value);
    else
      NOTREACHED();
  } else if (path == kAccountsPrefShowUserNamesOnSignIn) {
    em::ShowUserNamesOnSigninProto* show = settings.mutable_show_user_names();
    bool show_value;
    if (value.GetAsBoolean(&show_value))
      show->set_show_user_names(show_value);
    else
      NOTREACHED();
  } else if (path == kAccountsPrefDeviceLocalAccounts) {
    em::DeviceLocalAccountsProto* device_local_accounts =
        settings.mutable_device_local_accounts();
    device_local_accounts->clear_account();
    const base::ListValue* accounts_list = NULL;
    if (value.GetAsList(&accounts_list)) {
      for (base::ListValue::const_iterator entry(accounts_list->begin());
           entry != accounts_list->end();
           ++entry) {
        const base::DictionaryValue* entry_dict = NULL;
        if (entry->GetAsDictionary(&entry_dict)) {
          em::DeviceLocalAccountInfoProto* account =
              device_local_accounts->add_account();
          std::string account_id;
          if (entry_dict->GetStringWithoutPathExpansion(
                  kAccountsPrefDeviceLocalAccountsKeyId, &account_id)) {
            account->set_account_id(account_id);
          }
          int type;
          if (entry_dict->GetIntegerWithoutPathExpansion(
                  kAccountsPrefDeviceLocalAccountsKeyType, &type)) {
            account->set_type(
                static_cast<em::DeviceLocalAccountInfoProto::AccountType>(
                    type));
          }
          std::string kiosk_app_id;
          if (entry_dict->GetStringWithoutPathExpansion(
                  kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
                  &kiosk_app_id)) {
            account->mutable_kiosk_app()->set_app_id(kiosk_app_id);
          }
          std::string kiosk_app_update_url;
          if (entry_dict->GetStringWithoutPathExpansion(
                  kAccountsPrefDeviceLocalAccountsKeyKioskAppUpdateURL,
                  &kiosk_app_update_url)) {
            account->mutable_kiosk_app()->set_update_url(kiosk_app_update_url);
          }
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
    std::string id;
    if (value.GetAsString(&id))
      device_local_accounts->set_auto_login_id(id);
    else
      NOTREACHED();
  } else if (path == kAccountsPrefDeviceLocalAccountAutoLoginDelay) {
    em::DeviceLocalAccountsProto* device_local_accounts =
        settings.mutable_device_local_accounts();
    int delay;
    if (value.GetAsInteger(&delay))
      device_local_accounts->set_auto_login_delay(delay);
    else
      NOTREACHED();
  } else if (path == kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled) {
    em::DeviceLocalAccountsProto* device_local_accounts =
        settings.mutable_device_local_accounts();
    bool enabled;
    if (value.GetAsBoolean(&enabled))
      device_local_accounts->set_enable_auto_login_bailout(enabled);
    else
      NOTREACHED();
  } else if (path ==
             kAccountsPrefDeviceLocalAccountPromptForNetworkWhenOffline) {
    em::DeviceLocalAccountsProto* device_local_accounts =
        settings.mutable_device_local_accounts();
    bool should_prompt;
    if (value.GetAsBoolean(&should_prompt))
      device_local_accounts->set_prompt_for_network_when_offline(should_prompt);
    else
      NOTREACHED();
  } else if (path == kSignedDataRoamingEnabled) {
    em::DataRoamingEnabledProto* roam = settings.mutable_data_roaming_enabled();
    bool roaming_value = false;
    if (value.GetAsBoolean(&roaming_value))
      roam->set_data_roaming_enabled(roaming_value);
    else
      NOTREACHED();
  } else if (path == kReleaseChannel) {
    em::ReleaseChannelProto* release_channel =
        settings.mutable_release_channel();
    std::string channel_value;
    if (value.GetAsString(&channel_value))
      release_channel->set_release_channel(channel_value);
    else
      NOTREACHED();
  } else if (path == kStatsReportingPref) {
    em::MetricsEnabledProto* metrics = settings.mutable_metrics_enabled();
    bool metrics_value = false;
    if (value.GetAsBoolean(&metrics_value))
      metrics->set_metrics_enabled(metrics_value);
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
  } else if (path == kAccountsPrefEphemeralUsersEnabled) {
    em::EphemeralUsersEnabledProto* ephemeral_users_enabled =
        settings.mutable_ephemeral_users_enabled();
    bool ephemeral_users_enabled_value = false;
    if (value.GetAsBoolean(&ephemeral_users_enabled_value)) {
      ephemeral_users_enabled->set_ephemeral_users_enabled(
          ephemeral_users_enabled_value);
    } else {
      NOTREACHED();
    }
  } else if (path == kAllowRedeemChromeOsRegistrationOffers) {
    em::AllowRedeemChromeOsRegistrationOffersProto* allow_redeem_offers =
        settings.mutable_allow_redeem_offers();
    bool allow_redeem_offers_value;
    if (value.GetAsBoolean(&allow_redeem_offers_value)) {
      allow_redeem_offers->set_allow_redeem_offers(allow_redeem_offers_value);
    } else {
      NOTREACHED();
    }
  } else if (path == kStartUpFlags) {
    em::StartUpFlagsProto* flags_proto = settings.mutable_start_up_flags();
    flags_proto->Clear();
    const base::ListValue* flags;
    if (value.GetAsList(&flags)) {
      for (base::ListValue::const_iterator i = flags->begin();
           i != flags->end();
           ++i) {
        std::string flag;
        if (i->GetAsString(&flag))
          flags_proto->add_flags(flag);
      }
    }
  } else if (path == kSystemUse24HourClock) {
    em::SystemUse24HourClockProto* use_24hour_clock_proto =
        settings.mutable_use_24hour_clock();
    use_24hour_clock_proto->Clear();
    bool use_24hour_clock_value;
    if (value.GetAsBoolean(&use_24hour_clock_value)) {
      use_24hour_clock_proto->set_use_24hour_clock(use_24hour_clock_value);
    } else {
      NOTREACHED();
    }
  } else if (path == kAttestationForContentProtectionEnabled) {
    em::AttestationSettingsProto* attestation_settings =
        settings.mutable_attestation_settings();
    bool setting_enabled;
    if (value.GetAsBoolean(&setting_enabled)) {
      attestation_settings->set_content_protection_enabled(setting_enabled);
    } else {
      NOTREACHED();
    }
  } else {
    // The remaining settings don't support Set(), since they are not
    // intended to be customizable by the user:
    //   kAccountsPrefFamilyLinkAccountsAllowed
    //   kAccountsPrefSupervisedUsersEnabled
    //   kAccountsPrefTransferSAMLCookies
    //   kDeviceAttestationEnabled
    //   kDeviceOwner
    //   kHeartbeatEnabled
    //   kHeartbeatFrequency
    //   kReleaseChannelDelegated
    //   kReportDeviceActivityTimes
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
    //   kReportDevicePowerStatus
    //   kReportDeviceStorageStatus
    //   kReportDeviceSessionStatus
    //   kReportDeviceGraphicsStatus
    //   kReportDeviceCrashReportInfoStatus
    //   kReportDeviceVersionInfo
    //   kReportDeviceVpdInfo
    //   kReportDeviceUsers
    //   kReportDeviceAppInfo
    //   kReportDeviceSystemInfo
    //   kServiceAccountIdentity
    //   kSystemTimezonePolicy
    //   kVariationsRestrictParameter
    //   kDeviceDisabled
    //   kDeviceDisabledMessage

    LOG(FATAL) << "Device setting " << path << " is read-only.";
  }
}

void OwnerSettingsServiceChromeOS::OnPostKeypairLoadedActions() {
  DCHECK(thread_checker_.CalledOnValidThread());

  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile_);
  user_id_ = user ? user->GetAccountId().GetUserEmail() : std::string();

  const bool is_owner = IsOwner() || IsOwnerInTests(user_id_);
  if (is_owner && device_settings_service_)
    device_settings_service_->InitOwner(user_id_, weak_factory_.GetWeakPtr());

  has_pending_fixups_ = true;
}

void OwnerSettingsServiceChromeOS::ReloadKeypairImpl(const base::Callback<
    void(const scoped_refptr<PublicKey>& public_key,
         const scoped_refptr<PrivateKey>& private_key)>& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // The profile may not be fully created yet: abort, and wait till it is. The
  // ProfileManager may be null in unit tests, in which case we can assume the
  // profile is valid.
  if (g_browser_process->profile_manager() &&
      !g_browser_process->profile_manager()->IsValidProfile(profile_)) {
    return;
  }

  if (waiting_for_tpm_token_ || waiting_for_easy_unlock_operation_finshed_)
    return;

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&LoadPrivateKeyOnIOThread, owner_key_util_,
                     ProfileHelper::GetUserIdHashFromProfile(profile_),
                     callback));
}

void OwnerSettingsServiceChromeOS::StorePendingChanges() {
  if (!HasPendingChanges() || store_settings_factory_.HasWeakPtrs() ||
      !device_settings_service_ || user_id_.empty()) {
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
  } else {
    return;
  }

  for (const auto& change : pending_changes_)
    UpdateDeviceSettings(change.first, *change.second.get(), settings);
  pending_changes_.clear();

  std::unique_ptr<em::PolicyData> policy =
      AssemblePolicy(user_id_, device_settings_service_->policy_data(),
                     &settings);
  has_pending_fixups_ = false;

  scoped_refptr<base::TaskRunner> task_runner =
      base::ThreadPool::CreateTaskRunner({base::MayBlock()});
  bool rv = AssembleAndSignPolicyAsync(
      task_runner.get(), std::move(policy),
      base::BindOnce(&OwnerSettingsServiceChromeOS::OnPolicyAssembledAndSigned,
                     store_settings_factory_.GetWeakPtr()));
  if (!rv)
    ReportStatusAndContinueStoring(false /* success */);
}

void OwnerSettingsServiceChromeOS::OnPolicyAssembledAndSigned(
    std::unique_ptr<em::PolicyFetchResponse> policy_response) {
  if (!policy_response.get() || !device_settings_service_) {
    ReportStatusAndContinueStoring(false /* success */);
    return;
  }
  device_settings_service_->Store(
      std::move(policy_response),
      base::Bind(&OwnerSettingsServiceChromeOS::OnSignedPolicyStored,
                 store_settings_factory_.GetWeakPtr(), true /* success */));
}

void OwnerSettingsServiceChromeOS::OnSignedPolicyStored(bool success) {
  CHECK(device_settings_service_);
  ReportStatusAndContinueStoring(success &&
                                 device_settings_service_->status() ==
                                     DeviceSettingsService::STORE_SUCCESS);
}

void OwnerSettingsServiceChromeOS::ReportStatusAndContinueStoring(
    bool success) {
  store_settings_factory_.InvalidateWeakPtrs();
  for (auto& observer : observers_)
    observer.OnSignedPolicyStored(success);
  StorePendingChanges();
}

}  // namespace chromeos

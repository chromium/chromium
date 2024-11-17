// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_service.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chrome/browser/ash/file_manager/file_manager_pref_names.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager.h"
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/smb_client/discovery/mdns_host_locator.h"
#include "chrome/browser/ash/smb_client/discovery/netbios_client.h"
#include "chrome/browser/ash/smb_client/discovery/netbios_host_locator.h"
#include "chrome/browser/ash/smb_client/smb_file_system.h"
#include "chrome/browser/ash/smb_client/smb_file_system_id.h"
#include "chrome/browser/ash/smb_client/smb_kerberos_credentials_updater.h"
#include "chrome/browser/ash/smb_client/smb_provider.h"
#include "chrome/browser/ash/smb_client/smb_service_helper.h"
#include "chrome/browser/ash/smb_client/smb_share_info.h"
#include "chrome/browser/ash/smb_client/smb_url.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/webui/ash/smb_shares/smb_credentials_dialog.h"
#include "chrome/browser/ui/webui/ash/smb_shares/smb_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "crypto/random.h"
#include "net/base/network_interfaces.h"
#include "url/url_util.h"

namespace ash::smb_client {

namespace {

const char kShareUrlKey[] = "share_url";
const char kModeKey[] = "mode";
const char kModeDropDownValue[] = "drop_down";
const char kModePreMountValue[] = "pre_mount";
const char kModeUnknownValue[] = "unknown";
// Maximum number of smbfs shares to be mounted at the same time, only enforced
// on user-initiated mount requests.
const size_t kMaxSmbFsShares = 16;
// Length of salt used to obfuscate stored password in smbfs.
const size_t kSaltLength = 16;
static_assert(kSaltLength >=
                  smbfs::mojom::CredentialStorageOptions::kMinSaltLength,
              "Minimum salt length is "
              "smbfs::mojom::CredentialStorageOptions::kMinSaltLength");

net::NetworkInterfaceList GetInterfaces() {
  net::NetworkInterfaceList list;
  if (!net::GetNetworkList(&list, net::EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES)) {
    LOG(ERROR) << "GetInterfaces failed";
  }
  return list;
}

std::unique_ptr<NetBiosClientInterface> GetNetBiosClient(Profile* profile) {
  auto* network_context =
      profile->GetDefaultStoragePartition()->GetNetworkContext();
  return std::make_unique<NetBiosClient>(network_context);
}

// Metric recording functions.

// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
enum class AuthMethod {
  kNoCredentials = 0,
  kUsernameOnly = 1,
  kUsernameAndPassword = 2,
  kSSOKerberosAD = 3,
  kSSOKerberosGaia = 4,
  kMaxValue = kSSOKerberosGaia,
};

void RecordMountResult(SmbMountResult result) {
  DCHECK_LE(result, SmbMountResult::kMaxValue);
  UMA_HISTOGRAM_ENUMERATION("NativeSmbFileShare.MountResult", result);
}

void RecordAuthenticationMethod(AuthMethod method) {
  DCHECK_LE(method, AuthMethod::kMaxValue);
  UMA_HISTOGRAM_ENUMERATION("NativeSmbFileShare.AuthenticationMethod", method);
}

}  // namespace

bool SmbService::disable_share_discovery_for_testing_ = false;

SmbService::SmbService(Profile* profile,
                       std::unique_ptr<base::TickClock> tick_clock)
    : provider_id_(file_system_provider::ProviderId::CreateFromNativeId("smb")),
      profile_(profile),
      registry_(profile) {
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile_);
  DCHECK(user);

  SmbProviderClient* client = GetSmbProviderClient();
  if (!client) {
    return;
  }

  KerberosCredentialsManager* credentials_manager =
      KerberosCredentialsManagerFactory::GetExisting(profile);
  if (credentials_manager) {
    if (!base::FeatureList::IsEnabled(features::kSmbproviderdOnDemand)) {
      kerberos_credentials_updater_ =
          std::make_unique<SmbKerberosCredentialsUpdater>(
              credentials_manager,
              base::BindRepeating(&SmbService::UpdateKerberosCredentials,
                                  weak_ptr_factory_.GetWeakPtr()));
      SetupKerberos(kerberos_credentials_updater_->active_account_name());
      return;
    }

    // There is no need to call `UpdateKerberosCredentials`, which leads to the
    // DBus method to set up Kerberos when `kSmbproviderdOnDemand` is enabled,
    // since setting up Kerberos authentication is now implemented in smbfs and
    // this path is unnecessary.
    kerberos_credentials_updater_ =
        std::make_unique<SmbKerberosCredentialsUpdater>(credentials_manager,
                                                        base::DoNothing());
  }

  // Post a task to complete setup. This is to allow unit tests to perform
  // expectations setup after constructing an instance. It also mirrors the
  // behaviour when Kerberos is being used.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SmbService::CompleteSetup,
                                weak_ptr_factory_.GetWeakPtr()));
}

SmbService::~SmbService() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void SmbService::Shutdown() {
  // Unmount and destroy all smbfs instances explicitly before destruction,
  // since SmbFsShare accesses KeyedServices on destruction.
  smbfs_shares_.clear();
}

// static
void SmbService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kNetworkFileSharesAllowed, true);
  registry->RegisterBooleanPref(prefs::kNetBiosShareDiscoveryEnabled, true);
  registry->RegisterBooleanPref(prefs::kNTLMShareAuthenticationEnabled, true);
  registry->RegisterListPref(prefs::kNetworkFileSharesPreconfiguredShares);
  registry->RegisterStringPref(prefs::kMostRecentlyUsedNetworkFileShareURL, "");
  SmbPersistedShareRegistry::RegisterProfilePrefs(registry);
}

void SmbService::UnmountSmbFs(const base::FilePath& mount_path) {
  DCHECK(!mount_path.empty());

  for (auto it = smbfs_shares_.begin(); it != smbfs_shares_.end(); ++it) {
    SmbFsShare* share = it->second.get();
    if (share->mount_path() == mount_path) {
      if (share->options().save_restore_password) {
        share->RemoveSavedCredentials(
            base::BindOnce(&SmbService::OnSmbfsRemoveSavedCredentialsDone,
                           base::Unretained(this), it->first));
      } else {
        // If the password wasn't saved, there's nothing for smbfs to do.
        OnSmbfsRemoveSavedCredentialsDone(it->first, true /* success */);
      }
      return;
    }
  }

  LOG(WARNING) << "Smbfs mount path not found: " << mount_path;
}

void SmbService::OnSmbfsRemoveSavedCredentialsDone(const std::string& mount_id,
                                                   bool success) {
  DCHECK(!mount_id.empty());

  auto it = smbfs_shares_.find(mount_id);
  if (it == smbfs_shares_.end()) {
    LOG(WARNING) << "Smbfs mount id " << mount_id << " already deleted";
    return;
  }

  // UnmountSmbFs() is called by an explicit unmount by the user. In this
  // case, forget the share.
  registry_.Delete(it->second->share_url());
  smbfs_shares_.erase(it);
}

SmbFsShare* SmbService::GetSmbFsShareForPath(const base::FilePath& path) {
  DCHECK(!path.empty());
  DCHECK(path.IsAbsolute());

  for (const auto& entry : smbfs_shares_) {
    const base::FilePath mount_path = entry.second->mount_path();
    if (mount_path == path || mount_path.IsParent(path)) {
      return entry.second.get();
    }
  }
  return nullptr;
}

void SmbService::GatherSharesInNetwork(HostDiscoveryResponse discovery_callback,
                                       GatherSharesResponse shares_callback) {
  auto preconfigured_shares = GetPreconfiguredSharePathsForDropdown();
  if (!preconfigured_shares.empty()) {
    shares_callback.Run(std::move(preconfigured_shares), false);
  }
  share_finder_->GatherSharesInNetwork(
      std::move(discovery_callback),
      base::BindOnce(
          [](GatherSharesResponse shares_callback,
             const std::vector<SmbUrl>& shares_gathered) {
            std::move(shares_callback).Run(shares_gathered, true);
          },
          std::move(shares_callback)));
}

void SmbService::Mount(const std::string& display_name,
                       const base::FilePath& share_path,
                       const std::string& username_input,
                       const std::string& password_input,
                       bool use_kerberos,
                       bool should_open_file_manager_after_mount,
                       bool save_credentials,
                       MountResponse callback) {
  SmbUrl parsed_url(share_path.value());
  if (!parsed_url.IsValid() || parsed_url.GetShare().empty()) {
    // Handle invalid URLs early to avoid having unaccounted for UMA counts for
    // authentication method.
    std::move(callback).Run(SmbMountResult::kInvalidUrl);
    return;
  }

  // When using kerberos, the URL must contain the hostname because that is used
  // to obtain the ticket. If the user enters an IP address, Samba will give us
  // a permission error, which isn't correct or useful to the end user.
  if (use_kerberos && url::HostIsIPAddress(parsed_url.GetHost())) {
    std::move(callback).Run(SmbMountResult::kInvalidSsoUrl);
    return;
  }

  if (IsShareMounted(parsed_url)) {
    // Prevent a share from being mounted twice. Although technically possible,
    // the UX when doing so is incomplete.
    std::move(callback).Run(SmbMountResult::kMountExists);
    return;
  }

  if (smbfs_shares_.size() >= kMaxSmbFsShares) {
    // Prevent users from mounting an excessive number of shares.
    std::move(callback).Run(SmbMountResult::kTooManyOpened);
    return;
  }

  std::string username;
  std::string password;
  std::string workgroup;

  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile_);
  DCHECK(user);

  if (use_kerberos) {
    // Differentiate between AD and KerberosEnabled via policy in metrics.
    if (IsKerberosEnabledViaPolicy()) {
      RecordAuthenticationMethod(AuthMethod::kSSOKerberosGaia);
    } else {
      RecordAuthenticationMethod(AuthMethod::kSSOKerberosAD);
    }

    // Get the user's username and workgroup from their email address to be used
    // for Kerberos authentication.
    ParseUserPrincipalName(user->GetDisplayEmail(), &username, &workgroup);
  } else {
    // Record authentication method metrics.
    if (!username_input.empty() && !password_input.empty()) {
      RecordAuthenticationMethod(AuthMethod::kUsernameAndPassword);
    } else if (!username_input.empty()) {
      RecordAuthenticationMethod(AuthMethod::kUsernameOnly);
    } else {
      RecordAuthenticationMethod(AuthMethod::kNoCredentials);
    }

    // Use provided credentials and parse the username into username and
    // workgroup if necessary.
    username = username_input;
    password = password_input;
    if (!ParseUserName(username_input, &username, &workgroup)) {
      std::move(callback).Run(SmbMountResult::kInvalidUsername);
      return;
    }
  }

  std::vector<uint8_t> salt;
  if (save_credentials && !password.empty()) {
    // Only generate a salt if there's a password and we've been asked to save
    // credentials. If there is no password, there's nothing for smbfs to store
    // and the salt is unused.
    salt = crypto::RandBytesAsVector(kSaltLength);
  }
  SmbShareInfo info(parsed_url, display_name, username, workgroup, use_kerberos,
                    salt);
  MountInternal(info, password, save_credentials, false /* skip_connect */,
                base::BindOnce(&SmbService::OnUserInitiatedMountDone,
                               base::Unretained(this), std::move(callback),
                               info, should_open_file_manager_after_mount));

  profile_->GetPrefs()->SetString(prefs::kMostRecentlyUsedNetworkFileShareURL,
                                  share_path.value());
}

void SmbService::OnUserInitiatedMountDone(
    MountResponse callback,
    const SmbShareInfo& info,
    bool should_open_file_manager_after_mount,
    SmbMountResult result,
    const base::FilePath& mount_path) {
  if (result != SmbMountResult::kSuccess) {
    std::move(callback).Run(result);
    return;
  }

  DCHECK(!mount_path.empty());
  if (should_open_file_manager_after_mount) {
    platform_util::ShowItemInFolder(profile_, mount_path);
  }

  registry_.Save(info);

  RecordMountCount();
  std::move(callback).Run(SmbMountResult::kSuccess);
}

void SmbService::MountInternal(
    const SmbShareInfo& info,
    const std::string& password,
    bool save_credentials,
    bool skip_connect,
    MountInternalCallback callback) {
  // Preconfigured or persisted share information could be invalid.
  if (!info.share_url().IsValid() || info.share_url().GetShare().empty()) {
    std::move(callback).Run(SmbMountResult::kInvalidUrl, {});
    return;
  }

  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile_);
  DCHECK(user);

  SmbFsShare::MountOptions smbfs_options;
  smbfs_options.resolved_host =
      share_finder_->GetResolvedHost(info.share_url().GetHost());
  smbfs_options.username = info.username();
  smbfs_options.workgroup = info.workgroup();
  smbfs_options.password = password;
  smbfs_options.allow_ntlm = IsNTLMAuthenticationEnabled();
  smbfs_options.skip_connect = skip_connect;
  smbfs_options.enable_verbose_logging = profile_->GetPrefs()->GetBoolean(
      file_manager::prefs::kSmbfsEnableVerboseLogging);
  if (save_credentials && !info.password_salt().empty()) {
    smbfs_options.save_restore_password = true;
    smbfs_options.account_hash = user->username_hash();
    smbfs_options.password_salt = info.password_salt();
  }
  if (info.use_kerberos()) {
    if (kerberos_credentials_updater_) {
      smbfs_options.kerberos_options =
          std::make_optional<SmbFsShare::KerberosOptions>(
              SmbFsShare::KerberosOptions::Source::kKerberos,
              kerberos_credentials_updater_->active_account_name());
    } else {
      LOG(WARNING) << "No Kerberos credential source available";
      std::move(callback).Run(SmbMountResult::kAuthenticationFailed, {});
      return;
    }
  }

  std::unique_ptr<SmbFsShare> mount = std::make_unique<SmbFsShare>(
      profile_, info.share_url(), info.display_name(), smbfs_options);
  if (smbfs_mounter_creation_callback_) {
    mount->SetMounterCreationCallbackForTest(smbfs_mounter_creation_callback_);
  }

  SmbFsShare* raw_mount = mount.get();
  const std::string mount_id = mount->mount_id();
  smbfs_shares_[mount_id] = std::move(mount);
  raw_mount->Mount(base::BindOnce(&SmbService::OnSmbfsMountDone,
                                  weak_ptr_factory_.GetWeakPtr(), mount_id,
                                  std::move(callback)));
}

void SmbService::OnSmbfsMountDone(const std::string& smbfs_mount_id,
                                  MountInternalCallback callback,
                                  SmbMountResult result) {
  RecordMountResult(result);

  if (result != SmbMountResult::kSuccess) {
    smbfs_shares_.erase(smbfs_mount_id);
    std::move(callback).Run(result, {});
    return;
  }

  SmbFsShare* mount = smbfs_shares_[smbfs_mount_id].get();
  if (!mount) {
    LOG(ERROR) << "smbfs mount " << smbfs_mount_id << " does not exist";
    std::move(callback).Run(SmbMountResult::kUnknownFailure, {});
    return;
  }

  std::move(callback).Run(SmbMountResult::kSuccess, mount->mount_path());
}

file_system_provider::Service* SmbService::GetProviderService() const {
  return file_system_provider::Service::Get(profile_);
}

SmbProviderClient* SmbService::GetSmbProviderClient() const {
  return SmbProviderClient::Get();
}

void SmbService::RestoreMounts() {
  std::vector<SmbUrl> preconfigured_shares =
      GetPreconfiguredSharePathsForPremount();

  std::vector<SmbShareInfo> saved_smbfs_shares;
  // Restore smbfs shares.
  saved_smbfs_shares = registry_.GetAll();

  if (!saved_smbfs_shares.empty() || !preconfigured_shares.empty()) {
    share_finder_->DiscoverHostsInNetwork(base::BindOnce(
        &SmbService::OnHostsDiscovered, weak_ptr_factory_.GetWeakPtr(),
        std::move(saved_smbfs_shares), std::move(preconfigured_shares)));
  }
}

void SmbService::OnHostsDiscovered(
    const std::vector<SmbShareInfo>& saved_smbfs_shares,
    const std::vector<SmbUrl>& preconfigured_shares) {
  for (const auto& smbfs_share : saved_smbfs_shares) {
    MountSavedSmbfsShare(smbfs_share);
  }
  for (const auto& url : preconfigured_shares) {
    MountPreconfiguredShare(url);
  }
}

void SmbService::SetRestoredShareMountDoneCallbackForTesting(
    MountInternalCallback callback) {
  restored_share_mount_done_callback_ = std::move(callback);
}

void SmbService::MountSavedSmbfsShare(const SmbShareInfo& info) {
  MountInternal(info, "" /* password */, true /* save_credentials */,
                true /* skip_connect */,
                restored_share_mount_done_callback_.is_null()
                    ? base::BindOnce(&SmbService::OnMountSavedSmbfsShareDone,
                                     weak_ptr_factory_.GetWeakPtr())
                    : std::move(restored_share_mount_done_callback_));
}

void SmbService::OnMountSavedSmbfsShareDone(SmbMountResult result,
                                            const base::FilePath& mount_path) {
  LOG_IF(ERROR, result != SmbMountResult::kSuccess)
      << "Error restoring saved share: " << static_cast<int>(result);
}

void SmbService::MountPreconfiguredShare(const SmbUrl& share_url) {
  std::string display_name =
      base::FilePath(share_url.ToString()).BaseName().value();
  // Note: Preconfigured shares are mounted without credentials.
  SmbShareInfo info(share_url, display_name, "" /* username */,
                    "" /* workgroup */, false /* use_kerberos */);
  MountInternal(info, "" /* password */, false /* save_credentials */,
                true /* skip_connect */,
                restored_share_mount_done_callback_.is_null()
                    ? base::BindOnce(&SmbService::OnMountPreconfiguredShareDone,
                                     weak_ptr_factory_.GetWeakPtr())
                    : std::move(restored_share_mount_done_callback_));
}

void SmbService::OnMountPreconfiguredShareDone(
    SmbMountResult result,
    const base::FilePath& mount_path) {
  LOG_IF(ERROR, result != SmbMountResult::kSuccess)
      << "Error mounting preconfigured share: " << static_cast<int>(result);
}

bool SmbService::IsKerberosEnabledViaPolicy() const {
  return kerberos_credentials_updater_ &&
         kerberos_credentials_updater_->IsKerberosEnabled();
}

void SmbService::SetupKerberos(const std::string& account_identifier) {
  SmbProviderClient* client = GetSmbProviderClient();
  if (!client) {
    return;
  }

  client->SetupKerberos(account_identifier,
                        base::BindOnce(&SmbService::OnSetupKerberosResponse,
                                       weak_ptr_factory_.GetWeakPtr()));
}

void SmbService::UpdateKerberosCredentials(
    const std::string& account_identifier) {
  SmbProviderClient* client = GetSmbProviderClient();
  if (!client) {
    return;
  }

  client->SetupKerberos(
      account_identifier,
      base::BindOnce(&SmbService::OnUpdateKerberosCredentialsResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SmbService::OnUpdateKerberosCredentialsResponse(bool success) {
  LOG_IF(ERROR, !success) << "Update Kerberos credentials failed.";
}

void SmbService::OnSetupKerberosResponse(bool success) {
  if (!success) {
    LOG(ERROR) << "SmbService: Kerberos setup failed.";
  }

  CompleteSetup();
}

void SmbService::CompleteSetup() {
  share_finder_ = std::make_unique<SmbShareFinder>(GetSmbProviderClient());
  RegisterHostLocators();

  GetProviderService()->RegisterProvider(std::make_unique<SmbProvider>());
  RestoreMounts();
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);

  if (setup_complete_callback_) {
    std::move(setup_complete_callback_).Run();
  }
}

void SmbService::OnSetupCompleteForTesting(base::OnceClosure callback) {
  DCHECK(!setup_complete_callback_);
  if (share_finder_) {
    std::move(callback).Run();
    return;
  }
  setup_complete_callback_ = std::move(callback);
}

void SmbService::SetSmbFsMounterCreationCallbackForTesting(
    SmbFsShare::MounterCreationCallback callback) {
  smbfs_mounter_creation_callback_ = std::move(callback);
}

void SmbService::RegisterHostLocators() {
  if (disable_share_discovery_for_testing_) {
    return;
  }

  SetUpMdnsHostLocator();
  if (IsNetBiosDiscoveryEnabled()) {
    SetUpNetBiosHostLocator();
  } else {
    LOG(WARNING) << "SmbService: NetBios discovery disabled.";
  }
}

void SmbService::SetUpMdnsHostLocator() {
  share_finder_->RegisterHostLocator(std::make_unique<MDnsHostLocator>());
}

void SmbService::SetUpNetBiosHostLocator() {
  auto get_interfaces = base::BindRepeating(&GetInterfaces);
  auto client_factory = base::BindRepeating(&GetNetBiosClient, profile_);

  auto netbios_host_locator = std::make_unique<NetBiosHostLocator>(
      std::move(get_interfaces), std::move(client_factory),
      GetSmbProviderClient());

  share_finder_->RegisterHostLocator(std::move(netbios_host_locator));
}

bool SmbService::IsNetBiosDiscoveryEnabled() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kNetBiosShareDiscoveryEnabled);
}

bool SmbService::IsNTLMAuthenticationEnabled() const {
  return profile_->GetPrefs()->GetBoolean(
      prefs::kNTLMShareAuthenticationEnabled);
}

bool SmbService::IsShareMounted(const SmbUrl& share) const {
  std::vector<file_system_provider::ProvidedFileSystemInfo> file_systems =
      GetProviderService()->GetProvidedFileSystemInfoList(provider_id_);

  for (const auto& info : file_systems) {
    base::FilePath share_path =
        GetSharePathFromFileSystemId(info.file_system_id());
    SmbUrl parsed_url(share_path.value());
    DCHECK(parsed_url.IsValid());
    if (parsed_url.ToString() == share.ToString()) {
      return true;
    }
  }

  for (const auto& entry : smbfs_shares_) {
    if (entry.second->share_url().ToString() == share.ToString()) {
      return true;
    }
  }
  return false;
}

std::vector<SmbUrl> SmbService::GetPreconfiguredSharePaths(
    const std::string& policy_mode) const {
  std::vector<SmbUrl> preconfigured_urls;

  const base::Value::List& preconfigured_shares = profile_->GetPrefs()->GetList(
      prefs::kNetworkFileSharesPreconfiguredShares);

  for (const base::Value& info_val : preconfigured_shares) {
    // |info| is a dictionary with entries for `share_url` and `mode`.
    const base::Value::Dict& info = info_val.GetDict();
    const std::string* share_url_ptr = info.FindString(kShareUrlKey);
    const std::string* mode_ptr = info.FindString(kModeKey);

    const std::string& mode = CHECK_DEREF(mode_ptr);
    const std::string& share_url = CHECK_DEREF(share_url_ptr);
    if (policy_mode == kModeUnknownValue) {
      // kModeUnknownValue is used to filter for any shares that do not match
      // a presently known mode for preconfiguration. As new preconfigure
      // modes are added, this should be kept in sync.
      if (mode != kModeDropDownValue && mode != kModePreMountValue) {
        preconfigured_urls.emplace_back(share_url);
      }
    } else {
      // Filter normally.
      if (mode == policy_mode) {
        preconfigured_urls.emplace_back(share_url);
      }
    }
  }
  return preconfigured_urls;
}

std::vector<SmbUrl> SmbService::GetPreconfiguredSharePathsForDropdown() const {
  auto drop_down_paths = GetPreconfiguredSharePaths(kModeDropDownValue);
  auto fallback_paths = GetPreconfiguredSharePaths(kModeUnknownValue);

  for (auto&& fallback_path : fallback_paths) {
    drop_down_paths.push_back(std::move(fallback_path));
  }

  return drop_down_paths;
}

std::vector<SmbUrl> SmbService::GetPreconfiguredSharePathsForPremount() const {
  return GetPreconfiguredSharePaths(kModePreMountValue);
}

void SmbService::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  // Run host discovery to refresh list of cached hosts for subsequent name
  // resolution attempts.
  share_finder_->DiscoverHostsInNetwork(base::DoNothing()
                                        /* HostDiscoveryResponse */);
}

void SmbService::RecordMountCount() const {
  const std::vector<file_system_provider::ProvidedFileSystemInfo> file_systems =
      GetProviderService()->GetProvidedFileSystemInfoList(provider_id_);
  UMA_HISTOGRAM_COUNTS_100("NativeSmbFileShare.MountCount",
                           file_systems.size() + smbfs_shares_.size());
}

bool SmbService::IsAnySmbShareConfigured() {
  std::vector<SmbUrl> preconfigured_shares =
      GetPreconfiguredSharePathsForPremount();

  std::vector<SmbShareInfo> saved_smbfs_shares;
  saved_smbfs_shares = registry_.GetAll();

  return !saved_smbfs_shares.empty() || !preconfigured_shares.empty();
}

}  // namespace ash::smb_client

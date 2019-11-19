// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_service.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/smb_client/discovery/mdns_host_locator.h"
#include "chrome/browser/chromeos/smb_client/discovery/netbios_client.h"
#include "chrome/browser/chromeos/smb_client/discovery/netbios_host_locator.h"
#include "chrome/browser/chromeos/smb_client/smb_file_system.h"
#include "chrome/browser/chromeos/smb_client/smb_file_system_id.h"
#include "chrome/browser/chromeos/smb_client/smb_provider.h"
#include "chrome/browser/chromeos/smb_client/smb_service_factory.h"
#include "chrome/browser/chromeos/smb_client/smb_service_helper.h"
#include "chrome/browser/chromeos/smb_client/smb_url.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/webui/chromeos/smb_shares/smb_credentials_dialog.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/smb_provider_client.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/network_interfaces.h"
#include "url/url_util.h"

using chromeos::file_system_provider::Service;

namespace chromeos {
namespace smb_client {

namespace {

const char kShareUrlKey[] = "share_url";
const char kModeKey[] = "mode";
const char kModeDropDownValue[] = "drop_down";
const char kModePreMountValue[] = "pre_mount";
const char kModeUnknownValue[] = "unknown";
const base::TimeDelta kHostDiscoveryInterval = base::TimeDelta::FromSeconds(60);
// -3 is chosen because -1 and -2 have special meaning in smbprovider.
const int32_t kInvalidMountId = -3;

net::NetworkInterfaceList GetInterfaces() {
  net::NetworkInterfaceList list;
  if (!net::GetNetworkList(&list, net::EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES)) {
    LOG(ERROR) << "GetInterfaces failed";
  }
  return list;
}

std::unique_ptr<NetBiosClientInterface> GetNetBiosClient(Profile* profile) {
  auto* network_context =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetNetworkContext();
  return std::make_unique<NetBiosClient>(network_context);
}

bool IsEnabledByFlag() {
  return base::FeatureList::IsEnabled(features::kNativeSmb);
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
  kSSOKerberos = 3,
  kMaxValue = kSSOKerberos,
};

void RecordMountResult(SmbMountResult result) {
  DCHECK_LE(result, SmbMountResult::kMaxValue);
  UMA_HISTOGRAM_ENUMERATION("NativeSmbFileShare.MountResult", result);
}

void RecordRemountResult(SmbMountResult result) {
  DCHECK_LE(result, SmbMountResult::kMaxValue);
  UMA_HISTOGRAM_ENUMERATION("NativeSmbFileShare.RemountResult", result);
}

void RecordAuthenticationMethod(AuthMethod method) {
  DCHECK_LE(method, AuthMethod::kMaxValue);
  UMA_HISTOGRAM_ENUMERATION("NativeSmbFileShare.AuthenticationMethod", method);
}

std::unique_ptr<TempFileManager> CreateTempFileManager() {
  return std::make_unique<TempFileManager>();
}

}  // namespace

bool SmbService::service_should_run_ = false;
bool SmbService::disable_share_discovery_for_testing_ = false;

SmbService::SmbService(Profile* profile,
                       std::unique_ptr<base::TickClock> tick_clock)
    : provider_id_(ProviderId::CreateFromNativeId("smb")),
      profile_(profile),
      tick_clock_(std::move(tick_clock)) {
  service_should_run_ = IsEnabledByFlag() && IsAllowedByPolicy();
  if (service_should_run_) {
    StartSetup();
  }
}

SmbService::~SmbService() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

// static
SmbService* SmbService::Get(content::BrowserContext* context) {
  if (service_should_run_) {
    return SmbServiceFactory::Get(context);
  }
  return nullptr;
}

// static
void SmbService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kNetworkFileSharesAllowed, true);
  registry->RegisterBooleanPref(prefs::kNetBiosShareDiscoveryEnabled, true);
  registry->RegisterBooleanPref(prefs::kNTLMShareAuthenticationEnabled, true);
  registry->RegisterListPref(prefs::kNetworkFileSharesPreconfiguredShares);
  registry->RegisterStringPref(prefs::kMostRecentlyUsedNetworkFileShareURL, "");
}

void SmbService::Mount(const file_system_provider::MountOptions& options,
                       const base::FilePath& share_path,
                       const std::string& username,
                       const std::string& password,
                       bool use_chromad_kerberos,
                       bool should_open_file_manager_after_mount,
                       bool save_credentials,
                       MountResponse callback) {
  DCHECK(temp_file_manager_);

  CallMount(options, share_path, username, password, use_chromad_kerberos,
            should_open_file_manager_after_mount, save_credentials,
            std::move(callback));
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

void SmbService::UpdateCredentials(int32_t mount_id,
                                   const std::string& username,
                                   const std::string& password) {
  DCHECK(temp_file_manager_);

  std::string parsed_username = username;
  std::string workgroup;
  ParseUserName(username, &parsed_username, &workgroup);

  GetSmbProviderClient()->UpdateMountCredentials(
      mount_id, workgroup, parsed_username,
      temp_file_manager_->WritePasswordToFile(password),
      base::BindOnce(&SmbService::OnUpdateCredentialsResponse, AsWeakPtr(),
                     mount_id));
}

void SmbService::OnUpdateCredentialsResponse(int32_t mount_id,
                                             smbprovider::ErrorType error) {
  auto creds_reply_iter = update_credential_replies_.find(mount_id);
  DCHECK(creds_reply_iter != update_credential_replies_.end());

  if (error == smbprovider::ERROR_OK) {
    std::move(creds_reply_iter->second).Run();
  } else {
    LOG(ERROR) << "Failed to update the credentials for mount id " << mount_id;
  }

  update_credential_replies_.erase(creds_reply_iter);
}

void SmbService::UpdateSharePath(int32_t mount_id,
                                 const std::string& share_path,
                                 StartReadDirIfSuccessfulCallback reply) {
  GetSmbProviderClient()->UpdateSharePath(
      mount_id, share_path,
      base::BindOnce(&SmbService::OnUpdateSharePathResponse, AsWeakPtr(),
                     mount_id, std::move(reply)));
}

void SmbService::OnUpdateSharePathResponse(
    int32_t mount_id,
    StartReadDirIfSuccessfulCallback reply,
    smbprovider::ErrorType error) {
  if (error != smbprovider::ERROR_OK) {
    LOG(ERROR) << "Failed to update the share path for mount id " << mount_id;
    std::move(reply).Run(false /* should_retry_start_read_dir */);
    return;
  }
  std::move(reply).Run(true /* should_retry_start_read_dir */);
}

void SmbService::CallMount(const file_system_provider::MountOptions& options,
                           const base::FilePath& share_path,
                           const std::string& username_input,
                           const std::string& password_input,
                           bool use_chromad_kerberos,
                           bool should_open_file_manager_after_mount,
                           bool save_credentials,
                           MountResponse callback) {
  SmbUrl parsed_url(share_path.value());
  if (!parsed_url.IsValid() || parsed_url.GetShare().empty()) {
    // Handle invalid URLs early to avoid having unaccounted for UMA counts for
    // authentication method.
    std::move(callback).Run(
        TranslateErrorToMountResult(base::File::Error::FILE_ERROR_INVALID_URL));
    return;
  }

  // When using kerberos, the URL must contain the hostname because that is used
  // to obtain the ticket. If the user enters an IP address, Samba will give us
  // a permission error, which isn't correct or useful to the end user.
  if (use_chromad_kerberos && url::HostIsIPAddress(parsed_url.GetHost())) {
    std::move(callback).Run(SmbMountResult::INVALID_SSO_URL);
    return;
  }

  if (IsShareMounted(parsed_url)) {
    // Prevent a share from being mounted twice. Although technically possible,
    // the UX when doing so is incomplete.
    std::move(callback).Run(SmbMountResult::MOUNT_EXISTS);
    return;
  }

  std::string username;
  std::string password;
  std::string workgroup;

  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  DCHECK(user);
  if (use_chromad_kerberos) {
    RecordAuthenticationMethod(AuthMethod::kSSOKerberos);
    // Get the user's username and workgroup from their email address to be used
    // for Kerberos authentication.
    DCHECK(user->IsActiveDirectoryUser());
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
    ParseUserName(username_input, &username, &workgroup);
  }

  // If using kerberos, the hostname should not be resolved since kerberos
  // service tickets are keyed on hosname.
  const std::string url = use_chromad_kerberos
                              ? parsed_url.ToString()
                              : share_finder_->GetResolvedUrl(parsed_url);
  const base::FilePath mount_path(url);

  SmbProviderClient::MountOptions smb_mount_options;
  smb_mount_options.original_path = parsed_url.ToString();
  smb_mount_options.username = username;
  smb_mount_options.workgroup = workgroup;
  smb_mount_options.ntlm_enabled = IsNTLMAuthenticationEnabled();
  smb_mount_options.save_password = save_credentials && !use_chromad_kerberos;
  smb_mount_options.account_hash = user->username_hash();
  GetSmbProviderClient()->Mount(
      mount_path, smb_mount_options,
      temp_file_manager_->WritePasswordToFile(password),
      base::BindOnce(&SmbService::OnMountResponse, AsWeakPtr(),
                     base::Passed(&callback), options, share_path,
                     use_chromad_kerberos, should_open_file_manager_after_mount,
                     username, workgroup, save_credentials));

  profile_->GetPrefs()->SetString(prefs::kMostRecentlyUsedNetworkFileShareURL,
                                  share_path.value());
}

void SmbService::OnMountResponse(
    MountResponse callback,
    const file_system_provider::MountOptions& options,
    const base::FilePath& share_path,
    bool is_kerberos_chromad,
    bool should_open_file_manager_after_mount,
    const std::string& username,
    const std::string& workgroup,
    bool save_credentials,
    smbprovider::ErrorType error,
    int32_t mount_id) {
  if (error != smbprovider::ERROR_OK) {
    FireMountCallback(std::move(callback), TranslateErrorToMountResult(error));
    return;
  }

  DCHECK_GE(mount_id, 0);

  file_system_provider::MountOptions mount_options(options);
  if (is_kerberos_chromad) {
    mount_options.file_system_id =
        CreateFileSystemId(share_path, is_kerberos_chromad);
  } else {
    std::string full_username;
    if (save_credentials) {
      // Only save the username if the user request credentials be saved.
      full_username = username;
      if (!workgroup.empty()) {
        DCHECK(!username.empty());
        full_username.append("@");
        full_username.append(workgroup);
      }
    }
    mount_options.file_system_id =
        CreateFileSystemIdForUser(share_path, full_username);
  }
  mount_id_map_[mount_options.file_system_id] = mount_id;

  base::File::Error result =
      GetProviderService()->MountFileSystem(provider_id_, mount_options);

  if (result == base::File::FILE_OK && should_open_file_manager_after_mount) {
    OpenFileManager(mount_options.file_system_id);
  }

  FireMountCallback(std::move(callback), TranslateErrorToMountResult(result));

  // Record Mount count after callback to include the new mount.
  RecordMountCount();
}

int32_t SmbService::GetMountId(const ProvidedFileSystemInfo& info) const {
  const auto iter = mount_id_map_.find(info.file_system_id());
  if (iter == mount_id_map_.end()) {
    // Either the mount process has not yet completed, or it failed to provide
    // us with a mount id.
    return kInvalidMountId;
  }
  return iter->second;
}

base::File::Error SmbService::Unmount(
    const std::string& file_system_id,
    file_system_provider::Service::UnmountReason reason) {
  base::File::Error result = GetProviderService()->UnmountFileSystem(
      provider_id_, file_system_id, reason);
  // Always erase the mount_id, because at this point, the share has already
  // been unmounted in smbprovider.
  mount_id_map_.erase(file_system_id);
  return result;
}

Service* SmbService::GetProviderService() const {
  return file_system_provider::Service::Get(profile_);
}

SmbProviderClient* SmbService::GetSmbProviderClient() const {
  // If the DBusThreadManager or the SmbProviderClient aren't available,
  // there isn't much we can do. This should only happen when running tests.
  if (!chromeos::DBusThreadManager::IsInitialized() ||
      !chromeos::DBusThreadManager::Get()) {
    return nullptr;
  }
  return chromeos::DBusThreadManager::Get()->GetSmbProviderClient();
}

void SmbService::RestoreMounts() {
  std::vector<ProvidedFileSystemInfo> file_systems =
      GetProviderService()->GetProvidedFileSystemInfoList(provider_id_);

  std::vector<SmbUrl> preconfigured_shares =
      GetPreconfiguredSharePathsForPremount();

  if (!file_systems.empty() || !preconfigured_shares.empty()) {
    share_finder_->DiscoverHostsInNetwork(base::BindOnce(
        &SmbService::OnHostsDiscovered, AsWeakPtr(), std::move(file_systems),
        std::move(preconfigured_shares)));
  }
}

void SmbService::OnHostsDiscovered(
    const std::vector<ProvidedFileSystemInfo>& file_systems,
    const std::vector<SmbUrl>& preconfigured_shares) {
  for (const auto& file_system : file_systems) {
    Remount(file_system);
  }
  for (const auto& url : preconfigured_shares) {
    const base::FilePath share_path(share_finder_->GetResolvedUrl(url));
    Premount(share_path);
  }
}

void SmbService::OnHostsDiscoveredForUpdateSharePath(
    int32_t mount_id,
    const std::string& share_path,
    StartReadDirIfSuccessfulCallback reply) {
  std::string resolved_url;
  if (share_finder_->TryResolveUrl(SmbUrl(share_path), &resolved_url)) {
    UpdateSharePath(mount_id, resolved_url, std::move(reply));
  } else {
    std::move(reply).Run(false /* should_retry_start_read_dir */);
  }
}

void SmbService::Remount(const ProvidedFileSystemInfo& file_system_info) {
  const base::FilePath share_path =
      GetSharePathFromFileSystemId(file_system_info.file_system_id());
  const bool is_kerberos_chromad =
      IsKerberosChromadFileSystemId(file_system_info.file_system_id());

  std::string workgroup;
  std::string username;

  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  DCHECK(user);
  if (is_kerberos_chromad) {
    DCHECK(user->IsActiveDirectoryUser());

    ParseUserPrincipalName(user->GetDisplayEmail(), &username, &workgroup);
  } else {
    base::Optional<std::string> user_workgroup =
        GetUserFromFileSystemId(file_system_info.file_system_id());
    if (user_workgroup &&
        !ParseUserName(*user_workgroup, &username, &workgroup)) {
      LOG(ERROR) << "Failed to parse username/workgroup from file system ID";
    }
  }

  SmbUrl parsed_url(share_path.value());
  if (!parsed_url.IsValid()) {
    OnRemountResponse(file_system_info.file_system_id(),
                      smbprovider::ERROR_INVALID_URL, kInvalidMountId);
    return;
  }

  // If using kerberos, the hostname should not be resolved since kerberos
  // service tickets are keyed on hosname.
  const base::FilePath mount_path =
      is_kerberos_chromad
          ? base::FilePath(parsed_url.ToString())
          : base::FilePath(share_finder_->GetResolvedUrl(parsed_url));

  // An empty password is passed to Mount to conform with the credentials API
  // which expects username & workgroup strings along with a password file
  // descriptor.
  SmbProviderClient::MountOptions smb_mount_options;
  smb_mount_options.original_path = parsed_url.ToString();
  smb_mount_options.username = username;
  smb_mount_options.workgroup = workgroup;
  smb_mount_options.ntlm_enabled = IsNTLMAuthenticationEnabled();
  smb_mount_options.skip_connect = true;
  smb_mount_options.restore_password =
      !username.empty() && !is_kerberos_chromad;
  smb_mount_options.account_hash = user->username_hash();
  GetSmbProviderClient()->Mount(
      mount_path, smb_mount_options,
      temp_file_manager_->WritePasswordToFile("" /* password */),
      base::BindOnce(&SmbService::OnRemountResponse, AsWeakPtr(),
                     file_system_info.file_system_id()));
}

void SmbService::OnRemountResponse(const std::string& file_system_id,
                                   smbprovider::ErrorType error,
                                   int32_t mount_id) {
  RecordRemountResult(TranslateErrorToMountResult(error));

  if (error != smbprovider::ERROR_OK) {
    LOG(ERROR) << "SmbService: failed to restore filesystem with error: "
               << error;
    // Note: The filesystem isn't removed on failure because doing so will
    // stop persisting the mount. The mount should only be removed as a result
    // of user action, and not due to failures, which might be transient (i.e.
    // smbprovider crashed).
    return;
  }

  DCHECK_GE(mount_id, 0);
  mount_id_map_[file_system_id] = mount_id;
}

void SmbService::Premount(const base::FilePath& share_path) {
  // Premounting is equivalent to remounting, but with an empty username and
  // password.
  SmbProviderClient::MountOptions smb_mount_options;
  smb_mount_options.ntlm_enabled = IsNTLMAuthenticationEnabled();
  smb_mount_options.skip_connect = true;
  GetSmbProviderClient()->Mount(
      share_path, smb_mount_options,
      temp_file_manager_->WritePasswordToFile("" /* password */),
      base::BindOnce(&SmbService::OnPremountResponse, AsWeakPtr(), share_path));
}

void SmbService::OnPremountResponse(const base::FilePath& share_path,
                                    smbprovider::ErrorType error,
                                    int32_t mount_id) {
  DCHECK_GE(mount_id, 0);

  file_system_provider::MountOptions mount_options;
  mount_options.display_name = share_path.BaseName().value();
  mount_options.writable = true;
  // |is_chromad_kerberos| is false because we do not pass user and workgroup
  // at mount time. Premounts also do not get remounted and currently
  // |is_chromad_kerberos| is only used at remounts to determine if the share
  // was mounted with chromad kerberos.
  // TODO(jimmyxgong): Support chromad kerberos for premount.
  mount_options.file_system_id =
      CreateFileSystemId(share_path, false /* is_chromad_kerberos */);
  // Disable remounting of preconfigured shares.
  mount_options.persistent = false;
  mount_id_map_[mount_options.file_system_id] = mount_id;

  const base::File::Error result =
      GetProviderService()->MountFileSystem(provider_id_, mount_options);

  if (result != base::File::FILE_OK) {
    LOG(ERROR) << "Error mounting preconfigured share with File Manager.";
  }
}

void SmbService::StartSetup() {
  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);

  if (!user) {
    // An instance of SmbService is created on the lockscreen. When this
    // instance is created, no setup will run.
    return;
  }

  SmbProviderClient* client = GetSmbProviderClient();
  if (!client) {
    return;
  }

  if (user->IsActiveDirectoryUser()) {
    auto account_id = user->GetAccountId();
    const std::string account_id_guid = account_id.GetObjGuid();

    GetSmbProviderClient()->SetupKerberos(
        account_id_guid,
        base::BindOnce(&SmbService::OnSetupKerberosResponse, AsWeakPtr()));
    return;
  }

  SetupTempFileManagerAndCompleteSetup();
}

void SmbService::SetupTempFileManagerAndCompleteSetup() {
  // CreateTempFileManager() has to be called on a separate thread since it
  // contains a call that requires a blockable thread.
  base::TaskTraits task_traits = {base::ThreadPool(), base::MayBlock(),
                                  base::TaskPriority::USER_BLOCKING,
                                  base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};
  auto task = base::BindOnce(&CreateTempFileManager);
  auto reply = base::BindOnce(&SmbService::CompleteSetup, AsWeakPtr());

  base::PostTaskAndReplyWithResult(FROM_HERE, task_traits, std::move(task),
                                   std::move(reply));
}

void SmbService::OnSetupKerberosResponse(bool success) {
  if (!success) {
    LOG(ERROR) << "SmbService: Kerberos setup failed.";
  }

  SetupTempFileManagerAndCompleteSetup();
}

void SmbService::CompleteSetup(
    std::unique_ptr<TempFileManager> temp_file_manager) {
  DCHECK(temp_file_manager);
  DCHECK(!temp_file_manager_);

  temp_file_manager_ = std::move(temp_file_manager);
  share_finder_ = std::make_unique<SmbShareFinder>(GetSmbProviderClient());
  RegisterHostLocators();

  GetProviderService()->RegisterProvider(std::make_unique<SmbProvider>(
      base::BindRepeating(&SmbService::GetMountId, base::Unretained(this)),
      base::BindRepeating(&SmbService::Unmount, base::Unretained(this)),
      base::BindRepeating(&SmbService::RequestCredentials,
                          base::Unretained(this)),
      base::BindRepeating(&SmbService::RequestUpdatedSharePath,
                          base::Unretained(this))));
  RestoreMounts();
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);

  if (setup_complete_callback_) {
    std::move(setup_complete_callback_).Run();
  }
}

void SmbService::OnSetupCompleteForTesting(base::OnceClosure callback) {
  DCHECK(!setup_complete_callback_);
  if (temp_file_manager_) {
    std::move(callback).Run();
    return;
  }
  setup_complete_callback_ = std::move(callback);
}

void SmbService::FireMountCallback(MountResponse callback,
                                   SmbMountResult result) {
  RecordMountResult(result);

  std::move(callback).Run(result);
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

void SmbService::OpenFileManager(const std::string& file_system_id) {
  base::FilePath mount_path = file_system_provider::util::GetMountPath(
      profile_, provider_id_, file_system_id);

  platform_util::ShowItemInFolder(profile_, mount_path);
}

bool SmbService::IsAllowedByPolicy() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kNetworkFileSharesAllowed);
}

bool SmbService::IsNetBiosDiscoveryEnabled() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kNetBiosShareDiscoveryEnabled);
}

bool SmbService::IsNTLMAuthenticationEnabled() const {
  return profile_->GetPrefs()->GetBoolean(
      prefs::kNTLMShareAuthenticationEnabled);
}

bool SmbService::IsShareMounted(const SmbUrl& share) const {
  std::vector<ProvidedFileSystemInfo> file_systems =
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
  return false;
}

std::vector<SmbUrl> SmbService::GetPreconfiguredSharePaths(
    const std::string& policy_mode) const {
  std::vector<SmbUrl> preconfigured_urls;

  const base::Value* preconfigured_shares = profile_->GetPrefs()->GetList(
      prefs::kNetworkFileSharesPreconfiguredShares);

  for (const base::Value& info : preconfigured_shares->GetList()) {
    // |info| is a dictionary with entries for |share_url| and |mode|.
    const base::Value* share_url = info.FindKey(kShareUrlKey);
    const base::Value* mode = info.FindKey(kModeKey);

    if (policy_mode == kModeUnknownValue) {
      // kModeUnknownValue is used to filter for any shares that do not match
      // a presently known mode for preconfiguration. As new preconfigure
      // modes are added, this should be kept in sync.
      if (mode->GetString() != kModeDropDownValue &&
          mode->GetString() != kModePreMountValue) {
        preconfigured_urls.emplace_back(share_url->GetString());
      }

    } else {
      // Filter normally
      if (mode->GetString() == policy_mode) {
        preconfigured_urls.emplace_back(share_url->GetString());
      }
    }
  }
  return preconfigured_urls;
}

void SmbService::RequestCredentials(const std::string& share_path,
                                    int32_t mount_id,
                                    base::OnceClosure reply) {
  update_credential_replies_[mount_id] = std::move(reply);
  OpenRequestCredentialsDialog(share_path, mount_id);
}

void SmbService::OpenRequestCredentialsDialog(const std::string& share_path,
                                              int32_t mount_id) {
  smb_dialog::SmbCredentialsDialog::Show(mount_id, share_path);
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

void SmbService::RequestUpdatedSharePath(
    const std::string& share_path,
    int32_t mount_id,
    StartReadDirIfSuccessfulCallback reply) {
  if (ShouldRunHostDiscoveryAgain()) {
    previous_host_discovery_time_ = tick_clock_->NowTicks();
    share_finder_->DiscoverHostsInNetwork(
        base::BindOnce(&SmbService::OnHostsDiscoveredForUpdateSharePath,
                       AsWeakPtr(), mount_id, share_path, std::move(reply)));
    return;
  }
  // Host discovery did not run, but try to resolve the hostname in case a
  // previous host discovery found the host.
  std::string resolved_url;
  if (share_finder_->TryResolveUrl(SmbUrl(share_path), &resolved_url)) {
    UpdateSharePath(mount_id, share_path, std::move(reply));
  } else {
    std::move(reply).Run(false /* should_retry_start_read_dir */);
  }
}

bool SmbService::ShouldRunHostDiscoveryAgain() const {
  return tick_clock_->NowTicks() >
         previous_host_discovery_time_ + kHostDiscoveryInterval;
}

void SmbService::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);

  if (!user) {
    // If a network change occurs on the lockscreen, do nothing.
    return;
  }

  // Run host discovery to refresh list of cached hosts for subsequent name
  // resolution attempts.
  share_finder_->DiscoverHostsInNetwork(base::DoNothing()
                                        /* HostDiscoveryResponse */);
}

void SmbService::RecordMountCount() const {
  const std::vector<ProvidedFileSystemInfo> file_systems =
      GetProviderService()->GetProvidedFileSystemInfoList(provider_id_);
  UMA_HISTOGRAM_COUNTS_100("NativeSmbFileShare.MountCount",
                           file_systems.size());
}

}  // namespace smb_client
}  // namespace chromeos

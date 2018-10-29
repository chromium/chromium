// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_service.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
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
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/smb_provider_client.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/network_interfaces.h"

using chromeos::file_system_provider::Service;

namespace chromeos {
namespace smb_client {

namespace {

const char kShareUrlKey[] = "share_url";
const char kModeKey[] = "mode";
const char kModeDropDownValue[] = "drop_down";

bool ContainsAt(const std::string& username) {
  return username.find('@') != std::string::npos;
}

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

SmbService::SmbService(Profile* profile)
    : provider_id_(ProviderId::CreateFromNativeId("smb")), profile_(profile) {
  service_should_run_ = IsEnabledByFlag() && IsAllowedByPolicy();
  if (service_should_run_) {
    StartSetup();
  }
}

SmbService::~SmbService() {}

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
}

void SmbService::Mount(const file_system_provider::MountOptions& options,
                       const base::FilePath& share_path,
                       const std::string& username,
                       const std::string& password,
                       bool use_chromad_kerberos,
                       MountResponse callback) {
  DCHECK(temp_file_manager_);

  CallMount(options, share_path, username, password, use_chromad_kerberos,
            std::move(callback));
}

void SmbService::GatherSharesInNetwork(HostDiscoveryResponse discovery_callback,
                                       GatherSharesResponse shares_callback) {
  shares_callback.Run(GetPreconfiguredSharePathsForDropDown());
  share_finder_->GatherSharesInNetwork(std::move(discovery_callback),
                                       std::move(shares_callback));
}

void SmbService::CallMount(const file_system_provider::MountOptions& options,
                           const base::FilePath& share_path,
                           const std::string& username_input,
                           const std::string& password_input,
                           bool use_chromad_kerberos,
                           MountResponse callback) {
  std::string username;
  std::string password;
  std::string workgroup;

  if (use_chromad_kerberos) {
    RecordAuthenticationMethod(AuthMethod::kSSOKerberos);
    // Get the user's username and workgroup from their email address to be used
    // for Kerberos authentication.
    user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
    if (user) {
      DCHECK(user->IsActiveDirectoryUser());
      ParseUserPrincipalName(user->GetDisplayEmail(), &username, &workgroup);
    }
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
    if (ContainsAt(username)) {
      ParseUserPrincipalName(username_input, &username, &workgroup);
    }
  }

  SmbUrl parsed_url(share_path.value());
  if (!parsed_url.IsValid()) {
    std::move(callback).Run(
        TranslateErrorToMountResult(base::File::Error::FILE_ERROR_INVALID_URL));
    return;
  }

  // If using kerberos, the hostname should not be resolved since kerberos
  // service tickets are keyed on hosname.
  const base::FilePath mount_path =
      use_chromad_kerberos
          ? share_path
          : base::FilePath(share_finder_->GetResolvedUrl(parsed_url));

  GetSmbProviderClient()->Mount(
      mount_path, IsNTLMAuthenticationEnabled(), workgroup, username,
      temp_file_manager_->WritePasswordToFile(password),
      base::BindOnce(&SmbService::OnMountResponse, AsWeakPtr(),
                     base::Passed(&callback), options, share_path,
                     use_chromad_kerberos));
}

void SmbService::OnMountResponse(
    MountResponse callback,
    const file_system_provider::MountOptions& options,
    const base::FilePath& share_path,
    bool is_kerberos_chromad,
    smbprovider::ErrorType error,
    int32_t mount_id) {
  if (error != smbprovider::ERROR_OK) {
    FireMountCallback(std::move(callback), TranslateErrorToMountResult(error));
    return;
  }

  DCHECK_GE(mount_id, 0);

  file_system_provider::MountOptions mount_options(options);
  mount_options.file_system_id =
      CreateFileSystemId(mount_id, share_path, is_kerberos_chromad);

  base::File::Error result =
      GetProviderService()->MountFileSystem(provider_id_, mount_options);

  if (result == base::File::FILE_OK) {
    OpenFileManager(mount_options.file_system_id);
  }

  FireMountCallback(std::move(callback), TranslateErrorToMountResult(result));

  // Record Mount count after callback to include the new mount.
  RecordMountCount();
}

base::File::Error SmbService::Unmount(
    const std::string& file_system_id,
    file_system_provider::Service::UnmountReason reason) const {
  return GetProviderService()->UnmountFileSystem(provider_id_, file_system_id,
                                                 reason);
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
  const std::vector<ProvidedFileSystemInfo> file_systems =
      GetProviderService()->GetProvidedFileSystemInfoList(provider_id_);

  if (!file_systems.empty()) {
    share_finder_->DiscoverHostsInNetwork(base::BindOnce(
        &SmbService::OnHostsDiscovered, AsWeakPtr(), file_systems));
  }
}

void SmbService::OnHostsDiscovered(
    const std::vector<ProvidedFileSystemInfo>& file_systems) {
  for (const auto& file_system : file_systems) {
    Remount(file_system);
  }
}

void SmbService::Remount(const ProvidedFileSystemInfo& file_system_info) {
  const base::FilePath share_path =
      GetSharePathFromFileSystemId(file_system_info.file_system_id());
  const int32_t mount_id =
      GetMountIdFromFileSystemId(file_system_info.file_system_id());
  const bool is_kerberos_chromad =
      IsKerberosChromadFileSystemId(file_system_info.file_system_id());

  std::string workgroup;
  std::string username;

  if (is_kerberos_chromad) {
    user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
    DCHECK(user);
    DCHECK(user->IsActiveDirectoryUser());

    ParseUserPrincipalName(user->GetDisplayEmail(), &username, &workgroup);
  }

  SmbUrl parsed_url(share_path.value());
  if (!parsed_url.IsValid()) {
    OnRemountResponse(file_system_info.file_system_id(),
                      smbprovider::ERROR_INVALID_URL);
    return;
  }

  // If using kerberos, the hostname should not be resolved since kerberos
  // service tickets are keyed on hosname.
  const base::FilePath mount_path =
      is_kerberos_chromad
          ? share_path
          : base::FilePath(share_finder_->GetResolvedUrl(parsed_url));

  // An empty password is passed to Remount to conform with the credentials API
  // which expects username & workgroup strings along with a password file
  // descriptor.
  GetSmbProviderClient()->Remount(
      mount_path, mount_id, IsNTLMAuthenticationEnabled(), workgroup, username,
      temp_file_manager_->WritePasswordToFile("" /* password */),
      base::BindOnce(&SmbService::OnRemountResponse, AsWeakPtr(),
                     file_system_info.file_system_id()));
}

void SmbService::OnRemountResponse(const std::string& file_system_id,
                                   smbprovider::ErrorType error) {
  RecordRemountResult(TranslateErrorToMountResult(error));

  if (error != smbprovider::ERROR_OK) {
    LOG(ERROR) << "SmbService: failed to restore filesystem: ";
    Unmount(file_system_id, file_system_provider::Service::UNMOUNT_REASON_USER);
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
  base::TaskTraits task_traits = {base::MayBlock(),
                                  base::TaskPriority::USER_BLOCKING,
                                  base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};
  auto task = base::BindOnce(&CreateTempFileManager);
  auto reply = base::BindOnce(&SmbService::CompleteSetup, AsWeakPtr());

  base::PostTaskWithTraitsAndReplyWithResult(FROM_HERE, task_traits,
                                             std::move(task), std::move(reply));
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
      base::BindRepeating(&SmbService::Unmount, base::Unretained(this))));
  RestoreMounts();
}

void SmbService::FireMountCallback(MountResponse callback,
                                   SmbMountResult result) {
  RecordMountResult(result);

  std::move(callback).Run(result);
}

void SmbService::RegisterHostLocators() {
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

std::vector<SmbUrl> SmbService::GetPreconfiguredSharePathsForDropDown() const {
  std::vector<SmbUrl> preconfigured_urls;

  const base::Value* preconfigured_shares = profile_->GetPrefs()->GetList(
      prefs::kNetworkFileSharesPreconfiguredShares);

  for (const base::Value& info : preconfigured_shares->GetList()) {
    // |info| is a dictionary with entries for |share_url| and |mode|.
    const base::Value* share_url = info.FindKey(kShareUrlKey);
    const base::Value* mode = info.FindKey(kModeKey);

    DCHECK(mode->GetString() == kModeDropDownValue);

    if (mode->GetString() == kModeDropDownValue) {
      preconfigured_urls.emplace_back(share_url->GetString());
    }
  }
  return preconfigured_urls;
}

void SmbService::RecordMountCount() const {
  const std::vector<ProvidedFileSystemInfo> file_systems =
      GetProviderService()->GetProvidedFileSystemInfoList(provider_id_);
  UMA_HISTOGRAM_COUNTS_100("NativeSmbFileShare.MountCount",
                           file_systems.size());
}

}  // namespace smb_client
}  // namespace chromeos

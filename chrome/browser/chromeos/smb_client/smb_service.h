// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provider_interface.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/smb_client/smb_errors.h"
#include "chrome/browser/chromeos/smb_client/smb_share_finder.h"
#include "chrome/browser/chromeos/smb_client/smb_task_queue.h"
#include "chrome/browser/chromeos/smb_client/temp_file_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/smb_provider_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/base/network_change_notifier.h"

namespace base {
class FilePath;
}  // namespace base

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace chromeos {
namespace smb_client {

using file_system_provider::Capabilities;
using file_system_provider::ProvidedFileSystemInfo;
using file_system_provider::ProvidedFileSystemInterface;
using file_system_provider::ProviderId;
using file_system_provider::ProviderInterface;
using file_system_provider::Service;

// Creates and manages an smb file system.
class SmbService : public KeyedService,
                   public net::NetworkChangeNotifier::NetworkChangeObserver,
                   public base::SupportsWeakPtr<SmbService> {
 public:
  using MountResponse = base::OnceCallback<void(SmbMountResult result)>;
  using StartReadDirIfSuccessfulCallback =
      base::OnceCallback<void(bool should_retry_start_read_dir)>;
  using GatherSharesResponse =
      base::RepeatingCallback<void(const std::vector<SmbUrl>& shares_gathered,
                                   bool done)>;

  SmbService(Profile* profile, std::unique_ptr<base::TickClock> tick_clock);
  ~SmbService() override;

  // Gets the singleton instance for the |context|.
  static SmbService* Get(content::BrowserContext* context);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Starts the process of mounting an SMB file system.
  // |use_kerberos| indicates whether the share should be mounted with a user's
  // chromad kerberos tickets.
  // Calls SmbProviderClient::Mount().
  void Mount(const file_system_provider::MountOptions& options,
             const base::FilePath& share_path,
             const std::string& username,
             const std::string& password,
             bool use_chromad_kerberos,
             bool should_open_file_manager_after_mount,
             bool save_credentials,
             MountResponse callback);

  // Completes the mounting of an SMB file system, passing |options| on to
  // file_system_provider::Service::MountFileSystem(). Passes error status to
  // callback.
  void OnMountResponse(MountResponse callback,
                       const file_system_provider::MountOptions& options,
                       const base::FilePath& share_path,
                       bool is_kerberos_chromad,
                       bool should_open_file_manager_after_mount,
                       const std::string& username,
                       const std::string& workgroup,
                       bool save_credentials,
                       smbprovider::ErrorType error,
                       int32_t mount_id);

  // Gathers the hosts in the network using |share_finder_| and gets the shares
  // for each of the hosts found. |discovery_callback| is called as soon as host
  // discovery is complete. |shares_callback| may be called multiple times with
  // new shares. |shares_callback| will be called with |done| == false when more
  // shares are expected to be discovered. When share discovery is finished,
  // |shares_callback| is called with |done| == true and will not be called
  // again.
  void GatherSharesInNetwork(HostDiscoveryResponse discovery_callback,
                             GatherSharesResponse shares_callback);

  // Updates the credentials for |mount_id|. If there is a stored callback in
  // |update_credentials_replies_| for |mount_id|, it will be run upon once the
  // credentials are successfully updated.
  void UpdateCredentials(int32_t mount_id,
                         const std::string& username,
                         const std::string& password);

  // Updates the share path for |mount_id|.
  void UpdateSharePath(int32_t mount_id,
                       const std::string& share_path,
                       StartReadDirIfSuccessfulCallback reply);

  // Disable share discovery in test.
  static void DisableShareDiscoveryForTesting() {
    disable_share_discovery_for_testing_ = true;
  }

  // Run |callback| when setup had completed. If setup has already completed,
  // |callback| will be run inline.
  void OnSetupCompleteForTesting(base::OnceClosure callback);

 private:
  friend class SmbServiceTest;

  // Calls SmbProviderClient::Mount(). |temp_file_manager_| must be initialized
  // before this is called.
  void CallMount(const file_system_provider::MountOptions& options,
                 const base::FilePath& share_path,
                 const std::string& username,
                 const std::string& password,
                 bool use_chromad_kerberos,
                 bool should_open_file_manager_after_mount,
                 bool save_credentials,
                 MountResponse callback);

  // Retrieves the mount_id for |file_system_info|.
  int32_t GetMountId(const ProvidedFileSystemInfo& info) const;

  // Calls file_system_provider::Service::UnmountFileSystem().
  base::File::Error Unmount(
      const std::string& file_system_id,
      file_system_provider::Service::UnmountReason reason);

  Service* GetProviderService() const;

  SmbProviderClient* GetSmbProviderClient() const;

  // Attempts to restore any previously mounted shares remembered by the File
  // System Provider.
  void RestoreMounts();

  void OnHostsDiscovered(
      const std::vector<ProvidedFileSystemInfo>& file_systems,
      const std::vector<SmbUrl>& preconfigured_shares);

  // Closure for OnHostDiscovered(). |reply| is passed down to
  // UpdateSharePath().
  void OnHostsDiscoveredForUpdateSharePath(
      int32_t mount_id,
      const std::string& share_path,
      StartReadDirIfSuccessfulCallback reply);

  // Attempts to remount a share with the information in |file_system_info|.
  void Remount(const ProvidedFileSystemInfo& file_system_info);

  // Handles the response from attempting to remount the file system. If
  // remounting fails, this logs and removes the file_system from the volume
  // manager.
  void OnRemountResponse(const std::string& file_system_id,
                         smbprovider::ErrorType error,
                         int32_t mount_id);

  // Calls SmbProviderClient::Premount(). |temp_file_manager_| must be
  // initialized before this is called.
  void Premount(const base::FilePath& share_path);

  // Handles the response from attempting to premount a share configured via
  // policy. If premounting fails it will log and exit the operation.
  void OnPremountResponse(const base::FilePath& share_path,
                          smbprovider::ErrorType error,
                          int32_t mount_id);

  // Sets up SmbService, including setting up Keberos if the user is ChromAD.
  void StartSetup();

  // Sets up |temp_file_manager_|. Calls CompleteSetup().
  void SetupTempFileManagerAndCompleteSetup();

  // Completes SmbService setup including ShareFinder initialization and
  // remounting shares. Called by SetupTempFileManagerAndCompleteSetup().
  void CompleteSetup(std::unique_ptr<TempFileManager> temp_file_manager);

  // Handles the response from attempting to setup Kerberos.
  void OnSetupKerberosResponse(bool success);

  // Fires |callback| with |result|.
  void FireMountCallback(MountResponse callback, SmbMountResult result);

  // Registers host locators for |share_finder_|.
  void RegisterHostLocators();

  // Set up Multicast DNS host locator.
  void SetUpMdnsHostLocator();

  // Set up NetBios host locator.
  void SetUpNetBiosHostLocator();

  // Opens |file_system_id| in the File Manager. Must only be called on a
  // mounted share.
  void OpenFileManager(const std::string& file_system_id);

  // Whether Network File Shares are allowed to be used. Controlled via policy.
  bool IsAllowedByPolicy() const;

  // Whether NetBios discovery should be used. Controlled via policy.
  bool IsNetBiosDiscoveryEnabled() const;

  // Whether NTLM should be used. Controlled via policy.
  bool IsNTLMAuthenticationEnabled() const;

  // Whether |share| is already mounted.
  bool IsShareMounted(const SmbUrl& share) const;

  // Gets the list of all shares preconfigured via policy with mode
  // |policy_mode|. If |policy_mode| is "unknown", returns a list of all shares
  // preconfigured with a mode that does not match any currently known mode.
  // This can occur if a new policy is added not yet supported by CrOS.
  std::vector<SmbUrl> GetPreconfiguredSharePaths(
      const std::string& policy_mode) const;

  // Gets the shares preconfigured via policy that should be displayed in the
  // discovery dropdown. This includes shares that are explicitly set to be
  // shown in the dropdown as well as shares configured with an unrecognized
  // mode.
  std::vector<SmbUrl> GetPreconfiguredSharePathsForDropdown() const;

  // Gets the shares preconfigured via policy that should be premounted.
  std::vector<SmbUrl> GetPreconfiguredSharePathsForPremount() const;

  // Requests new credentials for the |share_path|. |reply| is stored. Once the
  // credentials have been successfully updated, |reply| is run.
  void RequestCredentials(const std::string& share_path,
                          int32_t mount_id,
                          base::OnceClosure reply);

  // Opens a request credential dialog for the share path |share_path|.
  // When a user clicks "Update" in the dialog, UpdateCredentials is run.
  void OpenRequestCredentialsDialog(const std::string& share_path,
                                    int32_t mount_id);

  // Handles the response from attempting to the update the credentials of an
  // existing share. If |error| indicates success, the callback is run and
  // removed from |update_credential_replies_|. Otherwise, the callback
  // is removed from |update_credential_replies_| and the error is logged.
  void OnUpdateCredentialsResponse(int32_t mount_id,
                                   smbprovider::ErrorType error);

  // Requests an updated share path via running
  // ShareFinder::DiscoverHostsInNetwork. |reply| is stored. Once the share path
  // has been successfully updated, |reply| is run.
  void RequestUpdatedSharePath(const std::string& share_path,
                               int32_t mount_id,
                               StartReadDirIfSuccessfulCallback reply);

  // Handles the response for attempting to update the share path of a mount.
  // |reply| will run if |error| is ERROR_OK. Logs the error otherwise.
  void OnUpdateSharePathResponse(int32_t mount_id,
                                 StartReadDirIfSuccessfulCallback reply,
                                 smbprovider::ErrorType error);

  // Helper function that determines if HostDiscovery can be run again. Returns
  // false if HostDiscovery was recently run.
  bool ShouldRunHostDiscoveryAgain() const;

  // NetworkChangeNotifier::NetworkChangeObserver override. Runs HostDiscovery
  // when network detects a change.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // Records metrics on the number of SMB mounts a user has.
  void RecordMountCount() const;

  static bool service_should_run_;
  static bool disable_share_discovery_for_testing_;

  base::TimeTicks previous_host_discovery_time_;
  const ProviderId provider_id_;
  Profile* profile_;
  std::unique_ptr<base::TickClock> tick_clock_;
  std::unique_ptr<TempFileManager> temp_file_manager_;
  std::unique_ptr<SmbShareFinder> share_finder_;
  // |mount_id| -> |reply|. Stored callbacks to run after updating credential.
  std::map<int32_t, base::OnceClosure> update_credential_replies_;
  // |file_system_id| -> |mount_id|
  std::unordered_map<std::string, int32_t> mount_id_map_;

  base::OnceClosure setup_complete_callback_;

  DISALLOW_COPY_AND_ASSIGN(SmbService);
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_SERVICE_H_

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
#include "chrome/browser/chromeos/smb_client/smb_persisted_share_registry.h"
#include "chrome/browser/chromeos/smb_client/smb_share_finder.h"
#include "chrome/browser/chromeos/smb_client/smb_task_queue.h"
#include "chrome/browser/chromeos/smb_client/smbfs_share.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/power/power_manager_client.h"
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

using file_system_provider::ProvidedFileSystemInfo;

class SmbKerberosCredentialsUpdater;
class SmbShareInfo;

// Creates and manages an smb file system.
class SmbService : public KeyedService,
                   public net::NetworkChangeNotifier::NetworkChangeObserver,
                   public chromeos::PowerManagerClient::Observer,
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

  // KeyedService override.
  void Shutdown() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Starts the process of mounting an SMB file system.
  // |use_kerberos| indicates whether the share should be mounted with a user's
  // chromad kerberos tickets.
  void Mount(const file_system_provider::MountOptions& options,
             const base::FilePath& share_path,
             const std::string& username,
             const std::string& password,
             bool use_kerberos,
             bool should_open_file_manager_after_mount,
             bool save_credentials,
             MountResponse callback);

  // Unmounts the SmbFs share mounted at |mount_path|.
  void UnmountSmbFs(const base::FilePath& mount_path);

  // Returns the SmbFsShare instance for the file at |path|. If |path| is not
  // part of an smbfs share, returns nullptr.
  SmbFsShare* GetSmbFsShareForPath(const base::FilePath& path);

  // Gathers the hosts in the network using |share_finder_| and gets the shares
  // for each of the hosts found. |discovery_callback| is called as soon as host
  // discovery is complete. |shares_callback| may be called multiple times with
  // new shares. |shares_callback| will be called with |done| == false when more
  // shares are expected to be discovered. When share discovery is finished,
  // |shares_callback| is called with |done| == true and will not be called
  // again.
  void GatherSharesInNetwork(HostDiscoveryResponse discovery_callback,
                             GatherSharesResponse shares_callback);

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

  // Sets up Kerberos / AD services.
  void SetupKerberos(const std::string& account_identifier);

  // Updates credentials for Kerberos service.
  void UpdateKerberosCredentials(const std::string& account_identifier);

  // Returns true if Kerberos was enabled via policy at service creation time
  // and is still enabled now.
  bool IsKerberosEnabledViaPolicy() const;

  // Sets the mounter creation callback, which is passed to
  // SmbFsShare::SetMounterCreationCallbackForTest() when a new SmbFs share is
  // created.
  void SetSmbFsMounterCreationCallbackForTesting(
      SmbFsShare::MounterCreationCallback callback);

  // chromeos::PowerManagerClient::Observer overrides
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

 private:
  friend class SmbServiceTest;

  using MountInternalCallback =
      base::OnceCallback<void(SmbMountResult result,
                              const base::FilePath& mount_path)>;

  // Callback passed to MountInternal().
  void MountInternalDone(MountResponse callback,
                         const SmbShareInfo& info,
                         bool should_open_file_manager_after_mount,
                         SmbMountResult result,
                         const base::FilePath& mount_path);

  // Mounts an SMB share with url |share_url| using either smbprovider or smbfs
  // based on feature flags.
  // Calls SmbProviderClient::Mount() or start the smbfs mount process.
  void MountInternal(const file_system_provider::MountOptions& options,
                     const SmbShareInfo& info,
                     const std::string& password,
                     bool save_credentials,
                     bool skip_connect,
                     MountInternalCallback callback);

  // Handles the response from mounting an SMB share using smbprovider.
  // Completes the mounting of an SMB file system, passing |options| on to
  // file_system_provider::Service::MountFileSystem(). Passes error status to
  // callback.
  void OnProviderMountDone(MountInternalCallback callback,
                           const file_system_provider::MountOptions& options,
                           bool save_credentials,
                           smbprovider::ErrorType error,
                           int32_t mount_id);

  // Handles the response from mounting an smbfs share. Passes |result| onto
  // |callback|.
  void OnSmbfsMountDone(const std::string& smbfs_mount_id,
                        MountInternalCallback callback,
                        SmbMountResult result);

  // Callback passed to SmbFsShare::Unmount() during a power management
  // suspension. Ensures that suspension is blocked until the unmount completes.
  void OnSuspendUnmountDone(base::UnguessableToken power_manager_suspend_token,
                            chromeos::MountError result);

  // Retrieves the mount_id for |file_system_info|.
  int32_t GetMountId(const ProvidedFileSystemInfo& info) const;

  // Calls file_system_provider::Service::UnmountFileSystem().
  base::File::Error Unmount(
      const std::string& file_system_id,
      file_system_provider::Service::UnmountReason reason);

  file_system_provider::Service* GetProviderService() const;

  SmbProviderClient* GetSmbProviderClient() const;

  // Attempts to restore any previously mounted shares remembered by the File
  // System Provider.
  void RestoreMounts();

  void OnHostsDiscovered(
      const std::vector<ProvidedFileSystemInfo>& file_systems,
      const std::vector<SmbShareInfo>& saved_smbfs_shares,
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

  // Mounts a saved (smbfs) SMB share with details |info|.
  void MountSavedSmbfsShare(const SmbShareInfo& info);

  // Mounts a preconfigured (by policy) SMB share with path |share_url|. The
  // share is mounted with empty credentials.
  void MountPreconfiguredShare(const SmbUrl& share_url);

  // Handles the response from attempting to mount a share configured via
  // policy.
  void OnMountPreconfiguredShareDone(SmbMountResult result,
                                     const base::FilePath& mount_path);

  // Completes SmbService setup including ShareFinder initialization and
  // remounting shares.
  void CompleteSetup();

  // Handles the response from attempting to setup Kerberos.
  void OnSetupKerberosResponse(bool success);

  // Handles the response from attempting to update Kerberos credentials.
  void OnUpdateKerberosCredentialsResponse(bool success);

  // Registers host locators for |share_finder_|.
  void RegisterHostLocators();

  // Set up Multicast DNS host locator.
  void SetUpMdnsHostLocator();

  // Set up NetBios host locator.
  void SetUpNetBiosHostLocator();

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

  // Handles the response from showing the SMB credentials dialog. If |canceled|
  // is true, the |reply| callback is dropped. Otherwise, |username| and
  // |password| are passed to the smb service and |reply| is run if the service
  // returns success.
  void OnSmbCredentialsDialogShown(int32_t mount_id,
                                   base::OnceClosure reply,
                                   bool canceled,
                                   const std::string& username,
                                   const std::string& password);

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

  // Handles the callback for SmbFsShare::RemoveSavedCredentials().
  void OnSmbfsRemoveSavedCredentialsDone(const std::string& mount_id,
                                         bool success);

  // Helper function that determines if HostDiscovery can be run again. Returns
  // false if HostDiscovery was recently run.
  bool ShouldRunHostDiscoveryAgain() const;

  // NetworkChangeNotifier::NetworkChangeObserver override. Runs HostDiscovery
  // when network detects a change.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // Records metrics on the number of SMB mounts a user has.
  void RecordMountCount() const;

  static bool disable_share_discovery_for_testing_;

  base::TimeTicks previous_host_discovery_time_;
  const file_system_provider::ProviderId provider_id_;
  Profile* profile_;
  std::unique_ptr<base::TickClock> tick_clock_;
  std::unique_ptr<SmbShareFinder> share_finder_;
  // |file_system_id| -> |mount_id|
  std::unordered_map<std::string, int32_t> mount_id_map_;
  // |smbfs_mount_id| -> SmbFsShare
  // Note, mount ID for smbfs is a randomly generated string. For smbprovider
  // shares, it is an integer.
  std::unordered_map<std::string, std::unique_ptr<SmbFsShare>> smbfs_shares_;
  SmbPersistedShareRegistry registry_;

  std::unique_ptr<SmbKerberosCredentialsUpdater> smb_credentials_updater_;

  base::OnceClosure setup_complete_callback_;
  SmbFsShare::MounterCreationCallback smbfs_mounter_creation_callback_;

  DISALLOW_COPY_AND_ASSIGN(SmbService);
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_SERVICE_H_

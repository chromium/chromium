// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SERVICE_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provider_interface.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/smb_client/smb_errors.h"
#include "chrome/browser/ash/smb_client/smb_persisted_share_registry.h"
#include "chrome/browser/ash/smb_client/smb_share_finder.h"
#include "chrome/browser/ash/smb_client/smbfs_share.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/smbprovider/smb_provider_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/base/network_change_notifier.h"

namespace base {
class FilePath;
}  // namespace base

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ash::smb_client {

class SmbKerberosCredentialsUpdater;
class SmbShareInfo;

// Creates and manages an smb file system.
class SmbService : public KeyedService,
                   public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  using MountResponse = base::OnceCallback<void(SmbMountResult result)>;
  using StartReadDirIfSuccessfulCallback =
      base::OnceCallback<void(bool should_retry_start_read_dir)>;
  using GatherSharesResponse =
      base::RepeatingCallback<void(const std::vector<SmbUrl>& shares_gathered,
                                   bool done)>;

  SmbService(Profile* profile, std::unique_ptr<base::TickClock> tick_clock);
  SmbService(const SmbService&) = delete;
  SmbService& operator=(const SmbService&) = delete;
  ~SmbService() override;

  // KeyedService override.
  void Shutdown() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Starts the process of mounting an SMB file system. |use_kerberos| indicates
  // whether the share should be mounted with a Kerberos ticket - acquired
  // though Chromad login or KerberosCredentialsManager.
  void Mount(const std::string& display_name,
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

  // Returns true if the Kerberos feature is enabled.
  bool IsKerberosEnabledViaPolicy() const;

  // Sets the mounter creation callback, which is passed to
  // SmbFsShare::SetMounterCreationCallbackForTest() when a new SmbFs share is
  // created.
  void SetSmbFsMounterCreationCallbackForTesting(
      SmbFsShare::MounterCreationCallback callback);

  // Returns true if any SMB shares have been configured or saved before.
  bool IsAnySmbShareConfigured();

 private:
  FRIEND_TEST_ALL_PREFIXES(SmbServiceWithSmbfsTest, MountInvalidSaved);
  FRIEND_TEST_ALL_PREFIXES(SmbServiceWithSmbfsTest, MountInvalidPreconfigured);

  using MountInternalCallback =
      base::OnceCallback<void(SmbMountResult result,
                              const base::FilePath& mount_path)>;

  // Callback passed to MountInternal() when mounts are initiated
  // (generally by user interaction) via Mount().
  void OnUserInitiatedMountDone(MountResponse callback,
                                const SmbShareInfo& info,
                                bool should_open_file_manager_after_mount,
                                SmbMountResult result,
                                const base::FilePath& mount_path);

  // Mounts an SMB share with url |share_url| using either smbprovider or smbfs
  // based on feature flags.
  // Calls SmbProviderClient::Mount() or start the smbfs mount process.
  void MountInternal(const SmbShareInfo& info,
                     const std::string& password,
                     bool save_credentials,
                     bool skip_connect,
                     MountInternalCallback callback);

  // Handles the response from mounting an smbfs share. Passes |result| onto
  // |callback|.
  void OnSmbfsMountDone(const std::string& smbfs_mount_id,
                        MountInternalCallback callback,
                        SmbMountResult result);

  file_system_provider::Service* GetProviderService() const;

  SmbProviderClient* GetSmbProviderClient() const;

  // Attempts to restore any previously mounted shares remembered by the File
  // System Provider.
  void RestoreMounts();

  void OnHostsDiscovered(
      const std::vector<SmbShareInfo>& saved_smbfs_shares,
      const std::vector<SmbUrl>& preconfigured_shares);

  // Sets the callback passed to MountInternal() when a saved or
  // preconfigured share mount request is made during setup.
  void SetRestoredShareMountDoneCallbackForTesting(
      MountInternalCallback callback);

  // Mounts a saved (smbfs) SMB share with details |info|.
  void MountSavedSmbfsShare(const SmbShareInfo& info);

  // Handles the response from attempting to mount a previously saved share.
  void OnMountSavedSmbfsShareDone(SmbMountResult result,
                                  const base::FilePath& mount_path);

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

  // Handles the callback for SmbFsShare::RemoveSavedCredentials().
  void OnSmbfsRemoveSavedCredentialsDone(const std::string& mount_id,
                                         bool success);

  // NetworkChangeNotifier::NetworkChangeObserver override. Runs HostDiscovery
  // when network detects a change.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // Records metrics on the number of SMB mounts a user has.
  void RecordMountCount() const;

  static bool disable_share_discovery_for_testing_;

  const file_system_provider::ProviderId provider_id_;
  raw_ptr<Profile> profile_;
  std::unique_ptr<SmbShareFinder> share_finder_;
  // |smbfs_mount_id| -> SmbFsShare
  // Note, mount ID for smbfs is a randomly generated string. For smbprovider
  // shares, it is an integer.
  std::unordered_map<std::string, std::unique_ptr<SmbFsShare>> smbfs_shares_;
  SmbPersistedShareRegistry registry_;

  std::unique_ptr<SmbKerberosCredentialsUpdater> kerberos_credentials_updater_;

  base::OnceClosure setup_complete_callback_;
  SmbFsShare::MounterCreationCallback smbfs_mounter_creation_callback_;
  MountInternalCallback restored_share_mount_done_callback_;
  base::WeakPtrFactory<SmbService> weak_ptr_factory_{this};
};

}  // namespace ash::smb_client

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SERVICE_H_

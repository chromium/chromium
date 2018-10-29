// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_SERVICE_H_

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provider_interface.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/smb_client/smb_errors.h"
#include "chrome/browser/chromeos/smb_client/smb_share_finder.h"
#include "chrome/browser/chromeos/smb_client/temp_file_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/smb_provider_client.h"
#include "components/keyed_service/core/keyed_service.h"

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
                   public base::SupportsWeakPtr<SmbService> {
 public:
  using MountResponse = base::OnceCallback<void(SmbMountResult result)>;

  explicit SmbService(Profile* profile);
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
             MountResponse callback);

  // Completes the mounting of an SMB file system, passing |options| on to
  // file_system_provider::Service::MountFileSystem(). Passes error status to
  // callback.
  void OnMountResponse(MountResponse callback,
                       const file_system_provider::MountOptions& options,
                       const base::FilePath& share_path,
                       bool is_kerberos_chromad,
                       smbprovider::ErrorType error,
                       int32_t mount_id);

  // Gathers the hosts in the network using |share_finder_| and gets the shares
  // for each of the hosts found. |discovery_callback| is called as soon as host
  // discovery is complete. |shares_callback| is called once per host and will
  // contain the URLs to the shares found.
  void GatherSharesInNetwork(HostDiscoveryResponse discovery_callback,
                             GatherSharesResponse shares_callback);

 private:
  // Calls SmbProviderClient::Mount(). |temp_file_manager_| must be initialized
  // before this is called.
  void CallMount(const file_system_provider::MountOptions& options,
                 const base::FilePath& share_path,
                 const std::string& username,
                 const std::string& password,
                 bool use_chromad_kerberos,
                 MountResponse callback);

  // Calls file_system_provider::Service::UnmountFileSystem().
  base::File::Error Unmount(
      const std::string& file_system_id,
      file_system_provider::Service::UnmountReason reason) const;

  Service* GetProviderService() const;

  SmbProviderClient* GetSmbProviderClient() const;

  // Attempts to restore any previously mounted shares remembered by the File
  // System Provider.
  void RestoreMounts();

  void OnHostsDiscovered(
      const std::vector<ProvidedFileSystemInfo>& file_systems);

  // Attempts to remount a share with the information in |file_system_info|.
  void Remount(const ProvidedFileSystemInfo& file_system_info);

  // Handles the response from attempting to remount the file system. If
  // remounting fails, this logs and removes the file_system from the volume
  // manager.
  void OnRemountResponse(const std::string& file_system_id,
                         smbprovider::ErrorType error);

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

  // Gets the shares preconfigured via policy that should be displayed in the
  // discovery drop down.
  std::vector<SmbUrl> GetPreconfiguredSharePathsForDropDown() const;

  // Records metrics on the number of SMB mounts a user has.
  void RecordMountCount() const;

  static bool service_should_run_;
  const ProviderId provider_id_;
  Profile* profile_;
  std::unique_ptr<TempFileManager> temp_file_manager_;
  std::unique_ptr<SmbShareFinder> share_finder_;

  DISALLOW_COPY_AND_ASSIGN(SmbService);
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_SERVICE_H_

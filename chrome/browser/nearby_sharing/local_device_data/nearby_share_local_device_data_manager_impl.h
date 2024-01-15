// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "third_party/nearby/sharing/proto/device_rpc.pb.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

class NearbyShareClientFactory;
class NearbyShareDeviceDataUpdater;
class NearbyShareProfileInfoProvider;
class PrefService;

namespace ash::nearby {
class NearbyScheduler;
}  // namespace ash::nearby

// Implementation of NearbyShareLocalDeviceDataManager that persists device data
// in prefs. All RPC-related calls are guarded by a timeout, so callbacks are
// guaranteed to be invoked. In addition to supporting on-demand device-data
// downloads, this implementation schedules periodic downloads of device
// data--full name and icon URL--from the server.
class NearbyShareLocalDeviceDataManagerImpl
    : public NearbyShareLocalDeviceDataManager {
 public:
  class Factory {
   public:
    static std::unique_ptr<NearbyShareLocalDeviceDataManager> Create(
        PrefService* pref_service,
        NearbyShareClientFactory* http_client_factory,
        NearbyShareProfileInfoProvider* profile_info_provider);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<NearbyShareLocalDeviceDataManager> CreateInstance(
        PrefService* pref_service,
        NearbyShareClientFactory* http_client_factory,
        NearbyShareProfileInfoProvider* profile_info_provider) = 0;

   private:
    static Factory* test_factory_;
  };

  ~NearbyShareLocalDeviceDataManagerImpl() override;

 private:
  NearbyShareLocalDeviceDataManagerImpl(
      PrefService* pref_service,
      NearbyShareClientFactory* http_client_factory,
      NearbyShareProfileInfoProvider* profile_info_provider);

  // NearbyShareLocalDeviceDataManager:
  std::string GetId() override;
  std::string GetDeviceName() const override;
  std::optional<std::string> GetFullName() const override;
  std::optional<std::string> GetIconUrl() const override;
  nearby_share::mojom::DeviceNameValidationResult ValidateDeviceName(
      const std::string& name) override;
  nearby_share::mojom::DeviceNameValidationResult SetDeviceName(
      const std::string& name) override;
  void DownloadDeviceData() override;
  void UploadContacts(std::vector<nearby::sharing::proto::Contact> contacts,
                      UploadCompleteCallback callback) override;
  void UploadCertificates(
      std::vector<nearby::sharing::proto::PublicCertificate> certificates,
      UploadCompleteCallback callback) override;
  void OnStart() override;
  void OnStop() override;

  std::optional<std::string> GetIconToken() const;

  // Creates a default device name of the form "<given name>'s <device type>."
  // For example, "Josh's Chromebook." If a given name cannot be found, returns
  // just the device type. If the resulting name is too long the user's name
  // will be truncated, for example "Mi...'s Chromebook."
  std::string GetDefaultDeviceName() const;

  void OnDownloadDeviceDataRequested();
  void OnDownloadDeviceDataFinished(
      const std::optional<nearby::sharing::proto::UpdateDeviceResponse>&
          response);
  void OnUploadContactsFinished(
      UploadCompleteCallback callback,
      const std::optional<nearby::sharing::proto::UpdateDeviceResponse>&
          response);
  void OnUploadCertificatesFinished(
      UploadCompleteCallback callback,
      const std::optional<nearby::sharing::proto::UpdateDeviceResponse>&
          response);
  void HandleUpdateDeviceResponse(
      const std::optional<nearby::sharing::proto::UpdateDeviceResponse>&
          response);

  raw_ptr<PrefService> pref_service_ = nullptr;
  raw_ptr<NearbyShareProfileInfoProvider> profile_info_provider_ = nullptr;
  std::unique_ptr<NearbyShareDeviceDataUpdater> device_data_updater_;
  std::unique_ptr<ash::nearby::NearbyScheduler> download_device_data_scheduler_;
  std::string default_device_name_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_IMPL_H_

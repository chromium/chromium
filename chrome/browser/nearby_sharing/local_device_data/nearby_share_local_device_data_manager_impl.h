// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "chrome/browser/nearby_sharing/proto/device_rpc.pb.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class NearbyShareClientFactory;
class NearbyShareDeviceDataUpdater;
class NearbyShareProfileInfoProvider;
class NearbyShareScheduler;
class PrefService;

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
  absl::optional<std::string> GetFullName() const override;
  absl::optional<std::string> GetIconUrl() const override;
  nearby_share::mojom::DeviceNameValidationResult ValidateDeviceName(
      const std::string& name) override;
  nearby_share::mojom::DeviceNameValidationResult SetDeviceName(
      const std::string& name) override;
  void DownloadDeviceData() override;
  void UploadContacts(std::vector<nearbyshare::proto::Contact> contacts,
                      UploadCompleteCallback callback) override;
  void UploadCertificates(
      std::vector<nearbyshare::proto::PublicCertificate> certificates,
      UploadCompleteCallback callback) override;
  void OnStart() override;
  void OnStop() override;

  absl::optional<std::string> GetIconToken() const;

  // Creates a default device name of the form "<given name>'s <device type>."
  // For example, "Josh's Chromebook." If a given name cannot be found, returns
  // just the device type. If the resulting name is too long the user's name
  // will be truncated, for example "Mi...'s Chromebook."
  std::string GetDefaultDeviceName() const;

  void OnDownloadDeviceDataRequested();
  void OnDownloadDeviceDataFinished(
      const absl::optional<nearbyshare::proto::UpdateDeviceResponse>& response);
  void OnUploadContactsFinished(
      UploadCompleteCallback callback,
      const absl::optional<nearbyshare::proto::UpdateDeviceResponse>& response);
  void OnUploadCertificatesFinished(
      UploadCompleteCallback callback,
      const absl::optional<nearbyshare::proto::UpdateDeviceResponse>& response);
  void HandleUpdateDeviceResponse(
      const absl::optional<nearbyshare::proto::UpdateDeviceResponse>& response);

  PrefService* pref_service_ = nullptr;
  NearbyShareProfileInfoProvider* profile_info_provider_ = nullptr;
  std::unique_ptr<NearbyShareDeviceDataUpdater> device_data_updater_;
  std::unique_ptr<NearbyShareScheduler> download_device_data_scheduler_;
  std::string default_device_name_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_IMPL_H_

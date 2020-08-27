// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/optional.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"

// Manages local device data related to the UpdateDevice RPC such as the device
// ID, name, and icon url; provides the user's full name and icon URL returned
// from the Nearby server; and handles uploading contacts and certificates to
// the Nearby server. The uploading of contacts and certificates might seem out
// of place, but this class is the entry point for  all UpdateDevice RPC calls.
class NearbyShareLocalDeviceDataManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnLocalDeviceDataChanged(bool did_device_name_change,
                                          bool did_full_name_change,
                                          bool did_icon_url_change) = 0;
  };

  using UploadCompleteCallback = base::OnceCallback<void(bool success)>;

  NearbyShareLocalDeviceDataManager();
  virtual ~NearbyShareLocalDeviceDataManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Starts/Stops local-device-data task scheduling.
  void Start();
  void Stop();
  bool is_running() { return is_running_; }

  // Returns the immutable ID generated for the local device, used to
  // differentiate a user's devices when communicating with the Nearby server.
  virtual std::string GetId() = 0;

  // Returns the name of the local device, typically in the format of
  // "UserName's DeviceType". This can be modified by SetDeviceName(). Returns
  // base::nullopt if the device name have not been set yet.
  virtual base::Optional<std::string> GetDeviceName() const = 0;

  // Returns the user's full name, for example, "Barack Obama". Returns
  // base::nullopt if the name has not yet been set from an UpdateDevice RPC
  // response.
  virtual base::Optional<std::string> GetFullName() const = 0;

  // Returns the URL of the user's image. Returns base::nullopt if the URL has
  // not yet been set from an UpdateDevice RPC response.
  virtual base::Optional<std::string> GetIconUrl() const = 0;

  // Sets and persists the device name in prefs. The device name is *not*
  // uploaded to the Nearby Share server; the UpdateDevice proto device_name
  // field in an artifact. Observers are notified via OnLocalDeviceDataChanged()
  // if the device name changes.
  virtual void SetDeviceName(const std::string& name) = 0;

  // Makes an UpdateDevice RPC call to the Nearby Share server to retrieve all
  // available device data, which includes the full name and icon URL for now.
  // This action is also scheduled periodically. Observers are notified via
  // OnLocalDeviceDataChanged() if any device data changes.
  virtual void DownloadDeviceData() = 0;

  // Uses the UpdateDevice RPC to send the local device's contact list to the
  // Nearby Share server, including which contacts are allowed for
  // selected-contacts visibility mode. This should only be invoked by the
  // contact manager, and the contact manager should handle scheduling, failure
  // retry, etc.
  virtual void UploadContacts(std::vector<nearbyshare::proto::Contact> contacts,
                              UploadCompleteCallback callback) = 0;

  // Uses the UpdateDevice RPC to send the local device's public certificates to
  // the Nearby Share server. This should only be invoked by the certificate
  // manager, and the certificate manager should handle scheduling, failure
  // retry, etc.
  virtual void UploadCertificates(
      std::vector<nearbyshare::proto::PublicCertificate> certificates,
      UploadCompleteCallback callback) = 0;

 protected:
  virtual void OnStart() = 0;
  virtual void OnStop() = 0;

  void NotifyLocalDeviceDataChanged(bool did_device_name_change,
                                    bool did_full_name_change,
                                    bool did_icon_url_change);

 private:
  bool is_running_ = false;
  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_DEVICE_DATA_UPDATER_H_
#define CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_DEVICE_DATA_UPDATER_H_

#include <optional>
#include <string>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "third_party/nearby/sharing/proto/device_rpc.pb.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

// Manages a queue of data needed to make UpdateDevice RPC requests to the
// Nearby Server. Implementations should make the actual HTTP calls by
// overriding HandleNextRequest(), which is invoked when the next request is
// ready to be run. Implementations should call FinishAttempt() with the result
// of the attempt and possibly the response.
//
// NOTE: We do *not* support upload of the device name. This field of the proto
// appears to be a relic of the old Nearby Share model. Upload of the field
// could be a privacy concern that we want to avoid.
//
// TODO(crbug.com/1105547): Instead of queuing requests, hold a single pending
// request and update the fields as other UpdateDeviceData() call are made.
// Then, queue up all of callbacks from the merged requests in the Request
// struct; invoke the callback in the order they were added. This will reduce
// the number of UpdateDevice RPC calls.
class NearbyShareDeviceDataUpdater {
 public:
  // If the request is unsuccessful, |response| is std::nullopt.
  using ResultCallback = base::OnceCallback<void(
      const std::optional<nearby::sharing::proto::UpdateDeviceResponse>&
          response)>;

  struct Request {
    Request(
        std::optional<std::vector<nearby::sharing::proto::Contact>> contacts,
        std::optional<std::vector<nearby::sharing::proto::PublicCertificate>>
            certificates,
        ResultCallback callback);
    Request(Request&& request);
    Request& operator=(Request&& request);
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    ~Request();

    std::optional<std::vector<nearby::sharing::proto::Contact>> contacts;
    std::optional<std::vector<nearby::sharing::proto::PublicCertificate>>
        certificates;
    ResultCallback callback;
  };

  // |device_id|: The ID used by the Nearby server to differentiate multiple
  //              devices from the same account.
  explicit NearbyShareDeviceDataUpdater(const std::string& device_id);

  virtual ~NearbyShareDeviceDataUpdater();

  // Queue up an UpdateDevice RPC request to update the following fields on the
  // Nearby server if the parameter is not std::nullopt:
  //
  // |contacts|: The list of contacts that the Nearby server will send
  //             all-contacts-visibility certificates to. Contacts marked
  //             is_selected will be sent selected-contacts-visibility
  //             certificates.
  //
  // |certificates|: The local device's certificates that the Nearby server will
  //                 distribute to the appropriate |contacts|.
  //
  // If only the UpdateDevice RPC response data is desired, set all
  // aforementioned parameters to std::nullopt.
  void UpdateDeviceData(
      std::optional<std::vector<nearby::sharing::proto::Contact>> contacts,
      std::optional<std::vector<nearby::sharing::proto::PublicCertificate>>
          certificates,
      ResultCallback callback);

 protected:
  void ProcessRequestQueue();
  virtual void HandleNextRequest() = 0;

  // If the request is unsuccessful, |response| is std::nullopt.
  void FinishAttempt(
      const std::optional<nearby::sharing::proto::UpdateDeviceResponse>&
          response);

  std::string device_id_;
  bool is_request_in_progress_ = false;
  base::queue<Request> pending_requests_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_DEVICE_DATA_UPDATER_H_

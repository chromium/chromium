// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CLIENT_FAKE_NEARBY_SHARE_CLIENT_H_
#define CHROME_BROWSER_NEARBY_SHARING_CLIENT_FAKE_NEARBY_SHARE_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_client.h"
#include "third_party/nearby/sharing/proto/certificate_rpc.pb.h"
#include "third_party/nearby/sharing/proto/contact_rpc.pb.h"
#include "third_party/nearby/sharing/proto/device_rpc.pb.h"

// A fake implementation of the Nearby Share HTTP client that stores all request
// data. Only use in unit tests.
class FakeNearbyShareClient : public NearbyShareClient {
 public:
  struct UpdateDeviceRequest {
    UpdateDeviceRequest(
        const nearby::sharing::proto::UpdateDeviceRequest& request,
        UpdateDeviceCallback&& callback,
        ErrorCallback&& error_callback);
    UpdateDeviceRequest(UpdateDeviceRequest&& request);
    ~UpdateDeviceRequest();
    nearby::sharing::proto::UpdateDeviceRequest request;
    UpdateDeviceCallback callback;
    ErrorCallback error_callback;
  };
  struct ListContactPeopleRequest {
    ListContactPeopleRequest(
        const nearby::sharing::proto::ListContactPeopleRequest& request,
        ListContactPeopleCallback&& callback,
        ErrorCallback&& error_callback);
    ListContactPeopleRequest(ListContactPeopleRequest&& request);
    ~ListContactPeopleRequest();
    nearby::sharing::proto::ListContactPeopleRequest request;
    ListContactPeopleCallback callback;
    ErrorCallback error_callback;
  };
  struct ListPublicCertificatesRequest {
    ListPublicCertificatesRequest(
        const nearby::sharing::proto::ListPublicCertificatesRequest& request,
        ListPublicCertificatesCallback&& callback,
        ErrorCallback&& error_callback);
    ListPublicCertificatesRequest(ListPublicCertificatesRequest&& request);
    ~ListPublicCertificatesRequest();
    nearby::sharing::proto::ListPublicCertificatesRequest request;
    ListPublicCertificatesCallback callback;
    ErrorCallback error_callback;
  };

  FakeNearbyShareClient();
  ~FakeNearbyShareClient() override;

  std::vector<UpdateDeviceRequest>& update_device_requests() {
    return update_device_requests_;
  }
  std::vector<ListContactPeopleRequest>& list_contact_people_requests() {
    return list_contact_people_requests_;
  }
  std::vector<ListPublicCertificatesRequest>&
  list_public_certificates_requests() {
    return list_public_certificates_requests_;
  }

  void SetAccessTokenUsed(const std::string& token);

 private:
  // NearbyShareClient:
  void UpdateDevice(const nearby::sharing::proto::UpdateDeviceRequest& request,
                    UpdateDeviceCallback&& callback,
                    ErrorCallback&& error_callback) override;
  void ListContactPeople(
      const nearby::sharing::proto::ListContactPeopleRequest& request,
      ListContactPeopleCallback&& callback,
      ErrorCallback&& error_callback) override;
  void ListPublicCertificates(
      const nearby::sharing::proto::ListPublicCertificatesRequest& request,
      ListPublicCertificatesCallback&& callback,
      ErrorCallback&& error_callback) override;
  std::string GetAccessTokenUsed() override;

  std::vector<UpdateDeviceRequest> update_device_requests_;
  std::vector<ListContactPeopleRequest> list_contact_people_requests_;
  std::vector<ListPublicCertificatesRequest> list_public_certificates_requests_;
  std::string access_token_used_;
};

class FakeNearbyShareClientFactory : public NearbyShareClientFactory {
 public:
  FakeNearbyShareClientFactory();
  ~FakeNearbyShareClientFactory() override;

 public:
  // Returns all FakeNearbyShareClient instances created by CreateInstance().
  std::vector<raw_ptr<FakeNearbyShareClient, VectorExperimental>>& instances() {
    return instances_;
  }

 private:
  // NearbyShareClientFactory:
  std::unique_ptr<NearbyShareClient> CreateInstance() override;

  std::vector<raw_ptr<FakeNearbyShareClient, VectorExperimental>> instances_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CLIENT_FAKE_NEARBY_SHARE_CLIENT_H_

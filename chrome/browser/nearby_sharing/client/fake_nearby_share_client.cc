// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/client/fake_nearby_share_client.h"

#include <utility>

FakeNearbyShareClient::UpdateDeviceRequest::UpdateDeviceRequest(
    const nearby::sharing::proto::UpdateDeviceRequest& request,
    UpdateDeviceCallback&& callback,
    ErrorCallback&& error_callback)
    : request(request),
      callback(std::move(callback)),
      error_callback(std::move(error_callback)) {}

FakeNearbyShareClient::UpdateDeviceRequest::UpdateDeviceRequest(
    FakeNearbyShareClient::UpdateDeviceRequest&& request) = default;

FakeNearbyShareClient::UpdateDeviceRequest::~UpdateDeviceRequest() = default;

FakeNearbyShareClient::ListContactPeopleRequest::ListContactPeopleRequest(
    const nearby::sharing::proto::ListContactPeopleRequest& request,
    ListContactPeopleCallback&& callback,
    ErrorCallback&& error_callback)
    : request(request),
      callback(std::move(callback)),
      error_callback(std::move(error_callback)) {}

FakeNearbyShareClient::ListContactPeopleRequest::ListContactPeopleRequest(
    FakeNearbyShareClient::ListContactPeopleRequest&& request) = default;

FakeNearbyShareClient::ListContactPeopleRequest::~ListContactPeopleRequest() =
    default;

FakeNearbyShareClient::ListPublicCertificatesRequest::
    ListPublicCertificatesRequest(
        const nearby::sharing::proto::ListPublicCertificatesRequest& request,
        ListPublicCertificatesCallback&& callback,
        ErrorCallback&& error_callback)
    : request(request),
      callback(std::move(callback)),
      error_callback(std::move(error_callback)) {}

FakeNearbyShareClient::ListPublicCertificatesRequest::
    ListPublicCertificatesRequest(
        FakeNearbyShareClient::ListPublicCertificatesRequest&& request) =
        default;

FakeNearbyShareClient::ListPublicCertificatesRequest::
    ~ListPublicCertificatesRequest() = default;

FakeNearbyShareClient::FakeNearbyShareClient() = default;

FakeNearbyShareClient::~FakeNearbyShareClient() = default;

void FakeNearbyShareClient::SetAccessTokenUsed(const std::string& token) {
  access_token_used_ = token;
}

void FakeNearbyShareClient::UpdateDevice(
    const nearby::sharing::proto::UpdateDeviceRequest& request,
    UpdateDeviceCallback&& callback,
    ErrorCallback&& error_callback) {
  update_device_requests_.emplace_back(request, std::move(callback),
                                       std::move(error_callback));
}

void FakeNearbyShareClient::ListContactPeople(
    const nearby::sharing::proto::ListContactPeopleRequest& request,
    ListContactPeopleCallback&& callback,
    ErrorCallback&& error_callback) {
  list_contact_people_requests_.emplace_back(request, std::move(callback),
                                             std::move(error_callback));
}

void FakeNearbyShareClient::ListPublicCertificates(
    const nearby::sharing::proto::ListPublicCertificatesRequest& request,
    ListPublicCertificatesCallback&& callback,
    ErrorCallback&& error_callback) {
  list_public_certificates_requests_.emplace_back(request, std::move(callback),
                                                  std::move(error_callback));
}

std::string FakeNearbyShareClient::GetAccessTokenUsed() {
  return access_token_used_;
}

FakeNearbyShareClientFactory::FakeNearbyShareClientFactory() = default;

FakeNearbyShareClientFactory::~FakeNearbyShareClientFactory() = default;

std::unique_ptr<NearbyShareClient>
FakeNearbyShareClientFactory::CreateInstance() {
  auto instance = std::make_unique<FakeNearbyShareClient>();
  instances_.push_back(instance.get());

  return instance;
}

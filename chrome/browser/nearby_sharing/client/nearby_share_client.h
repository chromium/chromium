// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CLIENT_NEARBY_SHARE_CLIENT_H_
#define CHROME_BROWSER_NEARBY_SHARING_CLIENT_NEARBY_SHARE_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_http_result.h"

namespace nearbyshare {
namespace proto {
class ListContactPeopleRequest;
class ListContactPeopleResponse;
class ListPublicCertificatesRequest;
class ListPublicCertificatesResponse;
class UpdateDeviceRequest;
class UpdateDeviceResponse;
}  // namespace proto
}  // namespace nearbyshare

// Interface for making API requests to the NearbyShare service, which
// manages certificates and provides access to contacts.
// Implementations shall only processes a single request, so create a new
// instance for each request you make. DO NOT REUSE.
class NearbyShareClient {
 public:
  using ErrorCallback = base::OnceCallback<void(NearbyShareHttpError)>;
  using ListContactPeopleCallback = base::OnceCallback<void(
      const nearbyshare::proto::ListContactPeopleResponse&)>;
  using ListPublicCertificatesCallback = base::OnceCallback<void(
      const nearbyshare::proto::ListPublicCertificatesResponse&)>;
  using UpdateDeviceCallback =
      base::OnceCallback<void(const nearbyshare::proto::UpdateDeviceResponse&)>;

  NearbyShareClient() = default;
  virtual ~NearbyShareClient() = default;

  // NearbyShareService v1: UpdateDevice
  virtual void UpdateDevice(
      const nearbyshare::proto::UpdateDeviceRequest& request,
      UpdateDeviceCallback&& callback,
      ErrorCallback&& error_callback) = 0;

  // NearbyShareService v1: ListContactPeople
  virtual void ListContactPeople(
      const nearbyshare::proto::ListContactPeopleRequest& request,
      ListContactPeopleCallback&& callback,
      ErrorCallback&& error_callback) = 0;

  // NearbyShareService v1: ListPublicCertificates
  virtual void ListPublicCertificates(
      const nearbyshare::proto::ListPublicCertificatesRequest& request,
      ListPublicCertificatesCallback&& callback,
      ErrorCallback&& error_callback) = 0;

  // Returns the access token used to make the request. If no request has been
  // made yet, this function will return an empty string.
  virtual std::string GetAccessTokenUsed() = 0;
};

// Interface for creating NearbyShareClient instances. Because each
// NearbyShareClient instance can only be used for one API call, a factory
// makes it easier to make multiple requests in sequence or in parallel.
class NearbyShareClientFactory {
 public:
  NearbyShareClientFactory() = default;
  virtual ~NearbyShareClientFactory() = default;

  virtual std::unique_ptr<NearbyShareClient> CreateInstance() = 0;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CLIENT_NEARBY_SHARE_CLIENT_H_

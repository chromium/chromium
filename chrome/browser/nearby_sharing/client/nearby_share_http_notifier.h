// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CLIENT_NEARBY_SHARE_HTTP_NOTIFIER_H_
#define CHROME_BROWSER_NEARBY_SHARING_CLIENT_NEARBY_SHARE_HTTP_NOTIFIER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/nearby_sharing/proto/certificate_rpc.pb.h"
#include "chrome/browser/nearby_sharing/proto/contact_rpc.pb.h"
#include "chrome/browser/nearby_sharing/proto/device_rpc.pb.h"

// Interface for passing HTTP Responses/Requests to observers, by passing
// instance of this class to each HTTP Client.
class NearbyShareHttpNotifier {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when HTTP RPC is made for request and responses.
    virtual void OnUpdateDeviceRequest(
        const nearbyshare::proto::UpdateDeviceRequest& request) = 0;
    virtual void OnUpdateDeviceResponse(
        const nearbyshare::proto::UpdateDeviceResponse& response) = 0;
    virtual void OnListContactPeopleRequest(
        const nearbyshare::proto::ListContactPeopleRequest& request) = 0;
    virtual void OnListContactPeopleResponse(
        const nearbyshare::proto::ListContactPeopleResponse& response) = 0;
    virtual void OnListPublicCertificatesRequest(
        const nearbyshare::proto::ListPublicCertificatesRequest& request) = 0;
    virtual void OnListPublicCertificatesResponse(
        const nearbyshare::proto::ListPublicCertificatesResponse& response) = 0;
  };

  NearbyShareHttpNotifier();
  NearbyShareHttpNotifier(const NearbyShareHttpNotifier&) = delete;
  NearbyShareHttpNotifier& operator=(const NearbyShareHttpNotifier&) = delete;
  ~NearbyShareHttpNotifier();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Sends |request| to all observers.
  void NotifyOfRequest(const nearbyshare::proto::UpdateDeviceRequest& request);
  void NotifyOfRequest(
      const nearbyshare::proto::ListContactPeopleRequest& request);
  void NotifyOfRequest(
      const nearbyshare::proto::ListPublicCertificatesRequest& request);

  // Sends |response| to all observers.
  void NotifyOfResponse(
      const nearbyshare::proto::UpdateDeviceResponse& response);
  void NotifyOfResponse(
      const nearbyshare::proto::ListContactPeopleResponse& response);
  void NotifyOfResponse(
      const nearbyshare::proto::ListPublicCertificatesResponse& response);

 private:
  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CLIENT_NEARBY_SHARE_HTTP_NOTIFIER_H_

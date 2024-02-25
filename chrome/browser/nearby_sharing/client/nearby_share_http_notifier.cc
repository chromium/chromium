// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/client/nearby_share_http_notifier.h"

NearbyShareHttpNotifier::NearbyShareHttpNotifier() = default;

NearbyShareHttpNotifier::~NearbyShareHttpNotifier() = default;

void NearbyShareHttpNotifier::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NearbyShareHttpNotifier::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void NearbyShareHttpNotifier::NotifyOfRequest(
    const nearby::sharing::proto::UpdateDeviceRequest& request) {
  for (auto& observer : observers_)
    observer.OnUpdateDeviceRequest(request);
}

void NearbyShareHttpNotifier::NotifyOfResponse(
    const nearby::sharing::proto::UpdateDeviceResponse& response) {
  for (auto& observer : observers_)
    observer.OnUpdateDeviceResponse(response);
}

void NearbyShareHttpNotifier::NotifyOfRequest(
    const nearby::sharing::proto::ListContactPeopleRequest& request) {
  for (auto& observer : observers_)
    observer.OnListContactPeopleRequest(request);
}

void NearbyShareHttpNotifier::NotifyOfResponse(
    const nearby::sharing::proto::ListContactPeopleResponse& response) {
  for (auto& observer : observers_)
    observer.OnListContactPeopleResponse(response);
}

void NearbyShareHttpNotifier::NotifyOfRequest(
    const nearby::sharing::proto::ListPublicCertificatesRequest& request) {
  for (auto& observer : observers_)
    observer.OnListPublicCertificatesRequest(request);
}

void NearbyShareHttpNotifier::NotifyOfResponse(
    const nearby::sharing::proto::ListPublicCertificatesResponse& response) {
  for (auto& observer : observers_)
    observer.OnListPublicCertificatesResponse(response);
}

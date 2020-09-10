// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager.h"

NearbyShareContactManager::NearbyShareContactManager() = default;

NearbyShareContactManager::~NearbyShareContactManager() = default;

void NearbyShareContactManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NearbyShareContactManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void NearbyShareContactManager::Start() {
  if (is_running_)
    return;

  is_running_ = true;
  OnStart();
}

void NearbyShareContactManager::Stop() {
  if (!is_running_)
    return;

  is_running_ = false;
  OnStop();
}

void NearbyShareContactManager::NotifyAllowlistChanged(
    bool were_contacts_added_to_allowlist,
    bool were_contacts_removed_from_allowlist) {
  for (auto& observer : observers_) {
    observer.OnAllowlistChanged(were_contacts_added_to_allowlist,
                                were_contacts_removed_from_allowlist);
  }
}

void NearbyShareContactManager::NotifyContactsDownloaded(
    const std::set<std::string>& allowed_contact_ids,
    const std::vector<nearbyshare::proto::ContactRecord>& contacts) {
  for (auto& observer : observers_) {
    observer.OnContactsDownloaded(allowed_contact_ids, contacts);
  }
}

void NearbyShareContactManager::NotifyContactsUploaded(
    bool did_contacts_change_since_last_upload) {
  for (auto& observer : observers_) {
    observer.OnContactsUploaded(did_contacts_change_since_last_upload);
  }
}

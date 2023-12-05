// Copyright 2020 The Chromium Authors
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

void NearbyShareContactManager::SetAllowedContacts(
    const std::vector<std::string>& allowed_contacts) {
  // This is mojo version of the call, but mojo doesn't support sets, so we
  // have to convert the vector to set.
  std::set<std::string> set(allowed_contacts.begin(), allowed_contacts.end());
  SetAllowedContacts(set);
}

void NearbyShareContactManager::NotifyContactsDownloaded(
    const std::set<std::string>& allowed_contact_ids,
    const std::vector<nearby::sharing::proto::ContactRecord>& contacts,
    uint32_t num_unreachable_contacts_filtered_out) {
  for (auto& observer : observers_) {
    observer.OnContactsDownloaded(allowed_contact_ids, contacts,
                                  num_unreachable_contacts_filtered_out);
  }
}

void NearbyShareContactManager::NotifyContactsUploaded(
    bool did_contacts_change_since_last_upload) {
  for (auto& observer : observers_) {
    observer.OnContactsUploaded(did_contacts_change_since_last_upload);
  }
}

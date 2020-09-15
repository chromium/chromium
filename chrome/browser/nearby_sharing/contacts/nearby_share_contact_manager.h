// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/optional.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"
#include "chrome/browser/ui/webui/nearby_share/public/mojom/nearby_share_settings.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

// The Nearby Share contacts manager interfaces with the Nearby server in the
// following ways:
//   1) The user's contacts are downloaded from People API, using the Nearby
//   server as a proxy.
//   2) All of the user's contacts are uploaded to Nearby server, along with an
//   indication of what contacts are allowed for selected-contacts visibility
//   mode. The Nearby server will distribute all-contacts and selected-contacts
//   visibility certificates accordingly. For privacy reasons, the Nearby server
//   needs to explicitly receive the list of contacts from the device instead of
//   pulling them directly from People API.
//
// All contact data and update notifications are conveyed via observer methods;
// the manager does not return data directly from function calls.
class NearbyShareContactManager : public nearby_share::mojom::ContactManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnAllowlistChanged(
        bool were_contacts_added_to_allowlist,
        bool were_contacts_removed_from_allowlist) = 0;
    virtual void OnContactsDownloaded(
        const std::set<std::string>& allowed_contact_ids,
        const std::vector<nearbyshare::proto::ContactRecord>& contacts) = 0;
    virtual void OnContactsUploaded(
        bool did_contacts_change_since_last_upload) = 0;
  };

  NearbyShareContactManager();
  ~NearbyShareContactManager() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Starts/Stops contact task scheduling.
  void Start();
  void Stop();
  bool is_running() { return is_running_; }

  // Makes RPC calls to check if the user's contact list has changed since the
  // last contact upload via the UpdateDevice RPC. If it changed or if
  // |only_download_if_changed| is false, the contact list is downloaded from
  // the server. The locally persisted list of allowed contacts is reconciled
  // with the newly downloaded contacts. These RPC calls are also scheduled
  // periodically. The results are sent to observers via OnContactsDownloaded().
  virtual void DownloadContacts(bool only_download_if_changed) = 0;

  // Assigns the set of contacts that the local device allows sharing with when
  // in selected-contacts visibility mode. (Note: This set is irrelevant for
  // all-contacts visibility mode.) The allowed contact list determines what
  // contacts receive the local device's "selected-contacts" visibility public
  // certificates. Changes to the allowlist will trigger an RPC call. Observers
  // are notified of any changes to the allowlist via OnAllowlistChanged().
  virtual void SetAllowedContacts(
      const std::set<std::string>& allowed_contact_ids) = 0;

  virtual void Bind(
      mojo::PendingReceiver<nearby_share::mojom::ContactManager> receiver) = 0;

  // nearby_share::mojom::ContactManager:
  void SetAllowedContacts(
      const std::vector<std::string>& allowed_contacts) override;
  // This prevents the mojo version of DownloadContacts from being hidden.
  using nearby_share::mojom::ContactManager::DownloadContacts;

 protected:
  virtual void OnStart() = 0;
  virtual void OnStop() = 0;

  void NotifyAllowlistChanged(bool were_contacts_added_to_allowlist,
                              bool were_contacts_removed_from_allowlist);
  void NotifyContactsDownloaded(
      const std::set<std::string>& allowed_contact_ids,
      const std::vector<nearbyshare::proto::ContactRecord>& contacts);
  void NotifyContactsUploaded(bool did_contacts_change_since_last_upload);

 private:
  bool is_running_ = false;
  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_REFRESHER_H_
#define CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_REFRESHER_H_

#include <map>
#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"
#include "content/public/browser/push_messaging_service.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom-forward.h"

// This class enables push subscription refreshes as defined in the docs:
// https://w3c.github.io/push-api/#subscription-refreshes
// The idea is to keep the refresh information of both new and old subscription
// in memory during the refresh process to be still able to receive messages
// through the old subscription after it was replaced by the new subscription.
class PushMessagingRefresher {
 public:
  PushMessagingRefresher();

  PushMessagingRefresher(const PushMessagingRefresher&) = delete;
  PushMessagingRefresher& operator=(const PushMessagingRefresher&) = delete;

  ~PushMessagingRefresher();

  // Return number of objects that are currently being refreshed
  size_t GetCount() const;

  // Register a new refresh pair with relevant information.
  void Refresh(PushMessagingAppIdentifier old_app_identifier,
               const std::string& new_app_id,
               const std::string& sender_id);

  // The subscription with the new app id was updated, new messages arriving
  // through the new subscription should be accepted now.
  void OnSubscriptionUpdated(const std::string& new_app_id);

  // Unsubscribe event happened for the old subscription. It is deleted in
  // the RefreshMap and notify all observers that the refresh process for
  // |app_id| has finished
  void OnUnsubscribed(const std::string& app_id);

  // If a new message arrives through an |app_id| that is associated with a
  // refresh, the old subscription needs to be deactivated.
  void GotMessageFrom(const std::string& app_id);

  // If a subscription was refreshed, we accept the old subscription for
  // a moment after refresh
  std::optional<PushMessagingAppIdentifier> FindActiveAppIdentifier(
      const std::string& app_id);

  base::WeakPtr<PushMessagingRefresher> GetWeakPtr();

  // Observer for Refresh status updates
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnOldSubscriptionExpired(const std::string& app_id,
                                          const std::string& sender_id) = 0;
    virtual void OnRefreshFinished(
        const PushMessagingAppIdentifier& app_identifier) = 0;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // A RefreshObject carries subscription information that is needed to receive
  // messages and to unsubscribe from the old subscription
  struct RefreshObject {
    PushMessagingAppIdentifier old_identifier;
    std::string sender_id;
    bool is_valid;
  };

  void NotifyOnOldSubscriptionExpired(const std::string& app_id,
                                      const std::string& sender_id);

  base::ObserverList<Observer> observers_;

  // Maps from new app id to the refresh information of the old subscription
  // that is needed to receive messages and unsubscribe
  using RefreshInfo = std::map<std::string, RefreshObject>;
  RefreshInfo old_subscriptions_;

  // Maps from old app id to new app id
  using RefreshMap = std::map<std::string, std::string>;
  RefreshMap refresh_map_;

  base::WeakPtrFactory<PushMessagingRefresher> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_REFRESHER_H_

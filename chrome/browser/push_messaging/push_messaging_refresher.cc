// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_refresher.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"
#include "chrome/browser/push_messaging/push_messaging_constants.h"
#include "chrome/browser/push_messaging/push_messaging_service_impl.h"
#include "chrome/browser/push_messaging/push_messaging_utils.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/push_messaging_service.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom.h"
#include "url/gurl.h"

PushMessagingRefresher::PushMessagingRefresher() = default;

PushMessagingRefresher::~PushMessagingRefresher() = default;

size_t PushMessagingRefresher::GetCount() const {
  return old_subscriptions_.size();
}

void PushMessagingRefresher::Refresh(
    PushMessagingAppIdentifier old_app_identifier,
    const std::string& new_app_id,
    const std::string& sender_id) {
  RefreshObject refresh_object = {old_app_identifier, sender_id,
                                  false /* is_valid */};
  // Insert is as current started refresh
  old_subscriptions_.emplace(new_app_id, refresh_object);
  refresh_map_.emplace(old_app_identifier.app_id(), new_app_id);
  // TODO(viviy): Save old_subscription in a seperate map in preferences, so
  // that in case of a browser shutdown the subscription is remembered.
  // Unsubscribe on next startup.
}

void PushMessagingRefresher::OnSubscriptionUpdated(
    const std::string& new_app_id) {
  RefreshInfo::iterator result = old_subscriptions_.find(new_app_id);

  if (result == old_subscriptions_.end())
    return;

  // Schedule a unsubscription event for the old subscription
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PushMessagingRefresher::NotifyOnOldSubscriptionExpired,
                     weak_factory_.GetWeakPtr(),
                     result->second.old_identifier.app_id(),
                     result->second.sender_id),
      kPushSubscriptionRefreshTimeDelta);
}

void PushMessagingRefresher::NotifyOnOldSubscriptionExpired(
    const std::string& old_app_id,
    const std::string& sender_id) {
  for (Observer& obs : observers_)
    obs.OnOldSubscriptionExpired(old_app_id, sender_id);
}

void PushMessagingRefresher::OnUnsubscribed(const std::string& old_app_id) {
  auto found_new_app_id = refresh_map_.find(old_app_id);
  // Already unsubscribed
  if (found_new_app_id == refresh_map_.end())
    return;

  std::string new_app_id = found_new_app_id->second;
  refresh_map_.erase(found_new_app_id);

  RefreshInfo::iterator result = old_subscriptions_.find(new_app_id);
  CHECK(result != old_subscriptions_.end(), base::NotFatalUntil::M130);

  PushMessagingAppIdentifier old_identifier = result->second.old_identifier;
  old_subscriptions_.erase(result);

  for (Observer& obs : observers_)
    obs.OnRefreshFinished(old_identifier);
}

void PushMessagingRefresher::GotMessageFrom(const std::string& app_id) {
  RefreshInfo::iterator result = old_subscriptions_.find(app_id);
  // If a message arrives that is part of the refresh, expire the old
  // subscription immediately
  if (result != old_subscriptions_.end() && !result->second.is_valid) {
    NotifyOnOldSubscriptionExpired(result->second.old_identifier.app_id(),
                                   result->second.sender_id);
    result->second.is_valid = true;
  }
}

std::optional<PushMessagingAppIdentifier>
PushMessagingRefresher::FindActiveAppIdentifier(const std::string& app_id) {
  std::optional<PushMessagingAppIdentifier> app_identifier;
  RefreshMap::iterator refresh_map_it = refresh_map_.find(app_id);
  if (refresh_map_it != refresh_map_.end()) {
    RefreshInfo::iterator result =
        old_subscriptions_.find(refresh_map_it->second);
    if (result != old_subscriptions_.end() && !result->second.is_valid) {
      app_identifier = result->second.old_identifier;
    }
  }
  return app_identifier;
}

base::WeakPtr<PushMessagingRefresher> PushMessagingRefresher::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PushMessagingRefresher::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PushMessagingRefresher::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

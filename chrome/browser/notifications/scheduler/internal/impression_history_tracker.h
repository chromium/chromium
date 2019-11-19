// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_IMPRESSION_HISTORY_TRACKER_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_IMPRESSION_HISTORY_TRACKER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/scheduler/internal/collection_store.h"
#include "chrome/browser/notifications/scheduler/internal/impression_types.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"
#include "chrome/browser/notifications/scheduler/public/impression_detail.h"
#include "chrome/browser/notifications/scheduler/public/user_action_handler.h"

namespace notifications {

// Provides functionalities to update notification impression history and adjust
// maximum daily notification shown to the user.
class ImpressionHistoryTracker : public UserActionHandler {
 public:
  using ClientStates =
      std::map<SchedulerClientType, std::unique_ptr<ClientState>>;
  using InitCallback = base::OnceCallback<void(bool)>;

  // Initializes the impression tracker.
  virtual void Init(InitCallback callback) = 0;

  // Add a new impression, called after the notification is shown.
  virtual void AddImpression(
      SchedulerClientType type,
      const std::string& guid,
      const Impression::ImpressionResultMap& impression_map,
      const Impression::CustomData& custom_data,
      const base::Optional<base::TimeDelta>& custom_suppression_duration) = 0;

  // Analyzes the impression history for all notification clients, and adjusts
  // the |current_max_daily_show|.
  virtual void AnalyzeImpressionHistory() = 0;

  // Queries the client states.
  virtual void GetClientStates(
      std::map<SchedulerClientType, const ClientState*>* client_states)
      const = 0;

  // Queries impression based on guid, returns nullptr if no impression is
  // found.
  virtual const Impression* GetImpression(const std::string& guid) const = 0;

  // Queries the impression detail of a given |SchedulerClientType|.
  virtual void GetImpressionDetail(
      SchedulerClientType type,
      ImpressionDetail::ImpressionDetailCallback callback) = 0;

  virtual ~ImpressionHistoryTracker() = default;

 protected:
  ImpressionHistoryTracker() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImpressionHistoryTracker);
};

// An implementation of ImpressionHistoryTracker backed by a database.
class ImpressionHistoryTrackerImpl : public ImpressionHistoryTracker {
 public:
  explicit ImpressionHistoryTrackerImpl(
      const SchedulerConfig& config,
      std::vector<SchedulerClientType> registered_clients,
      std::unique_ptr<CollectionStore<ClientState>> store,
      base::Clock* clock);
  ~ImpressionHistoryTrackerImpl() override;

 private:
  // ImpressionHistoryTracker implementation.
  void Init(InitCallback callback) override;
  void AddImpression(SchedulerClientType type,
                     const std::string& guid,
                     const Impression::ImpressionResultMap& impression_mapping,
                     const Impression::CustomData& custom_data,
                     const base::Optional<base::TimeDelta>&
                         custom_suppression_duration) override;
  void AnalyzeImpressionHistory() override;
  void GetClientStates(std::map<SchedulerClientType, const ClientState*>*
                           client_states) const override;
  const Impression* GetImpression(const std::string& guid) const override;
  void GetImpressionDetail(
      SchedulerClientType type,
      ImpressionDetail::ImpressionDetailCallback callback) override;
  void OnUserAction(const UserActionData& action_data) override;

  // Called after |store_| is initialized.
  void OnStoreInitialized(InitCallback callback,
                          bool success,
                          CollectionStore<ClientState>::Entries entries);

  // Sync with registered clients. Adds new data for new client and deletes data
  // for deprecated client.
  void SyncRegisteredClients();

  // Helper method to prune impressions created before |start_time|. Assumes
  // |impressions| are sorted by creation time.
  static void PruneImpressionByCreateTime(
      base::circular_deque<Impression*>* impressions,
      const base::Time& start_time);

  // Analyzes the impression history for a particular client.
  void AnalyzeImpressionHistory(ClientState* client_state);

  // Check consecutive user actions, and generate impression result if no less
  // than |num_actions| count of user actions.
  void CheckConsecutiveDismiss(ClientState* client_state,
                               base::circular_deque<Impression*>* impressions,
                               size_t num_actions);

  // Generates user impression result.
  void GenerateImpressionResult(Impression* impression);

  // Updates notification throttling based on the impression result.
  void UpdateThrottling(ClientState* client_state, Impression* impression);

  // Applies a positive impression result to this notification type.
  void ApplyPositiveImpression(ClientState* client_state,
                               Impression* impression);

  // Applies one negative impression.
  void ApplyNegativeImpression(ClientState* client_state,
                               Impression* impression);

  // Checks if suppression is expired and recover to a certain daily quota.
  void CheckSuppressionExpiration(ClientState* client_state);

  // Tries to update the database records for |type|. Returns whether the db is
  // actually updated.
  bool MaybeUpdateDb(SchedulerClientType type);
  bool MaybeUpdateAllDb();

  // Sets/Gets the flag if impression data for |type| needs update in the
  // database.
  void SetNeedsUpdate(SchedulerClientType type, bool needs_update);
  bool NeedsUpdate(SchedulerClientType type) const;

  // Finds an impression that needs to update based on notification id.
  Impression* FindImpressionNeedsUpdate(const std::string& notification_guid);

  void OnClickInternal(const std::string& notification_guid, bool update_db);
  void OnButtonClickInternal(const std::string& notification_guid,
                             ActionButtonType button_type,
                             bool update_db);
  void OnDismissInternal(const std::string& notification_guid, bool update_db);

  // Impression history and global states for all notification scheduler
  // clients.
  ClientStates client_states_;

  // Notification guid to Impression map.
  // TODO(xingliu): Consider to remove this.
  std::map<std::string, Impression*> impression_map_;

  // The storage that persists data.
  std::unique_ptr<CollectionStore<ClientState>> store_;

  // System configuration.
  const SchedulerConfig& config_;

  const std::vector<SchedulerClientType> registered_clients_;

  // Whether the impression tracker is successfully initialized.
  bool initialized_;

  // If the database needs an update when any of the impression data is updated.
  std::map<SchedulerClientType, bool> need_update_db_;

  // The clock to provide the current timestamp.
  base::Clock* clock_;

  base::WeakPtrFactory<ImpressionHistoryTrackerImpl> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ImpressionHistoryTrackerImpl);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_IMPRESSION_HISTORY_TRACKER_H_

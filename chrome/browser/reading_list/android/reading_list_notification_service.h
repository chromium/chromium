// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_NOTIFICATION_SERVICE_H_

#include <memory>
#include <queue>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/reading_list/core/reading_list_model_observer.h"

namespace notifications {
class NotificationScheduleService;
struct ClientOverview;
struct NotificationData;
}  // namespace notifications

class ReadingListModel;
class ReadingListNotificationDelegate;

// Handles reading list weekly notification logic.
class ReadingListNotificationService : public KeyedService {
 public:
  using NotificationDataCallback = base::OnceCallback<void(
      std::unique_ptr<notifications::NotificationData>)>;

  // Configuration related to the reading list notification.
  struct Config {
    Config();
    ~Config();

    // Local time that the weekly notification will be triggered.
    int notification_show_time = 8;
  };

  // Returns whether the reading list notification is enabled.
  static bool IsEnabled();

  ReadingListNotificationService() = default;
  ~ReadingListNotificationService() override = default;
  ReadingListNotificationService(const ReadingListNotificationService&) =
      delete;
  ReadingListNotificationService& operator=(
      const ReadingListNotificationService&) = delete;

  // Called when Chrome starts.
  virtual void OnStart() = 0;

  // Called before the notification is shown. Update the number of unread pages.
  virtual void BeforeShowNotification(
      std::unique_ptr<notifications::NotificationData> notification_data,
      NotificationDataCallback callback) = 0;

  // Called when the notification is clicked.
  virtual void OnClick() = 0;
};

// Implementation of ReadingListNotificationService.
class ReadingListNotificationServiceImpl
    : public ReadingListNotificationService,
      public ReadingListModelObserver {
 public:
  ReadingListNotificationServiceImpl(
      ReadingListModel* reading_list_model,
      notifications::NotificationScheduleService* notification_scheduler,
      std::unique_ptr<ReadingListNotificationDelegate> delegate,
      std::unique_ptr<Config> config,
      base::Clock* clock);
  ~ReadingListNotificationServiceImpl() override;

  // ReadingListNotificationService implementation.
  void OnStart() override;
  void BeforeShowNotification(
      std::unique_ptr<notifications::NotificationData> notification_data,
      NotificationDataCallback callback) override;
  void OnClick() override;

  // ReadingListModelObserver implementation.
  void ReadingListModelLoaded(const ReadingListModel* model) override;

  std::queue<base::OnceClosure>* GetCachedClosureForTesting();

 private:
  // Calls the |closure| right away or waits for |reading_list_model_| loaded.
  void CallWhenModelLoaded(base::OnceClosure closure);

  // Schedules the weekly notification that shows the total number of unread
  // pages.
  void MaybeScheduleNotification();
  void OnClientOverview(size_t unread_count,
                        notifications::ClientOverview overview);
  void ScheduleNotification(int unread_count);

  // Gets the number of unread reading list articles.
  size_t ReadingListUnreadSize();

  // Gets the next notification show time.
  base::Time GetShowTime() const;

  raw_ptr<ReadingListModel> reading_list_model_;
  raw_ptr<notifications::NotificationScheduleService> notification_scheduler_;
  std::unique_ptr<ReadingListNotificationDelegate> delegate_;

  // Closures cached by MaybeCallSoon().
  std::queue<base::OnceClosure> cached_closures_;
  std::unique_ptr<Config> config_;
  raw_ptr<base::Clock> clock_;

  base::WeakPtrFactory<ReadingListNotificationServiceImpl> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_NOTIFICATION_SERVICE_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_ONE_TIME_PERMISSIONS_TRACKER_H_
#define CHROME_BROWSER_PERMISSIONS_ONE_TIME_PERMISSIONS_TRACKER_H_

#include <map>
#include <set>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/permissions/one_time_permissions_condition_tracker.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_observer.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/origin.h"

namespace base {
class SequencedTaskRunner;
}

// This observable class keeps track of one-time permission related browsing
// states.
class OneTimePermissionsTracker : public KeyedService {
  using NotifyFunction =
      void (OneTimePermissionsTracker::*)(const url::Origin&);

 public:
  OneTimePermissionsTracker();
  ~OneTimePermissionsTracker() override;

  OneTimePermissionsTracker(const OneTimePermissionsTracker&) = delete;
  OneTimePermissionsTracker& operator=(const OneTimePermissionsTracker&) =
      delete;

  class Condition {
   public:
    virtual ~Condition() = default;
  };

  base::WeakPtr<OneTimePermissionsTracker> GetWeakPtr();

  std::unique_ptr<Condition> NewActivePage(const url::Origin& origin);
  std::unique_ptr<Condition> NewForegroundPage(const url::Origin& origin);
  // Adds observer implementing `OneTimePermissionsTrackerObserver`.
  void AddObserver(OneTimePermissionsTrackerObserver* observer);

  // Removes observer implementing `OneTimePermissionsTrackerObserver`.
  void RemoveObserver(OneTimePermissionsTrackerObserver* observer);

  std::unique_ptr<Condition> NewVideoCapturing(const url::Origin& origin);
  std::unique_ptr<Condition> NewAudioCapturing(const url::Origin& origin);

  void Shutdown() override;

  void NotifyLastPageFromOriginClosed(const url::Origin& origin);

  void SetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

 protected:
  void NotifyBackgroundTimerExpired(
      const OneTimePermissionsTrackerObserver::BackgroundExpiryType&
          expiry_type,
      const url::Origin& origin);

 private:
  void NotifyCapturingVideoExpired(const url::Origin& origin);
  void NotifyCapturingAudioExpired(const url::Origin& origin);

  base::ObserverList<OneTimePermissionsTrackerObserver> observer_list_;

  std::unique_ptr<OneTimePermissionsConditionTracker::Factory>
      active_page_tracker_factory_;
  std::unique_ptr<OneTimePermissionsConditionTracker::Factory>
      short_background_page_tracker_factory_;
  std::unique_ptr<OneTimePermissionsConditionTracker::Factory>
      long_background_page_tracker_factory_;
  std::unique_ptr<OneTimePermissionsConditionTracker::Factory>
      video_capturing_tracker_factory_;
  std::unique_ptr<OneTimePermissionsConditionTracker::Factory>
      audio_capturing_tracker_factory_;

  base::WeakPtrFactory<OneTimePermissionsTracker> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PERMISSIONS_ONE_TIME_PERMISSIONS_TRACKER_H_

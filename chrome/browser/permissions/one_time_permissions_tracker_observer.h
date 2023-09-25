// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_ONE_TIME_PERMISSIONS_TRACKER_OBSERVER_H_
#define CHROME_BROWSER_PERMISSIONS_ONE_TIME_PERMISSIONS_TRACKER_OBSERVER_H_

#include "base/observer_list_types.h"
#include "url/origin.h"

class OneTimePermissionsTrackerObserver : public base::CheckedObserver {
 public:
  // The value of this enum is used to determine which timer is stopped on
  // `NotifyBackgroundTimerExpired`, for the given timeout duration.
  enum class BackgroundExpiryType { kTimeout, kLongTimeout };

  // Event fired when the last tab in a given Profile whose top-level document
  // is from |origin| is closed or navigated away.
  virtual void OnLastPageFromOriginClosed(const url::Origin&) {}

  // Event fired when all tabs in a given Profile whose top-level document is
  // from `origin` have been discarded or have been in the backgrounded based
  // on the `OneTimePermissionTrackerObserver::BackgroundExpiryType` enum
  // value.
  virtual void OnAllTabsInBackgroundTimerExpired(
      const url::Origin& origin,
      const BackgroundExpiryType& expiry_type) {}

  // Event fired when one time permission for an origin's camera permission
  // should be expired.
  virtual void OnCapturingVideoExpired(const url::Origin&) {}

  // Event fired when one time permission for an origin's microphone permission
  // should be expired.
  virtual void OnCapturingAudioExpired(const url::Origin&) {}

  // Event fired to let the observers know that the BrowserContext is going to
  // shut down.
  // The observers don't need to take care of removing themselves as an
  // observer.
  virtual void OnShutdown() {}
};

#endif  // CHROME_BROWSER_PERMISSIONS_ONE_TIME_PERMISSIONS_TRACKER_OBSERVER_H_

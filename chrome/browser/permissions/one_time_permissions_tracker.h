// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_ONE_TIME_PERMISSIONS_TRACKER_H_
#define CHROME_BROWSER_PERMISSIONS_ONE_TIME_PERMISSIONS_TRACKER_H_

#include <map>
#include <set>

#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_observer.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/visibility.h"
#include "url/origin.h"

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

  // Handles primary page changes to `origin` and pages of `origin` being
  // undiscarded.
  void WebContentsLoadedOrigin(const url::Origin& origin);

  // Handles primary page changes from `origin`, pages of `origin` getting
  // discarded and other WebContent destroy events.
  void WebContentsUnloadedOrigin(const url::Origin& origin);

  // Adds observer implementing `OneTimePermissionsTrackerObserver`.
  void AddObserver(OneTimePermissionsTrackerObserver* observer);

  // Removes observer implementing `OneTimePermissionsTrackerObserver`.
  void RemoveObserver(OneTimePermissionsTrackerObserver* observer);

  // Handles a WebContents visibility changes to `HIDDEN`.
  void WebContentsBackgrounded(const url::Origin& origin);

  // Handles a WebContents visibility changes to `OCCLUDED` or `VISIBLE`
  void WebContentsUnbackgrounded(const url::Origin& origin);

  // Handles changes in video capturing state.
  void CapturingVideoChanged(const url::Origin& origin,
                             bool is_capturing_video);

  // Handles changes in audio capturing state.
  void CapturingAudioChanged(const url::Origin& origin,
                             bool is_capturing_audio);

  void Shutdown() override;

  // When the provider expires content settings, this function clears the
  // associated state in the tracker. This prevents unnecessary calls to the
  // provider for already expired content settings.
  void CleanupStateForExpiredContentSetting(
      ContentSettingsType type,
      ContentSettingsPattern primary_pattern,
      ContentSettingsPattern secondary_pattern);

  // Fires all running timers for testing purposes.
  void FireRunningTimersForTesting();

 protected:
  void NotifyLastPageFromOriginClosed(const url::Origin& origin);

 private:
  // Struct to hold the state of an origin
  struct OriginTrackEntry {
    OriginTrackEntry();
    ~OriginTrackEntry();

    // Tracks how many tabs of this origin are open and undiscarded at any
    // given time.
    int undiscarded_tab_counter = 0;

    // Tracks how many tabs of this origin are in the background.
    // Background is defined as either hidden or minimized.
    int background_tab_counter = 0;

    // Tracks how many active permission uses for a specific content setting
    // for this origin are in progress. Currently only used for camera
    // and microphone permissions.
    std::map<ContentSettingsType, int> content_setting_specific_counter_map;

    // Keeps track of which user-media one-time content settings have been used
    // for this origin.
    std::set<ContentSettingsType> used_content_settings_set;

    // One shot timer for expiring permissions that are temporarily disabled by
    // backgrounding. This is intentionally not merged with
    // `content_setting_specific_expiration_timer_map`, which is used by
    // permissions that aren't disabled by backgrounding.
    std::unique_ptr<base::OneShotTimer> background_expiration_timer =
        std::make_unique<base::OneShotTimer>();

    // One shot timer for expiring permissions that are temporarily disabled by
    // backgrounding. This timer is only used in the File System Access
    // Persistent Permissions implementation to detect tab backgrounding events.
    std::unique_ptr<base::OneShotTimer> background_expiration_long_timer =
        std::make_unique<base::OneShotTimer>();

    // One shot timer for user-media one-time permissions for this origin.
    std::map<ContentSettingsType, std::unique_ptr<base::OneShotTimer>>
        content_setting_specific_expiration_timer_map;
  };

  bool ShouldIgnoreOrigin(const url::Origin& origin);
  bool AreAllTabsToOriginBackgroundedOrDiscarded(const url::Origin& origin);
  void RemoveContentSettingUsedFromOrigin(const url::Origin& origin,
                                          ContentSettingsType content_setting);

  void HandleUserMediaState(const url::Origin& origin,
                            ContentSettingsType content_setting);

  void StartContentSpecificExpirationTimer(const url::Origin& origin,
                                           ContentSettingsType content_setting,
                                           NotifyFunction notify_callback);

  void NotifyBackgroundTimerExpired(
      const url::Origin& origin,
      const OneTimePermissionsTrackerObserver::BackgroundExpiryType&
          expiry_type);
  void NotifyCapturingVideoExpired(const url::Origin& origin);
  void NotifyCapturingAudioExpired(const url::Origin& origin);

  base::ObserverList<OneTimePermissionsTrackerObserver> observer_list_;

  std::map<url::Origin, OriginTrackEntry> origin_tracker_;

  base::WeakPtrFactory<OneTimePermissionsTracker> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PERMISSIONS_ONE_TIME_PERMISSIONS_TRACKER_H_

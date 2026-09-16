// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/one_time_permissions_tracker.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/permissions/one_time_permissions_condition_tracker.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/content_setting_permission_context_base.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/visibility.h"
#include "url/gurl.h"

namespace {

class ActivePageCondition : public OneTimePermissionsTracker::Condition {
 public:
  explicit ActivePageCondition(scoped_refptr<OneTimePermissionsConditionTracker>
                                   internal_active_page_tracker)
      : internal_active_page_tracker_(std::move(internal_active_page_tracker)) {
  }

 private:
  scoped_refptr<OneTimePermissionsConditionTracker>
      internal_active_page_tracker_;
};

class ForegroundPageCondition : public OneTimePermissionsTracker::Condition {
 public:
  ForegroundPageCondition(
      scoped_refptr<OneTimePermissionsConditionTracker> short_condition_tracker,
      scoped_refptr<OneTimePermissionsConditionTracker> long_condition_tracker)
      : short_condition_tracker_(std::move(short_condition_tracker)),
        long_condition_tracker_(std::move(long_condition_tracker)) {}

 private:
  scoped_refptr<OneTimePermissionsConditionTracker> short_condition_tracker_;
  scoped_refptr<OneTimePermissionsConditionTracker> long_condition_tracker_;
};

}  // namespace

OneTimePermissionsTracker::OneTimePermissionsTracker() {
  active_page_tracker_factory_ =
      std::make_unique<OneTimePermissionsConditionTracker::Factory>(
          base::BindRepeating(
              &OneTimePermissionsTracker::NotifyLastPageFromOriginClosed,
              weak_factory_.GetWeakPtr()),
          base::Seconds(0));
  short_background_page_tracker_factory_ = std::make_unique<
      OneTimePermissionsConditionTracker::Factory>(
      base::BindRepeating(
          &OneTimePermissionsTracker::NotifyBackgroundTimerExpired,
          weak_factory_.GetWeakPtr(),
          OneTimePermissionsTrackerObserver::BackgroundExpiryType::kTimeout),
      permissions::kOneTimePermissionTimeout);
  long_background_page_tracker_factory_ =
      std::make_unique<OneTimePermissionsConditionTracker::Factory>(
          base::BindRepeating(
              &OneTimePermissionsTracker::NotifyBackgroundTimerExpired,
              weak_factory_.GetWeakPtr(),
              OneTimePermissionsTrackerObserver::BackgroundExpiryType::
                  kLongTimeout),
          permissions::kOneTimePermissionMaximumLifetime);
}

OneTimePermissionsTracker::~OneTimePermissionsTracker() = default;

base::WeakPtr<OneTimePermissionsTracker>
OneTimePermissionsTracker::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::unique_ptr<OneTimePermissionsTracker::Condition>
OneTimePermissionsTracker::NewActivePage(const url::Origin& origin) {
  return std::make_unique<ActivePageCondition>(
      active_page_tracker_factory_->New(origin));
}

std::unique_ptr<OneTimePermissionsTracker::Condition>
OneTimePermissionsTracker::NewForegroundPage(const url::Origin& origin) {
  return std::make_unique<ForegroundPageCondition>(
      short_background_page_tracker_factory_->New(origin),
      long_background_page_tracker_factory_->New(origin));
}

OneTimePermissionsTracker::OriginTrackEntry::OriginTrackEntry() = default;

OneTimePermissionsTracker::OriginTrackEntry::~OriginTrackEntry() = default;

void OneTimePermissionsTracker::Shutdown() {
  for (auto& observer : observer_list_) {
    observer.OnShutdown();
  }
  observer_list_.Clear();
  active_page_tracker_factory_.reset();
  short_background_page_tracker_factory_.reset();
  long_background_page_tracker_factory_.reset();
}

void OneTimePermissionsTracker::AddObserver(
    OneTimePermissionsTrackerObserver* observer) {
  observer_list_.AddObserver(observer);
}

void OneTimePermissionsTracker::RemoveObserver(
    OneTimePermissionsTrackerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void OneTimePermissionsTracker::WebContentsBackgrounded(
    const url::Origin& origin) {
  // For some reason using `origin_tracker_[origin].background_tab_counter++;`
  // on some builds leaves the value of
  // `origin_tracker_[origin].background_tab_counter` at 0 in case of
  // insertion. My best efforts to understand it have failed. Hence, `+=` is
  // necessary here for the feature to work at all there.
  origin_tracker_[origin].background_tab_counter += 1;

  if (AreAllTabsToOriginBackgroundedOrDiscarded(origin)) {
    HandleUserMediaState(origin, ContentSettingsType::MEDIASTREAM_CAMERA);
    HandleUserMediaState(origin, ContentSettingsType::MEDIASTREAM_MIC);
  }
}

void OneTimePermissionsTracker::WebContentsUnbackgrounded(
    const url::Origin& origin) {
  origin_tracker_[origin].background_tab_counter--;
}

void OneTimePermissionsTracker::WebContentsLoadedOrigin(
    const url::Origin& origin) {
  origin_tracker_[origin].undiscarded_tab_counter++;
}

void OneTimePermissionsTracker::WebContentsUnloadedOrigin(
    const url::Origin& origin) {
  origin_tracker_[origin].undiscarded_tab_counter--;
  DCHECK(!(origin_tracker_[origin].undiscarded_tab_counter < 0));
  if (AreAllTabsToOriginBackgroundedOrDiscarded(origin)) {
    HandleUserMediaState(origin, ContentSettingsType::MEDIASTREAM_CAMERA);
    HandleUserMediaState(origin, ContentSettingsType::MEDIASTREAM_MIC);
  }
}

void OneTimePermissionsTracker::StartContentSpecificExpirationTimer(
    const url::Origin& origin,
    ContentSettingsType content_setting,
    NotifyFunction notify_callback) {
  origin_tracker_[origin].used_content_settings_set.insert(content_setting);
  origin_tracker_[origin]
      .content_setting_specific_expiration_timer_map[content_setting]
      ->Start(
          FROM_HERE, permissions::kOneTimePermissionTimeout,
          base::BindOnce(notify_callback, weak_factory_.GetWeakPtr(), origin));
}

void OneTimePermissionsTracker::HandleUserMediaState(
    const url::Origin& origin,
    ContentSettingsType content_setting) {
  NotifyFunction notify_callback;
  switch (content_setting) {
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      notify_callback = &OneTimePermissionsTracker::NotifyCapturingVideoExpired;
      break;
    case ContentSettingsType::MEDIASTREAM_MIC:
      notify_callback = &OneTimePermissionsTracker::NotifyCapturingAudioExpired;
      break;
    default:
      NOTREACHED();
  }

  if (origin_tracker_[origin].used_content_settings_set.find(content_setting) !=
      origin_tracker_[origin].used_content_settings_set.end()) {
    if (origin_tracker_[origin]
            .content_setting_specific_expiration_timer_map.find(
                content_setting) ==
        origin_tracker_[origin]
            .content_setting_specific_expiration_timer_map.end()) {
      origin_tracker_[origin]
          .content_setting_specific_expiration_timer_map[content_setting] =
          std::make_unique<base::OneShotTimer>();
    }

    if (origin_tracker_[origin]
                .content_setting_specific_counter_map[content_setting] == 0 &&
        AreAllTabsToOriginBackgroundedOrDiscarded(origin)) {
      StartContentSpecificExpirationTimer(origin, content_setting,
                                          notify_callback);
    } else {
      origin_tracker_[origin]
          .content_setting_specific_expiration_timer_map[content_setting]
          ->Stop();
    }
  }
}

void OneTimePermissionsTracker::CapturingVideoChanged(const url::Origin& origin,
                                                      bool is_capturing_video) {
  if (is_capturing_video &&
      origin_tracker_[origin].used_content_settings_set.find(
          ContentSettingsType::MEDIASTREAM_CAMERA) ==
          origin_tracker_[origin].used_content_settings_set.end()) {
    origin_tracker_[origin].used_content_settings_set.insert(
        ContentSettingsType::MEDIASTREAM_CAMERA);
  }

  origin_tracker_[origin].content_setting_specific_counter_map
      [ContentSettingsType::MEDIASTREAM_CAMERA] += is_capturing_video ? 1 : -1;
  HandleUserMediaState(origin, ContentSettingsType::MEDIASTREAM_CAMERA);
}

void OneTimePermissionsTracker::CapturingAudioChanged(const url::Origin& origin,
                                                      bool is_capturing_audio) {
  if (is_capturing_audio &&
      origin_tracker_[origin].used_content_settings_set.find(
          ContentSettingsType::MEDIASTREAM_MIC) ==
          origin_tracker_[origin].used_content_settings_set.end()) {
    origin_tracker_[origin].used_content_settings_set.insert(
        ContentSettingsType::MEDIASTREAM_MIC);
  }

  origin_tracker_[origin].content_setting_specific_counter_map
      [ContentSettingsType::MEDIASTREAM_MIC] += is_capturing_audio ? 1 : -1;
  HandleUserMediaState(origin, ContentSettingsType::MEDIASTREAM_MIC);
}

void OneTimePermissionsTracker::CleanupStateForExpiredContentSetting(
    ContentSettingsType type,
    ContentSettingsPattern primary_pattern,
    ContentSettingsPattern secondary_pattern) {
  std::vector<url::Origin> affected_origins;
  for (const auto& entry : origin_tracker_) {
    const GURL top_level_origin_as_gurl = entry.first.GetURL();
    if (primary_pattern.Matches(top_level_origin_as_gurl) &&
        secondary_pattern.Matches(top_level_origin_as_gurl)) {
      affected_origins.push_back(entry.first);
    }
  }

  for (const auto& origin : affected_origins) {
    origin_tracker_[origin].content_setting_specific_expiration_timer_map.erase(
        type);

    origin_tracker_[origin].content_setting_specific_counter_map.erase(type);
    origin_tracker_[origin].used_content_settings_set.erase(type);
  }
}

void OneTimePermissionsTracker::FireRunningTimersForTesting() {
  // The loops in this method require manual 'forward-looking' iteration because
  // the methods executing upon timer expiration might erase elements from the
  // map that is being iterated over.
  for (auto i_outer = origin_tracker_.begin(), e_outer = origin_tracker_.end();
       i_outer != e_outer;) {
    auto origin_entry = i_outer++;
    for (auto
             i_inner =
                 origin_entry->second
                     .content_setting_specific_expiration_timer_map.begin(),
             e_inner = origin_entry->second
                           .content_setting_specific_expiration_timer_map.end();
         i_inner != e_inner;) {
      auto timer_entry = i_inner++;
      if (timer_entry->second->IsRunning()) {
        timer_entry->second->FireNow();
      }
    }
  }
}

void OneTimePermissionsTracker::SetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  active_page_tracker_factory_->SetTaskRunnerForTesting(  // IN-TEST
      task_runner);
  short_background_page_tracker_factory_->SetTaskRunnerForTesting(  // IN-TEST
      task_runner);
  long_background_page_tracker_factory_->SetTaskRunnerForTesting(  // IN-TEST
      task_runner);
}

void OneTimePermissionsTracker::NotifyLastPageFromOriginClosed(
    const url::Origin& origin) {
  for (auto& observer : observer_list_) {
    observer.OnLastPageFromOriginClosed(origin);
  }
}

void OneTimePermissionsTracker::NotifyBackgroundTimerExpired(
    const OneTimePermissionsTrackerObserver::BackgroundExpiryType& expiry_type,
    const url::Origin& origin) {
  for (auto& observer : observer_list_) {
    observer.OnAllTabsInBackgroundTimerExpired(origin, expiry_type);
  }
}

void OneTimePermissionsTracker::NotifyCapturingVideoExpired(
    const url::Origin& origin) {
  for (auto& observer : observer_list_) {
    observer.OnCapturingVideoExpired(origin);
  }
}

void OneTimePermissionsTracker::NotifyCapturingAudioExpired(
    const url::Origin& origin) {
  for (auto& observer : observer_list_) {
    observer.OnCapturingAudioExpired(origin);
  }
}

bool OneTimePermissionsTracker::AreAllTabsToOriginBackgroundedOrDiscarded(
    const url::Origin& origin) {
  return origin_tracker_[origin].background_tab_counter ==
         origin_tracker_[origin].undiscarded_tab_counter;
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/one_time_permissions_tracker.h"

#include <array>
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

template <size_t size>
class MultipleTrackerCondition : public OneTimePermissionsTracker::Condition {
 public:
  template <typename... Args>
    requires(sizeof...(Args) == size)
  explicit MultipleTrackerCondition(Args&&... trackers)
      : trackers_{std::forward<Args>(trackers)...} {}

 private:
  std::array<scoped_refptr<OneTimePermissionsConditionTracker>, size> trackers_;
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
  video_capturing_tracker_factory_ =
      std::make_unique<OneTimePermissionsConditionTracker::Factory>(
          base::BindRepeating(
              &OneTimePermissionsTracker::NotifyCapturingVideoExpired,
              weak_factory_.GetWeakPtr()),
          permissions::kOneTimePermissionTimeout);
  audio_capturing_tracker_factory_ =
      std::make_unique<OneTimePermissionsConditionTracker::Factory>(
          base::BindRepeating(
              &OneTimePermissionsTracker::NotifyCapturingAudioExpired,
              weak_factory_.GetWeakPtr()),
          permissions::kOneTimePermissionTimeout);
}

OneTimePermissionsTracker::~OneTimePermissionsTracker() = default;

base::WeakPtr<OneTimePermissionsTracker>
OneTimePermissionsTracker::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::unique_ptr<OneTimePermissionsTracker::Condition>
OneTimePermissionsTracker::NewActivePage(const url::Origin& origin) {
  return std::make_unique<MultipleTrackerCondition<1>>(
      active_page_tracker_factory_->New(origin));
}

std::unique_ptr<OneTimePermissionsTracker::Condition>
OneTimePermissionsTracker::NewForegroundPage(const url::Origin& origin) {
  return std::make_unique<MultipleTrackerCondition<4>>(
      short_background_page_tracker_factory_->New(origin),
      long_background_page_tracker_factory_->New(origin),
      video_capturing_tracker_factory_->New(origin),
      audio_capturing_tracker_factory_->New(origin));
}

std::unique_ptr<OneTimePermissionsTracker::Condition>
OneTimePermissionsTracker::NewVideoCapturing(const url::Origin& origin) {
  return std::make_unique<MultipleTrackerCondition<1>>(
      video_capturing_tracker_factory_->New(origin));
}

std::unique_ptr<OneTimePermissionsTracker::Condition>
OneTimePermissionsTracker::NewAudioCapturing(const url::Origin& origin) {
  return std::make_unique<MultipleTrackerCondition<1>>(
      audio_capturing_tracker_factory_->New(origin));
}

void OneTimePermissionsTracker::SetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  short_background_page_tracker_factory_->SetTaskRunnerForTesting(  // IN-TEST
      task_runner);
  long_background_page_tracker_factory_->SetTaskRunnerForTesting(  // IN-TEST
      task_runner);
  video_capturing_tracker_factory_->SetTaskRunnerForTesting(  // IN-TEST
      task_runner);
  audio_capturing_tracker_factory_->SetTaskRunnerForTesting(  // IN-TEST
      task_runner);
}

void OneTimePermissionsTracker::Shutdown() {
  for (auto& observer : observer_list_) {
    observer.OnShutdown();
  }
  observer_list_.Clear();
  active_page_tracker_factory_.reset();
  short_background_page_tracker_factory_.reset();
  long_background_page_tracker_factory_.reset();
  video_capturing_tracker_factory_.reset();
  audio_capturing_tracker_factory_.reset();
}

void OneTimePermissionsTracker::AddObserver(
    OneTimePermissionsTrackerObserver* observer) {
  observer_list_.AddObserver(observer);
}

void OneTimePermissionsTracker::RemoveObserver(
    OneTimePermissionsTrackerObserver* observer) {
  observer_list_.RemoveObserver(observer);
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

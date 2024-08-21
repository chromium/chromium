// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/one_time_permissions_tracker_helper.h"

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/permissions/one_time_permissions_tracker.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_factory.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "content/public/browser/visibility.h"

OneTimePermissionsTrackerHelper::~OneTimePermissionsTrackerHelper() = default;

void OneTimePermissionsTrackerHelper::WebContentsDestroyed() {
  if (last_committed_origin_ && !web_contents()->WasDiscarded()) {
    auto* tracker = OneTimePermissionsTrackerFactory::GetForBrowserContext(
        web_contents()->GetBrowserContext());
    tracker->WebContentsUnloadedOrigin(*last_committed_origin_);
    if (web_contents()->GetVisibility() == content::Visibility::HIDDEN) {
      tracker->WebContentsUnbackgrounded(*last_committed_origin_);
    }
  }

  MediaCaptureDevicesDispatcher::GetInstance()
      ->GetMediaStreamCaptureIndicator()
      ->RemoveObserver(this);
}

void OneTimePermissionsTrackerHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  auto* tracker = OneTimePermissionsTrackerFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());
  const auto origin =
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  if (visibility != content::Visibility::HIDDEN) {
    tracker->WebContentsUnbackgrounded(origin);
  } else {
    tracker->WebContentsBackgrounded(origin);
  }

  last_visibility_ = std::move(visibility);
}

void OneTimePermissionsTrackerHelper::PrimaryPageChanged(content::Page& page) {
  url::Origin new_origin = page.GetMainDocument().GetLastCommittedOrigin();
  if (last_committed_origin_ && *last_committed_origin_ == new_origin) {
    return;
  }
  auto* tracker = OneTimePermissionsTrackerFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());

  if (last_committed_origin_) {
    tracker->WebContentsUnloadedOrigin(*last_committed_origin_);
  }

  if (web_contents()->GetVisibility() == content::Visibility::HIDDEN) {
    tracker->WebContentsBackgrounded(new_origin);
  }

  tracker->WebContentsLoadedOrigin(new_origin);
  last_committed_origin_ = std::move(new_origin);
}

void OneTimePermissionsTrackerHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (last_committed_origin_ && was_discarded_) {
    // If a new navigation has started, and the tab was previously discarded,
    // the tab has reactivated.
    auto* tracker = OneTimePermissionsTrackerFactory::GetForBrowserContext(
        web_contents()->GetBrowserContext());
    tracker->WebContentsLoadedOrigin(*last_committed_origin_);
  }
  was_discarded_ = false;
}

void OneTimePermissionsTrackerHelper::WasDiscarded() {
  if (last_committed_origin_) {
    auto* tracker = OneTimePermissionsTrackerFactory::GetForBrowserContext(
        web_contents()->GetBrowserContext());
    tracker->WebContentsUnloadedOrigin(*last_committed_origin_);
  }
  was_discarded_ = true;
}

void OneTimePermissionsTrackerHelper::OnIsCapturingVideoChanged(
    content::WebContents* web_contents,
    bool is_capturing_video) {
  if (last_committed_origin_.has_value() &&
      last_committed_origin_->IsSameOriginWith(
          web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin())) {
    OneTimePermissionsTrackerFactory::GetForBrowserContext(
        web_contents->GetBrowserContext())
        ->CapturingVideoChanged(
            web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
            is_capturing_video);
  }
}

void OneTimePermissionsTrackerHelper::OnIsCapturingAudioChanged(
    content::WebContents* web_contents,
    bool is_capturing_audio) {
  if (last_committed_origin_.has_value() &&
      last_committed_origin_->IsSameOriginWith(
          web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin())) {
    OneTimePermissionsTrackerFactory::GetForBrowserContext(
        web_contents->GetBrowserContext())
        ->CapturingAudioChanged(
            web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
            is_capturing_audio);
  }
}

OneTimePermissionsTrackerHelper::OneTimePermissionsTrackerHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<OneTimePermissionsTrackerHelper>(
          *web_contents) {
  MediaCaptureDevicesDispatcher::GetInstance()
      ->GetMediaStreamCaptureIndicator()
      ->AddObserver(this);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OneTimePermissionsTrackerHelper);

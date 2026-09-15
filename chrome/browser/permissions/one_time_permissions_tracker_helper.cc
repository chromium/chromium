// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/one_time_permissions_tracker_helper.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/permissions/one_time_permissions_tracker.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_factory.h"
#include "content/public/browser/page.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/visibility.h"

namespace {

// A helper class for tracking events relevant to OneTimePermissions expiration
// which are tied to a single Page.
class OneTimePermissionsPageTracker
    : public content::PageUserData<OneTimePermissionsPageTracker> {
 public:
  ~OneTimePermissionsPageTracker() override;

 private:
  explicit OneTimePermissionsPageTracker(content::Page& page);

  friend PageUserData;
  PAGE_USER_DATA_KEY_DECL();

  url::Origin origin_;
  base::WeakPtr<OneTimePermissionsTracker> tracker_;
};

PAGE_USER_DATA_KEY_IMPL(OneTimePermissionsPageTracker);

OneTimePermissionsPageTracker::OneTimePermissionsPageTracker(
    content::Page& page)
    : PageUserData(page),
      origin_(page.GetMainDocument().GetLastCommittedOrigin()) {
  auto* tracker = OneTimePermissionsTrackerFactory::GetForBrowserContext(
      page.GetMainDocument().GetBrowserContext());
  if (tracker) {
    tracker_ = tracker->GetWeakPtr();
    tracker_->WebContentsLoadedOrigin(origin_);
  }
}

OneTimePermissionsPageTracker::~OneTimePermissionsPageTracker() {
  // We call WebContentsUnloadedOrigin asynchronously to preserve one-time
  // grants on same-origin navigations (allowing for the
  // OneTimepermissionsPageTracker for the new page to be created before
  // WebContentsUnloadedOrigin runs).
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OneTimePermissionsTracker::WebContentsUnloadedOrigin,
                     tracker_, origin_));
}

}  // namespace

OneTimePermissionsTrackerHelper::~OneTimePermissionsTrackerHelper() = default;

void OneTimePermissionsTrackerHelper::WebContentsDestroyed() {
  if (last_committed_origin_) {
    auto* tracker = OneTimePermissionsTrackerFactory::GetForBrowserContext(
        web_contents()->GetBrowserContext());
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
  OneTimePermissionsPageTracker::CreateForPage(page);

  url::Origin new_origin = page.GetMainDocument().GetLastCommittedOrigin();
  if (last_committed_origin_ && *last_committed_origin_ == new_origin) {
    return;
  }
  auto* tracker = OneTimePermissionsTrackerFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());

  if (web_contents()->GetVisibility() == content::Visibility::HIDDEN) {
    tracker->WebContentsBackgrounded(new_origin);
  }

  last_committed_origin_ = std::move(new_origin);
}

void OneTimePermissionsTrackerHelper::PrimaryPageWillBeDeactivated(
    content::Page& page) {
  if (OneTimePermissionsPageTracker::GetForPage(page)) {
    OneTimePermissionsPageTracker::DeleteForPage(page);
  }
}

void OneTimePermissionsTrackerHelper::WasDiscarded() {
  if (web_contents()->WasDiscarded()) {
    if (OneTimePermissionsPageTracker::GetForPage(
            web_contents()->GetPrimaryPage())) {
      OneTimePermissionsPageTracker::DeleteForPage(
          web_contents()->GetPrimaryPage());
    }
  }
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

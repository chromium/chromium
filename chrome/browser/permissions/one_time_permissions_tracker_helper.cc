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
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

bool ShouldIgnoreOrigin(const url::Origin& origin) {
  // There are cases where chrome://newtab/ and chrome://new-tab-page/ are
  // used synonymously causing inconsistencies in the map. So we just ignore
  // them.
  return origin.opaque() ||
         origin == url::Origin::Create(GURL("chrome://newtab/")) ||
         origin == url::Origin::Create(GURL("chrome://new-tab-page/"));
}

// A helper class for tracking events relevant to OneTimePermissions expiration
// which are tied to a single Page.
class OneTimePermissionsPageTracker
    : public content::PageUserData<OneTimePermissionsPageTracker> {
 public:
  static void MaybeCreateForPage(content::Page& page) {
    if (ShouldIgnoreOrigin(page.GetMainDocument().GetLastCommittedOrigin())) {
      return;
    }
    CreateForPage(page);
  }

  ~OneTimePermissionsPageTracker() override;

  void OnVisibilityChanged(content::Visibility visibility);
  void OnIsCapturingVideoChanged(bool is_capturing_video);
  void OnIsCapturingAudioChanged(bool is_capturing_audio);

 private:
  explicit OneTimePermissionsPageTracker(content::Page& page);

  friend PageUserData;
  PAGE_USER_DATA_KEY_DECL();

  url::Origin origin_;
  base::WeakPtr<OneTimePermissionsTracker> tracker_;
  bool is_backgrounded_ = false;
  bool is_capturing_video_ = false;
  bool is_capturing_audio_ = false;
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
    if (content::WebContents::FromRenderFrameHost(&page.GetMainDocument())
            ->GetVisibility() == content::Visibility::HIDDEN) {
      is_backgrounded_ = true;
      tracker_->WebContentsBackgrounded(origin_);
    }
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
  if (tracker_) {
    if (is_capturing_video_) {
      tracker_->CapturingVideoChanged(origin_, false);
    }
    if (is_capturing_audio_) {
      tracker_->CapturingAudioChanged(origin_, false);
    }
    if (is_backgrounded_) {
      tracker_->WebContentsUnbackgrounded(origin_);
    }
  }
}

void OneTimePermissionsPageTracker::OnVisibilityChanged(
    content::Visibility visibility) {
  if (!tracker_) {
    return;
  }
  const bool is_hidden = (visibility == content::Visibility::HIDDEN);
  if (is_backgrounded_ == is_hidden) {
    return;
  }
  is_backgrounded_ = is_hidden;
  if (is_backgrounded_) {
    tracker_->WebContentsBackgrounded(origin_);
  } else {
    tracker_->WebContentsUnbackgrounded(origin_);
  }
}

void OneTimePermissionsPageTracker::OnIsCapturingVideoChanged(
    bool is_capturing_video) {
  if (is_capturing_video_ == is_capturing_video) {
    return;
  }
  is_capturing_video_ = is_capturing_video;
  if (tracker_) {
    tracker_->CapturingVideoChanged(origin_, is_capturing_video);
  }
}

void OneTimePermissionsPageTracker::OnIsCapturingAudioChanged(
    bool is_capturing_audio) {
  if (is_capturing_audio_ == is_capturing_audio) {
    return;
  }
  is_capturing_audio_ = is_capturing_audio;
  if (tracker_) {
    tracker_->CapturingAudioChanged(origin_, is_capturing_audio);
  }
}

}  // namespace

// static
bool OneTimePermissionsTrackerHelper::ShouldIgnoreOriginForTesting(
    const url::Origin& origin) {
  return ShouldIgnoreOrigin(origin);
}

OneTimePermissionsTrackerHelper::~OneTimePermissionsTrackerHelper() = default;

void OneTimePermissionsTrackerHelper::WebContentsDestroyed() {
  MediaCaptureDevicesDispatcher::GetInstance()
      ->GetMediaStreamCaptureIndicator()
      ->RemoveObserver(this);
}

void OneTimePermissionsTrackerHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  if (auto* tracker = OneTimePermissionsPageTracker::GetForPage(
          web_contents()->GetPrimaryPage())) {
    tracker->OnVisibilityChanged(visibility);
  }
}

void OneTimePermissionsTrackerHelper::PrimaryPageChanged(content::Page& page) {
  OneTimePermissionsPageTracker::MaybeCreateForPage(page);
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
  if (web_contents != this->web_contents()) {
    return;
  }
  if (auto* tracker = OneTimePermissionsPageTracker::GetForPage(
          web_contents->GetPrimaryPage())) {
    tracker->OnIsCapturingVideoChanged(is_capturing_video);
  }
}

void OneTimePermissionsTrackerHelper::OnIsCapturingAudioChanged(
    content::WebContents* web_contents,
    bool is_capturing_audio) {
  if (web_contents != this->web_contents()) {
    return;
  }
  if (auto* tracker = OneTimePermissionsPageTracker::GetForPage(
          web_contents->GetPrimaryPage())) {
    tracker->OnIsCapturingAudioChanged(is_capturing_audio);
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

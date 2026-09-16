// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/one_time_permissions_tracker_helper.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/permissions/one_time_permissions_condition_tracker.h"
#include "chrome/browser/permissions/one_time_permissions_tracker.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_factory.h"
#include "components/permissions/permission_util.h"
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
    // TODO(crbug.com/40226169): We should really use origins instead. Revisit
    // this when we fix the GURL vs Origin problem in
    // GetLastCommittedOriginAsURL.
    url::Origin origin = url::Origin::Create(
        permissions::PermissionUtil::GetLastCommittedOriginAsURL(
            &page.GetMainDocument()));
    if (ShouldIgnoreOrigin(origin)) {
      return;
    }
    CreateForPage(page, origin);
  }

  ~OneTimePermissionsPageTracker() override;

  void OnVisibilityChanged(content::Visibility visibility);
  void OnIsCapturingVideoChanged(bool is_capturing_video);
  void OnIsCapturingAudioChanged(bool is_capturing_audio);

 private:
  OneTimePermissionsPageTracker(content::Page& page, url::Origin origin);

  friend PageUserData;
  PAGE_USER_DATA_KEY_DECL();

  raw_ptr<OneTimePermissionsTracker> tracker_ = nullptr;
  url::Origin origin_;
  std::unique_ptr<OneTimePermissionsTracker::Condition> active_page_tracker_;
  std::unique_ptr<OneTimePermissionsTracker::Condition>
      foreground_page_tracker_;
  std::unique_ptr<OneTimePermissionsTracker::Condition>
      video_capturing_tracker_;
  std::unique_ptr<OneTimePermissionsTracker::Condition>
      audio_capturing_tracker_;
};

PAGE_USER_DATA_KEY_IMPL(OneTimePermissionsPageTracker);

OneTimePermissionsPageTracker::OneTimePermissionsPageTracker(
    content::Page& page,
    url::Origin origin)
    : PageUserData(page), origin_(std::move(origin)) {
  auto* tracker = OneTimePermissionsTrackerFactory::GetForBrowserContext(
      page.GetMainDocument().GetBrowserContext());
  if (!tracker) {
    return;
  }
  tracker_ = tracker;
  active_page_tracker_ = tracker_->NewActivePage(origin_);
  if (content::WebContents::FromRenderFrameHost(&page.GetMainDocument())
          ->GetVisibility() == content::Visibility::HIDDEN) {
    // Make sure we track this page being in background.
    tracker_->NewForegroundPage(origin_);
  } else {
    foreground_page_tracker_ = tracker_->NewForegroundPage(origin_);
  }
}

OneTimePermissionsPageTracker::~OneTimePermissionsPageTracker() = default;

void OneTimePermissionsPageTracker::OnVisibilityChanged(
    content::Visibility visibility) {
  if (!tracker_) {
    return;
  }
  const bool is_hidden = (visibility == content::Visibility::HIDDEN);
  if (is_hidden && foreground_page_tracker_) {
    foreground_page_tracker_.reset();
  } else if (!is_hidden && !foreground_page_tracker_) {
    foreground_page_tracker_ = tracker_->NewForegroundPage(origin_);
  }
}

void OneTimePermissionsPageTracker::OnIsCapturingVideoChanged(
    bool is_capturing_video) {
  if (!tracker_) {
    return;
  }
  if (is_capturing_video && !video_capturing_tracker_) {
    video_capturing_tracker_ = tracker_->NewVideoCapturing(origin_);
  } else if (!is_capturing_video && video_capturing_tracker_) {
    video_capturing_tracker_.reset();
  }
}

void OneTimePermissionsPageTracker::OnIsCapturingAudioChanged(
    bool is_capturing_audio) {
  if (!tracker_) {
    return;
  }
  if (is_capturing_audio && !audio_capturing_tracker_) {
    audio_capturing_tracker_ = tracker_->NewAudioCapturing(origin_);
  } else if (!is_capturing_audio && audio_capturing_tracker_) {
    audio_capturing_tracker_.reset();
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

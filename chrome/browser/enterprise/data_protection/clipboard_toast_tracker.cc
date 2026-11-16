// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/clipboard_toast_tracker.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"

namespace enterprise_data_protection {

namespace {
const void* const kClipboardToastTrackerKey = &kClipboardToastTrackerKey;
}  // namespace

ClipboardToastTracker::ClipboardToastTracker() = default;
ClipboardToastTracker::~ClipboardToastTracker() = default;

// static
ClipboardToastTracker* ClipboardToastTracker::GetForProfile(Profile* profile) {
  if (!profile) {
    return nullptr;
  }
  auto* tracker = static_cast<ClipboardToastTracker*>(
      profile->GetUserData(kClipboardToastTrackerKey));
  if (!tracker) {
    auto new_tracker = std::make_unique<ClipboardToastTracker>();
    tracker = new_tracker.get();
    profile->SetUserData(kClipboardToastTrackerKey, std::move(new_tracker));
  }
  return tracker;
}

bool ClipboardToastTracker::ShouldShowToast(CopyToastType type) {
  return shown_toasts_.find(type) == shown_toasts_.end();
}

void ClipboardToastTracker::RecordToastShown(CopyToastType type) {
  shown_toasts_.insert(type);
}

void ClipboardToastTracker::MarkVideoFrameCopy(
    content::GlobalRenderFrameHostId frame_id) {
  video_frame_copy_frame_id_ = frame_id;
}

bool ClipboardToastTracker::TakeVideoFrameCopy(
    content::GlobalRenderFrameHostId frame_id) {
  if (!frame_id || video_frame_copy_frame_id_ != frame_id) {
    return false;
  }
  video_frame_copy_frame_id_ = content::GlobalRenderFrameHostId();
  return true;
}

bool MaybeShowCopyToast(Profile* profile,
                        content::WebContents* web_contents,
                        CopyToastType type) {
  if (!profile || !web_contents) {
    return false;
  }
  auto* tracker = ClipboardToastTracker::GetForProfile(profile);
  if (!tracker || !tracker->ShouldShowToast(type)) {
    return false;
  }
  auto* toast_controller =
      ToastController::MaybeGetForWebContents(web_contents);
  if (toast_controller) {
    ToastId toast_id = (type == CopyToastType::kAudit)
                           ? ToastId::kEnterpriseCopyAudit
                           : ToastId::kEnterpriseCopyKeptInManagedChrome;
    tracker->RecordToastShown(type);
    return toast_controller->MaybeShowToast(ToastParams(toast_id));
  }
  return false;
}

}  // namespace enterprise_data_protection

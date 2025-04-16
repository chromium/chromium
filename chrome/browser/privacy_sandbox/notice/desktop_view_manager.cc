// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/desktop_view_manager.h"

#include <vector>

#include "chrome/browser/privacy_sandbox/notice/notice_model.h"

namespace privacy_sandbox {

using notice::mojom::PrivacySandboxNotice;

DesktopViewManager::DesktopViewManager(
    PrivacySandboxNoticeServiceInterface* notice_service)
    : notice_service_(notice_service) {
  CHECK(notice_service_);
}

DesktopViewManager::~DesktopViewManager() {
  observers_.Clear();
}

void DesktopViewManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DesktopViewManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DesktopViewManager::MaybeCreateView() {
  // TODO(crbug.com/408016824): Implement checks to determine whether we should
  // show the notice or not.
  if (!notice_service_) {
    return;
  }

  std::vector<PrivacySandboxNotice> required_notices =
      notice_service_->GetRequiredNotices(SurfaceType::kDesktopNewTab);

  if (required_notices != pending_notices_to_show_) {
    CloseAllOpenViews();
  }

  pending_notices_to_show_ = required_notices;
  // TODO(crbug.com/408016824): Call into dialog view to actually create a new
  // view.
}

void DesktopViewManager::CloseAllOpenViews() {
  for (auto& observer : observers_) {
    observer.MaybeNavigateToNextStep(std::nullopt);
  }
}

std::vector<PrivacySandboxNotice>
DesktopViewManager::GetPendingNoticesToShow() {
  return pending_notices_to_show_;
}

}  // namespace privacy_sandbox

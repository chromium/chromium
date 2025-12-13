// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/desktop_view_manager.h"

#include <vector>

#include "chrome/browser/privacy_sandbox/notice/notice_model.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"

namespace privacy_sandbox {

using notice::mojom::PrivacySandboxNotice;
using notice::mojom::PrivacySandboxNoticeEvent;
using enum notice::mojom::PrivacySandboxNoticeEvent;

DesktopViewManagerInterface::~DesktopViewManagerInterface() = default;

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

void DesktopViewManager::MaybeCreateView(BrowserWindowInterface* browser,
                                         ShowViewCallback show) {
  std::vector<PrivacySandboxNotice> required_notices =
      notice_service_->GetRequiredNotices(SurfaceType::kDesktopNewTab);

  if (required_notices != pending_notices_to_show_) {
    CloseAllOpenViews();
  }

  SetPendingNoticesToShow(required_notices);
  if (!pending_notices_to_show_.empty()) {
    std::move(show).Run(browser, pending_notices_to_show_[0]);
  }
}

void DesktopViewManager::CloseAllOpenViews() {
  for (auto& observer : observers_) {
    observer.MaybeNavigateToNextStep(std::nullopt);
  }
}

void DesktopViewManager::MaybeAdvanceAllOpenViews(
    PrivacySandboxNoticeEvent event) {
  switch (event) {
    case kAck:
    case kOptIn:
    case kOptOut:
    case kSettings: {
      pending_notices_to_show_.erase(pending_notices_to_show_.begin());
      std::optional<PrivacySandboxNotice> next_notice =
          pending_notices_to_show_.empty()
              ? std::nullopt
              : std::make_optional(pending_notices_to_show_.front());
      for (auto& observer : observers_) {
        observer.MaybeNavigateToNextStep(next_notice);
      }
      break;
    }
    // TODO(crbug.com/408016824): Change behavior of kClosed based on where it
    // emits from.
    case kClosed:
    case kShown: {
      return;
    }
  }
}

void DesktopViewManager::OnEventOccurred(PrivacySandboxNotice notice,
                                         PrivacySandboxNoticeEvent event) {
  // There should always be one element in the list, that is the notice that's
  // currently being shown.
  CHECK(!pending_notices_to_show_.empty());
  notice_service_->EventOccurred({notice, SurfaceType::kDesktopNewTab}, event);

  MaybeAdvanceAllOpenViews(event);
}

std::vector<PrivacySandboxNotice>
DesktopViewManager::GetPendingNoticesToShow() {
  return pending_notices_to_show_;
}

void DesktopViewManager::SetPendingNoticesToShow(
    std::vector<PrivacySandboxNotice> notices) {
  pending_notices_to_show_ = std::move(notices);
}

bool DesktopViewManager::IsPromptShowingOnBrowser(
    BrowserWindowInterface* browser) {
  for (auto& observer : observers_) {
    // Return true if an observer is registered for the current browser.
    if (observer.GetBrowser() == browser) {
      return true;
    }
  }
  return false;
}

void DesktopViewManager::HandleChromeOwnedPageNavigation(
    BrowserWindowInterface* browser_interface) {
  // TODO(crbug.com/408016824): Move this Feature flag check to the orchestrator
  // once implemented.
}

}  // namespace privacy_sandbox

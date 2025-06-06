// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_VIEW_MANAGER_TEST_PEER_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_VIEW_MANAGER_TEST_PEER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/privacy_sandbox/notice/desktop_view_manager.h"

namespace privacy_sandbox {

// Allow tests to access private variables and functions from
// `DesktopViewManager`.
class DesktopViewManagerTestPeer {
 public:
  explicit DesktopViewManagerTestPeer(DesktopViewManager* desktop_view_manager)
      : desktop_view_manager_(desktop_view_manager) {}
  ~DesktopViewManagerTestPeer() = default;

  bool IsPromptShowingOnBrowser(BrowserWindowInterface* browser) {
    return desktop_view_manager_->IsPromptShowingOnBrowser(browser);
  }

  void HandleChromeOwnedPageNavigation(BrowserWindowInterface* browser) {
    desktop_view_manager_->HandleChromeOwnedPageNavigation(browser);
  }

  void SetPendingNotices(
      std::vector<notice::mojom::PrivacySandboxNotice> pending_notices) {
    desktop_view_manager_->SetPendingNoticesToShow(pending_notices);
  }

  void CreateView(BrowserWindowInterface* browser,
                  DesktopViewManager::ShowViewCallback show) {
    desktop_view_manager_->MaybeCreateView(browser, std::move(show));
  }

 private:
  raw_ptr<DesktopViewManager> desktop_view_manager_;
};
}  // namespace privacy_sandbox
#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_VIEW_MANAGER_TEST_PEER_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_VIEW_MANAGER_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_VIEW_MANAGER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/privacy_sandbox/notice/desktop_entrypoint_handlers.h"
#include "chrome/browser/privacy_sandbox/notice/desktop_view_manager_interface.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom-forward.h"
#include "chrome/browser/privacy_sandbox/notice/notice_service_interface.h"

class BrowserWindowInterface;

namespace privacy_sandbox {

class PrivacySandboxNoticeServiceInterface;

class DesktopViewManager : public DesktopViewManagerInterface {
 public:
  using ShowViewCallback =
      base::OnceCallback<void(BrowserWindowInterface*,
                              notice::mojom::PrivacySandboxNotice)>;

  explicit DesktopViewManager(
      PrivacySandboxNoticeServiceInterface* notice_service);
  ~DesktopViewManager() override;

  // Accessors
  std::vector<notice::mojom::PrivacySandboxNotice> GetPendingNoticesToShow();

  // DesktopViewManagerInterface:
  NavigationHandler* GetNavigationHandler() override;
  void HandleChromeOwnedPageNavigation(
      BrowserWindowInterface* browser_interface) override;
  void OnEventOccurred(notice::mojom::PrivacySandboxNotice notice,
                       notice::mojom::PrivacySandboxNoticeEvent event) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  friend class DesktopViewManagerTestPeer;

  // Performs necessary checks to determine if a new view should be created.
  void MaybeCreateView(BrowserWindowInterface* browser, ShowViewCallback show);

  void SetPendingNoticesToShow(
      std::vector<notice::mojom::PrivacySandboxNotice> notices);

  // Notifies open views to close.
  void CloseAllOpenViews();

  // If the event taken isn't a shown event, notifies open views to advance to
  // the next step. If the next step doesn't exist, all open views will be
  // notified to close.
  void MaybeAdvanceAllOpenViews(notice::mojom::PrivacySandboxNoticeEvent event);

  base::ObserverList<Observer>::Unchecked observers_;
  raw_ptr<PrivacySandboxNoticeServiceInterface> notice_service_;
  std::vector<notice::mojom::PrivacySandboxNotice> pending_notices_to_show_;
  // Storage of various entrypoint handlers.
  std::unique_ptr<NavigationHandler> navigation_handler_;
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_VIEW_MANAGER_H_

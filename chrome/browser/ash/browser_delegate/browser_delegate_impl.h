// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_DELEGATE_IMPL_H_

#include "base/memory/raw_ref.h"
#include "chromeos/ash/components/browser_delegate/browser_delegate.h"

class BrowserWindowInterface;

namespace ash {

class BrowserDelegateImpl : public BrowserDelegate {
 public:
  explicit BrowserDelegateImpl(BrowserWindowInterface* browser);
  virtual ~BrowserDelegateImpl();

  // BrowserDelegate:
  BrowserWindowInterface& GetBrowser() const override;
  BrowserType GetType() const override;
  SessionID GetSessionID() const override;
  const AccountId& GetAccountId() const override;
  bool IsOffTheRecord() const override;
  bool IsCreatedByStartupCreator() const override;
  bool IsCreatedBySessionRestoreForStartupUrls() const override;
  gfx::Rect GetBounds() const override;
  content::WebContents* GetActiveWebContents() const override;
  size_t GetWebContentsCount() const override;
  content::WebContents* GetWebContentsAt(size_t index) const override;
  tabs::TabIteratorRange GetTabIterator() const override;
  content::WebContents* GetInspectedWebContents() const override;
  ui::BaseWindow* GetWindow() const override;
  aura::Window* GetNativeWindow() const override;
  std::optional<webapps::AppId> GetAppId() const override;
  std::optional<std::string> GetUserDefinedWindowTitle() const override;
  bool IsWebApp() const override;
  const SystemWebAppDelegate* GetSWADelegate() const override;
  bool IsClosing() const override;
  bool IsAttemptingToClose() const override;
  bool IsActive() const override;
  bool IsMinimized() const override;
  bool IsVisible() const override;
  bool IsFullscreen() const override;
  void SetFullscreen(bool fullscreen) override;
  void Show() override;
  void ShowInactive() override;
  void Activate() override;
  void Minimize() override;
  void Close() override;
  void SetSkipWarningUserOnClose(bool skip) override;
  void AddTab(const GURL& url,
              std::optional<size_t> index,
              TabDisposition disposition) override;
  void CloseWebContentsAt(size_t index, UserGesture user_gesture) override;
  content::WebContents* NavigateWebApp(
      const GURL& url,
      TabPinning pin_tab,
      std::optional<webapps::LaunchParams> launch_params =
          std::nullopt) override;
  void CreateTabGroup(const tab_groups::TabGroupInfo& tab_group) override;
  std::vector<tab_groups::TabGroupInfo> GetTabGroupInfos() const override;
  void PinTab(size_t tab_index) override;
  void MoveTab(size_t tab_index, BrowserDelegate& target_browser) override;
  bool CreateWebAppFromActiveWebContents() override;
  void ResetLocationBar() override;
  void SetOnTaskState(OnTaskState state) override;
  bool IsOnTaskState(OnTaskState state) const override;
  void ActivateWebContentsAt(size_t index) override;
  void SetContentsBackgroundVisible(bool visible) override;
  void EnterLockedFullscreen() override;
  void LeaveLockedFullscreen() override;
  bool IsLockedFullscreen() const override;

 private:
  // TODO(crbug.com/365146870): The following utility functions will be removed
  // once the LockedStateController migration is complete.
  void SetDevToolsCommandsEnabled(bool enabled);
  void SetTabSwitchCommandsEnabled(bool enabled);

  const raw_ref<BrowserWindowInterface> browser_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_DELEGATE_IMPL_H_

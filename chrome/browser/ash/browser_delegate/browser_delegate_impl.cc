// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/browser_delegate/browser_delegate_impl.h"

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ash/browser_delegate/browser_type.h"
#include "chrome/browser/ash/browser_delegate/browser_type_conversion.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_info.h"
#include "ui/base/base_window.h"

namespace ash {

BrowserDelegateImpl::BrowserDelegateImpl(Browser* browser)
    : browser_(CHECK_DEREF(browser)) {}

BrowserDelegateImpl::~BrowserDelegateImpl() = default;

Browser& BrowserDelegateImpl::GetBrowser() const {
  return browser_.get();
}

BrowserType BrowserDelegateImpl::GetType() const {
  return FromInternalBrowserType(browser_->type());
}

SessionID BrowserDelegateImpl::GetSessionID() const {
  return browser_->session_id();
}

const AccountId& BrowserDelegateImpl::GetAccountId() const {
  const AccountId* id =
      ash::AnnotatedAccountId::Get(browser_->profile()->GetOriginalProfile());
  if (id) {
    CHECK(id->is_valid());
  } else {
    CHECK_IS_TEST();
  }
  return id ? *id : EmptyAccountId();
}

bool BrowserDelegateImpl::IsOffTheRecord() const {
  return browser_->profile()->IsOffTheRecord();
}

gfx::Rect BrowserDelegateImpl::GetBounds() const {
  return browser_->window()->GetBounds();
}

content::WebContents* BrowserDelegateImpl::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

size_t BrowserDelegateImpl::GetWebContentsCount() const {
  return browser_->tab_strip_model()->count();
}

content::WebContents* BrowserDelegateImpl::GetWebContentsAt(
    size_t index) const {
  return browser_->tab_strip_model()->GetWebContentsAt(index);
}

content::WebContents* BrowserDelegateImpl::GetInspectedWebContents() const {
  if (GetType() != BrowserType::kDevTools) {
    return nullptr;
  }

  content::WebContents* target_tab = nullptr;
  if (auto* dev_tools_window = DevToolsWindow::AsDevToolsWindow(&*browser_)) {
    target_tab = dev_tools_window->GetInspectedWebContents();
  }

  return target_tab;
}

ui::BaseWindow* BrowserDelegateImpl::GetWindow() const {
  return browser_->window();
}

aura::Window* BrowserDelegateImpl::GetNativeWindow() const {
  return browser_->window()->GetNativeWindow();
}

std::optional<webapps::AppId> BrowserDelegateImpl::GetAppId() const {
  // The implementation of `GetAppIdFromApplicationName()` isn't specific to
  // WebApps, although the function resides in web_app_helpers.cc|h.
  std::string app_id =
      web_app::GetAppIdFromApplicationName(browser_->app_name());
  return app_id.empty() ? std::nullopt : std::optional<webapps::AppId>(app_id);
}

bool BrowserDelegateImpl::IsWebApp() const {
  return web_app::AppBrowserController::IsWebApp(&*browser_);
}

bool BrowserDelegateImpl::IsAttemptingToClose() const {
  return browser_->IsAttemptingToCloseBrowser();
}

bool BrowserDelegateImpl::IsClosing() const {
  return browser_->is_delete_scheduled();
}

bool BrowserDelegateImpl::IsActive() const {
  return browser_->window()->IsActive();
}

bool BrowserDelegateImpl::IsMinimized() const {
  return browser_->window()->IsMinimized();
}

bool BrowserDelegateImpl::IsVisible() const {
  return browser_->window()->IsVisible();
}

void BrowserDelegateImpl::Show() {
  browser_->window()->Show();
}

void BrowserDelegateImpl::ShowInactive() {
  browser_->window()->ShowInactive();
}

void BrowserDelegateImpl::Activate() {
  browser_->window()->Activate();
}

void BrowserDelegateImpl::Minimize() {
  browser_->window()->Minimize();
}

void BrowserDelegateImpl::Close() {
  browser_->window()->Close();
}

void BrowserDelegateImpl::AddTab(const GURL& url,
                                 std::optional<size_t> index,
                                 TabDisposition disposition) {
  chrome::AddTabAt(&browser_.get(), url, index.has_value() ? *index : -1,
                   disposition == TabDisposition::kForeground);
}

void BrowserDelegateImpl::CloseWebContentsAt(size_t index,
                                             UserGesture user_gesture) {
  browser_->tab_strip_model()->CloseWebContentsAt(
      index, user_gesture == UserGesture::kYes
                 ? TabCloseTypes::CLOSE_USER_GESTURE
                 : TabCloseTypes::CLOSE_NONE);
}

content::WebContents* BrowserDelegateImpl::NavigateWebApp(const GURL& url,
                                                          TabPinning pin_tab) {
  CHECK(GetType() == BrowserType::kApp || GetType() == BrowserType::kAppPopup)
      << "Unexpected browser type " << static_cast<int>(GetType()) << "("
      << browser_->type() << ")";

  NavigateParams nav_params(&browser_.get(), url,
                            ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  if (pin_tab == TabPinning::kYes) {
    nav_params.tabstrip_add_types |= AddTabTypes::ADD_PINNED;
  }

  return web_app::NavigateWebAppUsingParams(nav_params);
}

void BrowserDelegateImpl::CreateTabGroup(
    const tab_groups::TabGroupInfo& tab_group) {
  std::vector<int> indices;
  for (uint32_t index = tab_group.tab_range.start();
       index < tab_group.tab_range.end(); ++index) {
    indices.push_back(static_cast<int>(index));
  }

  TabStripModel* tab_strip_model = browser_->tab_strip_model();
  const tab_groups::TabGroupId new_group_id =
      tab_strip_model->AddToNewGroup(indices);
  tab_strip_model->ChangeTabGroupVisuals(new_group_id, tab_group.visual_data);
}

void BrowserDelegateImpl::PinTab(size_t tab_index) {
  browser_->tab_strip_model()->SetTabPinned(static_cast<int>(tab_index),
                                            /*pinned=*/true);
}

void BrowserDelegateImpl::MoveTab(size_t tab_index,
                                  BrowserDelegate& target_browser) {
  TabStripModel* source_tab_strip = browser_->tab_strip_model();
  TabStripModel* target_tab_strip =
      static_cast<BrowserDelegateImpl&>(target_browser)
          .browser_->tab_strip_model();

  const bool was_pinned = source_tab_strip->IsTabPinned(tab_index);

  std::unique_ptr<tabs::TabModel> detached_tab =
      source_tab_strip->DetachTabAtForInsertion(tab_index);
  target_tab_strip->InsertDetachedTabAt(
      TabStripModel::kNoTab, std::move(detached_tab),
      was_pinned ? AddTabTypes::ADD_PINNED : AddTabTypes::ADD_ACTIVE);
}

bool BrowserDelegateImpl::CreateWebAppFromActiveWebContents() {
  return chrome::ExecuteCommand(&*browser_, IDC_INSTALL_PWA);
}

}  // namespace ash

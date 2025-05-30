// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/browser_delegate/browser_delegate_impl.h"

#include "base/check_deref.h"
#include "chrome/browser/ash/browser_delegate/browser_type_conversion.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"

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

aura::Window* BrowserDelegateImpl::GetNativeWindow() const {
  return browser_->window()->GetNativeWindow();
}

bool BrowserDelegateImpl::IsClosing() const {
  return browser_->IsBrowserClosing();
}

void BrowserDelegateImpl::Show() {
  browser_->window()->Show();
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

}  // namespace ash

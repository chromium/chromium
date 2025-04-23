// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/browser_delegate/browser_delegate_impl.h"

#include "base/check_deref.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace ash {

BrowserDelegateImpl::BrowserDelegateImpl(Browser* browser)
    : browser_(CHECK_DEREF(browser)) {}

BrowserDelegateImpl::~BrowserDelegateImpl() = default;

Browser& BrowserDelegateImpl::GetBrowser() const {
  return browser_.get();
}

SessionID BrowserDelegateImpl::GetSessionID() const {
  return browser_->session_id();
}

content::WebContents* BrowserDelegateImpl::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

aura::Window* BrowserDelegateImpl::GetNativeWindow() const {
  return browser_->window()->GetNativeWindow();
}

bool BrowserDelegateImpl::IsClosing() const {
  return browser_->IsBrowserClosing();
}

void BrowserDelegateImpl::Show() {
  return browser_->window()->Show();
}

}  // namespace ash

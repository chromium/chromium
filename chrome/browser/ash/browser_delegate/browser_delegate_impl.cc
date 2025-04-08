// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/browser_delegate/browser_delegate_impl.h"

#include "chrome/browser/ui/browser.h"

namespace ash {

BrowserDelegateImpl::BrowserDelegateImpl(Browser* browser)
    : browser_(browser) {}

BrowserDelegateImpl::~BrowserDelegateImpl() = default;

SessionID BrowserDelegateImpl::GetSessionID() const {
  return browser_->session_id();
}

}  // namespace ash

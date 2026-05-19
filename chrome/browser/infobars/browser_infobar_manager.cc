// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/browser_infobar_manager.h"

#include "chrome/browser/browser_process.h"

namespace infobars {

DEFINE_USER_DATA(BrowserInfoBarManager);

BrowserInfoBarManager::BrowserInfoBarManager(BrowserProcess* browser_process)
    : scoped_unowned_user_data_(browser_process->GetUnownedUserDataHost(),
                                *this) {}

BrowserInfoBarManager::~BrowserInfoBarManager() = default;

// static
BrowserInfoBarManager* BrowserInfoBarManager::From(
    BrowserProcess* browser_process) {
  return Get(browser_process->GetUnownedUserDataHost());
}

void BrowserInfoBarManager::Show(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  // TODO(crbug.com/512825363): logic to show infobars and global injection
  // logic using GlobalBrowserCollection.
}

void BrowserInfoBarManager::Hide(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  // TODO(crbug.com/512825363): Global removal logic.
}

}  // namespace infobars

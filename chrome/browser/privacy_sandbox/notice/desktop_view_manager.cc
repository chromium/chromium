// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/desktop_view_manager.h"

namespace privacy_sandbox {

DesktopViewManager::DesktopViewManager(
    PrivacySandboxNoticeServiceInterface* notice_service) {}
DesktopViewManager::~DesktopViewManager() {
  observers_.Clear();
}

void DesktopViewManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DesktopViewManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace privacy_sandbox

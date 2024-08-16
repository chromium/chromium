// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/tab_desktop_media_list_mock_observer.h"

DesktopMediaListMockObserver::DesktopMediaListMockObserver() = default;
DesktopMediaListMockObserver::~DesktopMediaListMockObserver() = default;

void DesktopMediaListMockObserver::VerifyAndClearExpectations() {
  testing::Mock::VerifyAndClearExpectations(this);
}

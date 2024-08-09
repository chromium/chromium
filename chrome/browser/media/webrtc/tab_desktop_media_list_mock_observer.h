// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_TAB_DESKTOP_MEDIA_LIST_MOCK_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_TAB_DESKTOP_MEDIA_LIST_MOCK_OBSERVER_H_

#include <cstddef>

#include "chrome/browser/media/webrtc/desktop_media_list_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

class DesktopMediaListMockObserver : public DesktopMediaListObserver {
 public:
  DesktopMediaListMockObserver();
  ~DesktopMediaListMockObserver() override;

  MOCK_METHOD(void, OnSourceAdded, (int index), (override));
  MOCK_METHOD(void, OnSourceRemoved, (int index), (override));
  MOCK_METHOD(void, OnSourceMoved, (int old_index, int new_index), (override));
  MOCK_METHOD(void, OnSourceNameChanged, (int index), (override));
  MOCK_METHOD(void, OnSourceThumbnailChanged, (int index), (override));
  MOCK_METHOD(void, OnSourcePreviewChanged, (size_t index), (override));
  MOCK_METHOD(void, OnDelegatedSourceListSelection, (), (override));
  MOCK_METHOD(void, OnDelegatedSourceListDismissed, (), (override));

  void VerifyAndClearExpectations();
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_TAB_DESKTOP_MEDIA_LIST_MOCK_OBSERVER_H_

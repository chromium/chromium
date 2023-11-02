// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_READING_LIST_READING_LIST_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_READING_LIST_READING_LIST_BRIDGE_H_

#include "chrome/browser/reading_list/android/reading_list_notification_delegate.h"

class ReadingListBridge : public ReadingListNotificationDelegate {
 public:
  ReadingListBridge() = default;
  ~ReadingListBridge() override = default;

 private:
  // ReadingListNotificationDelegate implementation.
  std::u16string getNotificationTitle() override;
  std::u16string getNotificationSubTitle(int unread_size) override;
  void OpenReadingListPage() override;
};

#endif  // CHROME_BROWSER_ANDROID_READING_LIST_READING_LIST_BRIDGE_H_

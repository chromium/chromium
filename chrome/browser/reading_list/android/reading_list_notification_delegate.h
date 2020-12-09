// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_NOTIFICATION_DELEGATE_H_
#define CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_NOTIFICATION_DELEGATE_H_

#include "base/strings/string16.h"

class ReadingListNotificationDelegate {
 public:
  ReadingListNotificationDelegate() = default;
  virtual ~ReadingListNotificationDelegate() = default;

  ReadingListNotificationDelegate(const ReadingListNotificationDelegate&) =
      delete;
  ReadingListNotificationDelegate& operator=(
      const ReadingListNotificationDelegate&) = delete;

  // Gets the weekly notification title.
  virtual base::string16 getNotificationTitle() = 0;

  // Gets the weekly notification subtitle.
  virtual base::string16 getNotificationSubTitle(int unread_size) = 0;

  // Opens the reading list UI.
  virtual void OpenReadingListPage() = 0;
};

#endif  // CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_NOTIFICATION_DELEGATE_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_NOTIFICATION_DELEGATE_H_
#define CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_NOTIFICATION_DELEGATE_H_

#include <string>


class ReadingListNotificationDelegate {
 public:
  ReadingListNotificationDelegate() = default;
  virtual ~ReadingListNotificationDelegate() = default;

  ReadingListNotificationDelegate(const ReadingListNotificationDelegate&) =
      delete;
  ReadingListNotificationDelegate& operator=(
      const ReadingListNotificationDelegate&) = delete;

  // Gets the weekly notification title.
  virtual std::u16string getNotificationTitle() = 0;

  // Gets the weekly notification subtitle.
  virtual std::u16string getNotificationSubTitle(int unread_size) = 0;

  // Opens the reading list UI.
  virtual void OpenReadingListPage() = 0;
};

#endif  // CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_NOTIFICATION_DELEGATE_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_BOOKMARK_NAVIGATION_HANDLE_USER_DATA_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_BOOKMARK_NAVIGATION_HANDLE_USER_DATA_H_

#include "content/public/browser/navigation_handle_user_data.h"

// Store whether a navigation is initiated by BookmarkBar.
class BookmarkNavigationHandleUserData
    : public content::NavigationHandleUserData<
          BookmarkNavigationHandleUserData> {
 public:
  // The enum is used for identifying the source of bookmark navigation.
  enum class InitiatorLocation { kBookmarkBar, kOther };

  ~BookmarkNavigationHandleUserData() override = default;

  InitiatorLocation navigation_type() const { return navigation_type_; }

 private:
  BookmarkNavigationHandleUserData(content::NavigationHandle& navigation,
                                   InitiatorLocation navigation_type)
      : navigation_type_(navigation_type) {}

  const InitiatorLocation navigation_type_;

  friend content::NavigationHandleUserData<BookmarkNavigationHandleUserData>;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_BOOKMARK_NAVIGATION_HANDLE_USER_DATA_H_

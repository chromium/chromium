// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NAVIGATION_HANDLE_USER_DATA_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NAVIGATION_HANDLE_USER_DATA_H_

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/page_navigator.h"

// Store whether a navigation is initiated by BookmarkBar.
class NavigationHandleUserData
    : public content::NavigationHandleUserData<NavigationHandleUserData> {
 public:
  // The enum is used for identifying the source of bookmark navigation.
  enum class InitiatorLocation { kBookmarkBar, kNewTabPage, kOther };

  ~NavigationHandleUserData() override = default;

  InitiatorLocation navigation_type() const { return navigation_type_; }

  static void AttachNewTabPageNavigationHandleUserData(
      content::NavigationHandle& navigation_handle);

 private:
  NavigationHandleUserData(content::NavigationHandle& navigation,
                           InitiatorLocation navigation_type)
      : navigation_type_(navigation_type) {}

  const InitiatorLocation navigation_type_;

  friend content::NavigationHandleUserData<NavigationHandleUserData>;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NAVIGATION_HANDLE_USER_DATA_H_

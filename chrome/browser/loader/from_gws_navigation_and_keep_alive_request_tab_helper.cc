// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_tab_helper.h"

#include "content/public/browser/web_contents.h"

FromGWSNavigationAndKeepAliveRequestTabHelper::
    FromGWSNavigationAndKeepAliveRequestTabHelper(content::WebContents* tab)
    : FromGWSNavigationAndKeepAliveRequestObserver(tab),
      content::WebContentsUserData<
          FromGWSNavigationAndKeepAliveRequestTabHelper>(*tab) {}

FromGWSNavigationAndKeepAliveRequestTabHelper::
    ~FromGWSNavigationAndKeepAliveRequestTabHelper() = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(FromGWSNavigationAndKeepAliveRequestTabHelper);

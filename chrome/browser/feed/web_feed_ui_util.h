// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEED_WEB_FEED_UI_UTIL_H_
#define CHROME_BROWSER_FEED_WEB_FEED_UI_UTIL_H_

namespace content {
class WebContents;
}

namespace feed {

void FollowSite(content::WebContents* web_contents);

void UnfollowSite(content::WebContents* web_contents);

}  // namespace feed

#endif  // CHROME_BROWSER_FEED_WEB_FEED_UI_UTIL_H_

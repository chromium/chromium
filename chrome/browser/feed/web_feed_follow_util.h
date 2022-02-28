// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEED_WEB_FEED_FOLLOW_UTIL_H_
#define CHROME_BROWSER_FEED_WEB_FEED_FOLLOW_UTIL_H_

class Profile;
namespace content {
class WebContents;
}

namespace feed {

class WebFeedSubscriptions;

WebFeedSubscriptions* GetSubscriptionsForProfile(Profile* profile);

void FollowSite(content::WebContents* web_contents);

}  // namespace feed

#endif  // CHROME_BROWSER_FEED_WEB_FEED_FOLLOW_CONTROLLER_H_

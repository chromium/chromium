// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_FEED_DEBUGGING_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_FEED_FEED_DEBUGGING_BRIDGE_H_

#include <jni.h>
#include <string>

class GURL;

// Native counterpart of FeedDebuggingBridge.java.
namespace feed {

GURL GetFeedFetchUrlForDebugging();

std::string GetFeedProcessScopeDumpForDebugging();

void TriggerRefreshForDebugging();

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_FEED_DEBUGGING_BRIDGE_H_

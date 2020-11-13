// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_V2_FEED_SERVICE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_FEED_V2_FEED_SERVICE_BRIDGE_H_

#include <string>

#include "components/feed/core/v2/public/types.h"

namespace feed {

// Native access to |FeedServiceBridge| in Java.
class FeedServiceBridge {
 public:
  static std::string GetLanguageTag();
  static DisplayMetrics GetDisplayMetrics();
  static void ClearAll();
  static bool IsEnabled();
  static void PrefetchImage(const GURL& url);
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_V2_FEED_SERVICE_BRIDGE_H_

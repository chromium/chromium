// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_ICON_BUNDLE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_ICON_BUNDLE_H_

#include "third_party/skia/include/core/SkBitmap.h"

namespace notifications {

// A wrapper of various format of icon and andriod id.
struct IconBundle {
  IconBundle();
  explicit IconBundle(SkBitmap skbitmap);
  explicit IconBundle(int resource_id);
  IconBundle(const IconBundle& other);
  IconBundle(IconBundle&& other);
  IconBundle& operator=(const IconBundle& other);
  IconBundle& operator=(IconBundle&& other);
  ~IconBundle();

  // The icon bitmap.
  SkBitmap bitmap;

  // Android resource Id. Do not set it until BeforeShowNotification. Default is
  // 0, representing no resource_id.
  int resource_id = 0;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_ICON_BUNDLE_H_

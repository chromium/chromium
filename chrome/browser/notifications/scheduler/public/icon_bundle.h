// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_ICON_BUNDLE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_ICON_BUNDLE_H_

#include "base/macros.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace notifications {

// A wrapper of various format of icon and andriod id.
struct IconBundle {
  IconBundle();
  explicit IconBundle(SkBitmap skbitmap);
  explicit IconBundle(int resource_id);
  IconBundle(const IconBundle& other);
  ~IconBundle();

  // The icon bitmap.
  SkBitmap bitmap;

  // Android resource Id. Do not set it until BeforeShowNotification. Default is
  // 0, representing no resource_id.
  int resource_id;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_ICON_BUNDLE_H_

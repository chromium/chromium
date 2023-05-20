// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_BRIDGE_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_BRIDGE_H_

#include "content/public/browser/web_contents.h"

class TouchToFillPasswordGenerationBridge {
 public:
  virtual ~TouchToFillPasswordGenerationBridge() = default;

  virtual bool Show(content::WebContents* web_contents) = 0;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_BRIDGE_H_

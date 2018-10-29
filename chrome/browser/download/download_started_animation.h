// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_STARTED_ANIMATION_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_STARTED_ANIMATION_H_

#include "base/macros.h"

namespace content {
class WebContents;
}

class DownloadStartedAnimation {
 public:
  static void Show(content::WebContents* web_contents);

 private:
  DownloadStartedAnimation() { }

  DISALLOW_COPY_AND_ASSIGN(DownloadStartedAnimation);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_STARTED_ANIMATION_H_

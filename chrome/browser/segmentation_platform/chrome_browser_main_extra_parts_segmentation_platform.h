// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEGMENTATION_PLATFORM_CHROME_BROWSER_MAIN_EXTRA_PARTS_SEGMENTATION_PLATFORM_H_
#define CHROME_BROWSER_SEGMENTATION_PLATFORM_CHROME_BROWSER_MAIN_EXTRA_PARTS_SEGMENTATION_PLATFORM_H_

#include "chrome/browser/chrome_browser_main_extra_parts.h"

class ChromeBrowserMainExtraPartsSegmentationPlatform
    : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsSegmentationPlatform() = default;

  void PreCreateThreads() override;
  void PreProfileInit() override;
  void PostProfileInit(Profile* profile, bool is_initial_profile) override;
  void PostMainMessageLoopRun() override;
};

#endif  // CHROME_BROWSER_SEGMENTATION_PLATFORM_CHROME_BROWSER_MAIN_EXTRA_PARTS_SEGMENTATION_PLATFORM_H_

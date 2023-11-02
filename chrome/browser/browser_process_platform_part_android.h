// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_ANDROID_H_
#define CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_ANDROID_H_

#include "chrome/browser/browser_process_platform_part_base.h"

class BrowserProcessPlatformPart : public BrowserProcessPlatformPartBase {
 public:
  BrowserProcessPlatformPart();

  BrowserProcessPlatformPart(const BrowserProcessPlatformPart&) = delete;
  BrowserProcessPlatformPart& operator=(const BrowserProcessPlatformPart&) =
      delete;

  ~BrowserProcessPlatformPart() override;

  // Overridden from BrowserProcessPlatformPartBase:
  void AttemptExit(bool try_to_quit_application) override;
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_ANDROID_H_

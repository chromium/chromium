// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_WIN_H_
#define CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_WIN_H_

#include "chrome/browser/browser_process_platform_part_base.h"
#include "chrome/browser/google/did_run_updater_win.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class BrowserProcessPlatformPart : public BrowserProcessPlatformPartBase {
 public:
  BrowserProcessPlatformPart();
  BrowserProcessPlatformPart(const BrowserProcessPlatformPart&) = delete;
  BrowserProcessPlatformPart& operator=(const BrowserProcessPlatformPart&) =
      delete;
  ~BrowserProcessPlatformPart() override;

  // BrowserProcessPlatformPartBase:
  void PlatformSpecificCommandLineProcessing(
      const base::CommandLine& command_line) override;

 private:
  absl::optional<DidRunUpdater> did_run_updater_;
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_WIN_H_

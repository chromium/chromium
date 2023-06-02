// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_SCREEN_AI_DOWNLOADER_H_
#define CHROME_BROWSER_SCREEN_AI_SCREEN_AI_DOWNLOADER_H_

#include "chrome/browser/screen_ai/screen_ai_install_state.h"
namespace screen_ai {

class ScreenAIDownloader : public ScreenAIInstallState {
 public:
  ScreenAIDownloader();
  ScreenAIDownloader(const ScreenAIDownloader&) = delete;
  ScreenAIDownloader& operator=(const ScreenAIDownloader&) = delete;
  ~ScreenAIDownloader() override;

  void DownloadComponent() override;
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_SCREEN_AI_SCREEN_AI_DOWNLOADER_H_

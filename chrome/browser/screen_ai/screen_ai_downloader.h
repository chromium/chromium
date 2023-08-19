// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_SCREEN_AI_DOWNLOADER_H_
#define CHROME_BROWSER_SCREEN_AI_SCREEN_AI_DOWNLOADER_H_

#include "chrome/browser/screen_ai/screen_ai_install_state.h"

#include "build/chromeos_buildflags.h"

#include "base/memory/weak_ptr.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/screen_ai_downloader.mojom.h"
#endif

namespace screen_ai {

class ScreenAIDownloader : public ScreenAIInstallState {
 public:
  ScreenAIDownloader();
  ScreenAIDownloader(const ScreenAIDownloader&) = delete;
  ScreenAIDownloader& operator=(const ScreenAIDownloader&) = delete;
  ~ScreenAIDownloader() override;

  void DownloadComponentInternal() override;
  void SetLastUsageTime() override;

 private:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void MaybeGetComponentFolderFromAsh(bool download_if_needed);
  void MaybeSetLastUsageTimeInAsh();
#endif

  base::WeakPtrFactory<ScreenAIDownloader> weak_ptr_factory_{this};
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_SCREEN_AI_SCREEN_AI_DOWNLOADER_H_

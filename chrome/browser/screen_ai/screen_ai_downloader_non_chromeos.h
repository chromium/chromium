// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_SCREEN_AI_DOWNLOADER_NON_CHROMEOS_H_
#define CHROME_BROWSER_SCREEN_AI_SCREEN_AI_DOWNLOADER_NON_CHROMEOS_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "components/component_updater/component_updater_service.h"
#include "components/update_client/update_client.h"

namespace screen_ai {

class ScreenAIDownloaderNonChromeOS
    : public ScreenAIInstallState,
      public component_updater::ComponentUpdateService::Observer {
 public:
  ScreenAIDownloaderNonChromeOS();
  ScreenAIDownloaderNonChromeOS(const ScreenAIDownloaderNonChromeOS&) = delete;
  ScreenAIDownloaderNonChromeOS& operator=(
      const ScreenAIDownloaderNonChromeOS&) = delete;
  ~ScreenAIDownloaderNonChromeOS() override;

  // ScreenAIInstallState:
  void SetLastUsageTime() override;

  // component_updater::ComponentUpdateService::Observer:
  void OnEvent(const update_client::CrxUpdateItem& item) override;

 private:
  // ScreenAIInstallState:
  void DownloadComponentInternal() override;

  base::ScopedObservation<component_updater::ComponentUpdateService,
                          component_updater::ComponentUpdateService::Observer>
      component_updater_observation_{this};

  base::WeakPtrFactory<ScreenAIDownloaderNonChromeOS> weak_ptr_factory_{this};
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_SCREEN_AI_SCREEN_AI_DOWNLOADER_NON_CHROMEOS_H_

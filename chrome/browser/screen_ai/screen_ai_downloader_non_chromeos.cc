// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_downloader_non_chromeos.h"

#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/screen_ai_component_installer.h"
#include "chrome/browser/screen_ai/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "content/public/browser/browser_thread.h"
#include "services/screen_ai/public/cpp/utilities.h"

namespace screen_ai {

// static
ScreenAIInstallState* ScreenAIInstallState::Create() {
  static base::NoDestructor<ScreenAIDownloaderNonChromeOS> install_state;
  return install_state.get();
}

ScreenAIDownloaderNonChromeOS::ScreenAIDownloaderNonChromeOS() = default;
ScreenAIDownloaderNonChromeOS::~ScreenAIDownloaderNonChromeOS() = default;

void ScreenAIDownloaderNonChromeOS::DownloadComponentInternal() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Proactive defense to ensure calls do not crash in early browser
  // startup, late browser shutdown after `BrowserProcessImpl` teardown, or in
  // unit tests without `TestingBrowserProcess`.
  if (!g_browser_process || !g_browser_process->component_updater()) {
    SetState(State::kDownloadFailed);
    return;
  }
  component_updater::RegisterScreenAIComponent(
      g_browser_process->component_updater());
}

void ScreenAIDownloaderNonChromeOS::OnStateOrObserversChanged() {
  // Only observe `ComponentUpdateService` while downloading and observers are
  // present. Since `ScreenAIDownloaderNonChromeOS` is a `base::NoDestructor`
  // singleton, this also ensures the observation is reset before
  // `g_browser_process->component_updater()` is destroyed at shutdown.
  if (HasObservers() && get_state() == State::kDownloading) {
    if (!component_updater_observation_.IsObserving() && g_browser_process &&
        g_browser_process->component_updater()) {
      component_updater_observation_.Observe(
          g_browser_process->component_updater());
    }
  } else {
    component_updater_observation_.Reset();
  }
}

void ScreenAIDownloaderNonChromeOS::SetLastUsageTime() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Proactive defense to ensure calls do not crash in early browser
  // startup, late browser shutdown after `BrowserProcessImpl` teardown, or in
  // unit tests without `TestingBrowserProcess` / `ScopedTestingLocalState`.
  if (!g_browser_process || !g_browser_process->local_state()) {
    return;
  }
  g_browser_process->local_state()->SetTime(
      prefs::kScreenAILastUsedTimePrefName, base::Time::Now());
}

void ScreenAIDownloaderNonChromeOS::OnEvent(
    const update_client::CrxUpdateItem& item) {
  if (item.id !=
      component_updater::ScreenAIComponentInstallerPolicy::GetOmahaId()) {
    return;
  }

  if (item.state == update_client::ComponentState::kDownloading) {
    SetDownloadProgress(static_cast<double>(item.downloaded_bytes) /
                        item.total_bytes);
  }
}

}  // namespace screen_ai

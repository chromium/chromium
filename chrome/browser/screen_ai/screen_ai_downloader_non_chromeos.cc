// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_downloader_non_chromeos.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/screen_ai_component_installer.h"
#include "chrome/browser/screen_ai/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace screen_ai {

// static
std::unique_ptr<ScreenAIInstallState> ScreenAIInstallState::Create() {
  return std::make_unique<ScreenAIDownloaderNonChromeOS>();
}

ScreenAIDownloaderNonChromeOS::ScreenAIDownloaderNonChromeOS() = default;
ScreenAIDownloaderNonChromeOS::~ScreenAIDownloaderNonChromeOS() = default;

void ScreenAIDownloaderNonChromeOS::DownloadComponentInternal() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  component_updater::RegisterScreenAIComponent(
      g_browser_process->component_updater());
}

void ScreenAIDownloaderNonChromeOS::SetLastUsageTime() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  g_browser_process->local_state()->SetTime(
      prefs::kScreenAILastUsedTimePrefName, base::Time::Now());
}

}  // namespace screen_ai

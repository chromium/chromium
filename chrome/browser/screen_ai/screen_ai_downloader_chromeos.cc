// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_downloader_chromeos.h"

#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/screen_ai/pref_names.h"
#include "chrome/browser/screen_ai/screen_ai_dlc_installer.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace {

void SetLastUsageTimeToNow() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  g_browser_process->local_state()->SetTime(
      prefs::kScreenAILastUsedTimePrefName, base::Time::Now());
}

}  // namespace

namespace screen_ai {

// static
std::unique_ptr<ScreenAIInstallState> ScreenAIInstallState::Create() {
  return std::make_unique<ScreenAIDownloaderChromeOS>();
}

// static
ScreenAIInstallState* ScreenAIInstallState::CreateForTesting() {
  static base::NoDestructor<ScreenAIDownloaderChromeOS> install_state;
  return install_state.get();
}

ScreenAIDownloaderChromeOS::ScreenAIDownloaderChromeOS() = default;
ScreenAIDownloaderChromeOS::~ScreenAIDownloaderChromeOS() = default;

void ScreenAIDownloaderChromeOS::DownloadComponentInternal() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  screen_ai::dlc_installer::Install();
}

void ScreenAIDownloaderChromeOS::SetLastUsageTime() {
  if (::content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    SetLastUsageTimeToNow();
  } else {
    content::GetUIThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&SetLastUsageTimeToNow));
  }
}

}  // namespace screen_ai

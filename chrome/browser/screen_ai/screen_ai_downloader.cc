// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_downloader.h"

#include "chrome/browser/browser_process.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/screen_ai/screen_ai_chromeos_installer.h"
#else
#include "chrome/browser/component_updater/screen_ai_component_installer.h"
#endif

namespace screen_ai {

ScreenAIDownloader::ScreenAIDownloader() = default;
ScreenAIDownloader::~ScreenAIDownloader() = default;

void ScreenAIDownloader::DownloadComponent() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(crbug.com/1278249): Consider trying again if download has failed
  // before.
  if (get_state() != State::kNotDownloaded) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  screen_ai::chrome_os_installer::ManageInstallation(
      g_browser_process->local_state());
#else
  component_updater::RegisterScreenAIComponent(
      g_browser_process->component_updater());
#endif
}

}  // namespace screen_ai

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_downloader.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/screen_ai/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/screen_ai/screen_ai_chromeos_installer.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/files/file_path.h"
#include "chromeos/lacros/lacros_service.h"
#else
#include "chrome/browser/component_updater/screen_ai_component_installer.h"
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void SetScreenAIComponentPath(
    const absl::optional<base::FilePath>& component_path) {
  auto* install_state = screen_ai::ScreenAIInstallState::GetInstance();
  if (component_path) {
    install_state->SetComponentFolder(*component_path);
  } else {
    install_state->SetState(screen_ai::ScreenAIInstallState::State::kFailed);
  }
}
#else
void SetLastUsageTimeToNow() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  g_browser_process->local_state()->SetTime(
      prefs::kScreenAILastUsedTimePrefName, base::Time::Now());
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

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
  screen_ai::chrome_os_installer::Install();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  MaybeTriggerDownloadInAsh();
#else
  component_updater::RegisterScreenAIComponent(
      g_browser_process->component_updater());
#endif
}

void ScreenAIDownloader::SetLastUsageTime() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // The last usage time should be sent to Ash as well for keeping track of the
  // library usage, and either keeping it updated or deleting it when it is not
  // used for a period of time.
  MaybeSetLastUsageTimeInAsh();
#else
  if (::content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    SetLastUsageTimeToNow();
  } else {
    content::GetUIThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&SetLastUsageTimeToNow));
  }
#endif
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)

void ScreenAIDownloader::MaybeTriggerDownloadInAsh() {
  chromeos::LacrosService* impl = chromeos::LacrosService::Get();
  if (!impl->IsAvailable<crosapi::mojom::ScreenAIDownloader>()) {
    VLOG(0) << "ScreenAIDownloader is not available.";
    ScreenAIInstallState::GetInstance()->SetState(
        ScreenAIInstallState::State::kFailed);
    return;
  }

  ScreenAIInstallState::GetInstance()->SetState(
      ScreenAIInstallState::State::kDownloading);
  impl->GetRemote<crosapi::mojom::ScreenAIDownloader>()->DownloadComponent(
      base::BindOnce(&SetScreenAIComponentPath));
}

void ScreenAIDownloader::MaybeSetLastUsageTimeInAsh() {
  chromeos::LacrosService* impl = chromeos::LacrosService::Get();
  if (!impl->IsAvailable<crosapi::mojom::ScreenAIDownloader>()) {
    VLOG(0) << "ScreenAIDownloader is not available.";
    return;
  }

  impl->GetRemote<crosapi::mojom::ScreenAIDownloader>()->SetLastUsageTime();
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace screen_ai

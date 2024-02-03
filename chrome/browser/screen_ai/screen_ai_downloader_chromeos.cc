// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_downloader_chromeos.h"

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/screen_ai/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/screen_ai/screen_ai_dlc_installer.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/files/file_path.h"
#include "chromeos/lacros/lacros_service.h"
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void SetScreenAIComponentPath(
    bool set_failed_state_if_not_available,
    const std::optional<base::FilePath>& component_path) {
  auto* install_state = screen_ai::ScreenAIInstallState::GetInstance();
  if (component_path) {
    install_state->SetComponentFolder(*component_path);
  } else if (set_failed_state_if_not_available) {
    install_state->SetState(
        screen_ai::ScreenAIInstallState::State::kDownloadFailed);
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

// static
std::unique_ptr<ScreenAIInstallState> ScreenAIInstallState::Create() {
  return std::make_unique<ScreenAIDownloaderChromeOS>();
}

// static
ScreenAIInstallState* ScreenAIInstallState::CreateForTesting() {
  static base::NoDestructor<ScreenAIDownloaderChromeOS> install_state;
  return install_state.get();
}

ScreenAIDownloaderChromeOS::ScreenAIDownloaderChromeOS() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If component is already downloaded in Ash, update Lacros state.
  // Ash may download the component in the startup steps as it was used in
  // previous sessions, but that can be a bit after Lacros is created. Therefor
  // we wait for 3 seconds to ask. This is a best effort, and if it does not get
  // the state now, it will be done later on the first time that Lacros needs
  // the library.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &ScreenAIDownloaderChromeOS::MaybeGetComponentFolderFromAsh,
          weak_ptr_factory_.GetWeakPtr(),
          /*download_if_needed=*/false),
      base::Seconds(3));
#endif
}

ScreenAIDownloaderChromeOS::~ScreenAIDownloaderChromeOS() = default;

void ScreenAIDownloaderChromeOS::DownloadComponentInternal() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  screen_ai::dlc_installer::Install();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  MaybeGetComponentFolderFromAsh(/*download_if_needed=*/true);
#endif
}

void ScreenAIDownloaderChromeOS::SetLastUsageTime() {
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

void ScreenAIDownloaderChromeOS::MaybeGetComponentFolderFromAsh(
    bool download_if_needed) {
  chromeos::LacrosService* impl = chromeos::LacrosService::Get();
  if (!impl || !impl->IsAvailable<crosapi::mojom::ScreenAIDownloader>()) {
    VLOG(0) << "ScreenAIDownloaderChromeOS is not available.";
    ScreenAIInstallState::GetInstance()->SetState(
        ScreenAIInstallState::State::kDownloadFailed);
    return;
  }

  if (download_if_needed) {
    ScreenAIInstallState::GetInstance()->SetState(
        ScreenAIInstallState::State::kDownloading);
  }

  if (static_cast<uint32_t>(
          impl->GetInterfaceVersion<crosapi::mojom::ScreenAIDownloader>()) <
      crosapi::mojom::ScreenAIDownloader::kGetComponentFolderMinVersion) {
    // Getting component folder without asking for download is not supported
    // yet.
    if (!download_if_needed) {
      return;
    }

    impl->GetRemote<crosapi::mojom::ScreenAIDownloader>()
        ->DownloadComponentDeprecated(
            base::BindOnce(&SetScreenAIComponentPath,
                           /*set_failed_state_if_not_available=*/true));

  } else {
    impl->GetRemote<crosapi::mojom::ScreenAIDownloader>()->GetComponentFolder(
        download_if_needed,
        base::BindOnce(
            &SetScreenAIComponentPath,
            /*set_failed_state_if_not_available=*/download_if_needed));
  }
}

void ScreenAIDownloaderChromeOS::MaybeSetLastUsageTimeInAsh() {
  chromeos::LacrosService* impl = chromeos::LacrosService::Get();
  if (!impl || !impl->IsAvailable<crosapi::mojom::ScreenAIDownloader>()) {
    VLOG(0) << "ScreenAIDownloaderChromeOS is not available.";
    return;
  }

  impl->GetRemote<crosapi::mojom::ScreenAIDownloader>()->SetLastUsageTime();
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace screen_ai

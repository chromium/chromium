// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_chrome_service_delegate_impl.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/statistics_recorder.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/feedback/feedback_dialog_utils.h"
#include "chrome/browser/lacros/feedback_util.h"
#include "chrome/browser/lacros/system_logs/lacros_system_log_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths_lacros.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_util.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace {

constexpr char kHistogramsFilename[] = "lacros_histograms.txt";

// Default directories for the ash-side primary user.
// TODO(https://crbug.com/1150702): Remove these after Lacros drops support for
// Chrome OS M89.
constexpr char kMyFilesPath[] = "/home/chronos/user/MyFiles";
constexpr char kDefaultDownloadsPath[] = "/home/chronos/user/MyFiles/Downloads";

std::string GetCompressedHistograms() {
  std::string histograms =
      base::StatisticsRecorder::ToJSON(base::JSON_VERBOSITY_LEVEL_FULL);
  std::string compressed_histograms;
  if (feedback_util::ZipString(base::FilePath(kHistogramsFilename),
                               std::move(histograms), &compressed_histograms)) {
    return compressed_histograms;
  } else {
    LOG(ERROR) << "Failed to compress lacros histograms.";
    return std::string();
  }
}

}  // namespace

LacrosChromeServiceDelegateImpl::LacrosChromeServiceDelegateImpl() = default;

LacrosChromeServiceDelegateImpl::~LacrosChromeServiceDelegateImpl() = default;

void LacrosChromeServiceDelegateImpl::OnInitialized(
    const crosapi::mojom::BrowserInitParams& init_params) {
  if (init_params.default_paths) {
    // Set up default paths with values provided by ash.
    chrome::SetLacrosDefaultPaths(init_params.default_paths->documents,
                                  init_params.default_paths->downloads);
  } else {
    // On older ash, provide some defaults.
    // TODO(https://crbug.com/1150702): Remove this block after Lacros drops
    // support for Chrome OS M89.
    if (base::SysInfo::IsRunningOnChromeOS()) {
      // On device, use /home/chronos/user paths.
      chrome::SetLacrosDefaultPaths(base::FilePath(kMyFilesPath),
                                    base::FilePath(kDefaultDownloadsPath));
    } else {
      // For developers on Linux desktop, just pick reasonable defaults.
      base::FilePath home_dir = base::GetHomeDir();
      chrome::SetLacrosDefaultPaths(home_dir.Append("Documents"),
                                    home_dir.Append("Downloads"));
    }
  }
}

void LacrosChromeServiceDelegateImpl::NewWindow(bool incognito) {
  // TODO(crbug.com/1102815): Find what profile should be used.
  Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  DCHECK(profile) << "No last used profile is found.";
  chrome::NewEmptyWindow(incognito ? profile->GetPrimaryOTRProfile() : profile);
}

void LacrosChromeServiceDelegateImpl::NewTab() {
  // TODO(crbug.com/1102815): Find what profile should be used.
  Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  DCHECK(profile) << "No last used profile is found.";
  Browser* browser = chrome::FindBrowserWithProfile(profile);
  DCHECK(browser) << "No browser is found.";
  chrome::NewTab(browser);
}

void LacrosChromeServiceDelegateImpl::RestoreTab() {
  // TODO(crbug.com/1102815): Find what profile should be used.
  Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  DCHECK(profile) << "No last used profile is found.";
  Browser* browser = chrome::FindBrowserWithProfile(profile);
  DCHECK(browser) << "No browser is found.";
  chrome::RestoreTab(browser);
}

std::string LacrosChromeServiceDelegateImpl::GetChromeVersion() {
  return chrome::GetVersionString(chrome::WithExtendedStable(true));
}

void LacrosChromeServiceDelegateImpl::GetFeedbackData(
    GetFeedbackDataCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Self-deleting object.
  system_logs::SystemLogsFetcher* fetcher =
      system_logs::BuildLacrosSystemLogsFetcher(/*scrub_data=*/true);
  fetcher->Fetch(
      base::BindOnce(&LacrosChromeServiceDelegateImpl::OnSystemInformationReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LacrosChromeServiceDelegateImpl::GetHistograms(
    GetHistogramsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // GetCompressedHistograms calls functions marking as blocking, so it
  // can not be running on UI thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(GetCompressedHistograms),
      base::BindOnce(
          &LacrosChromeServiceDelegateImpl::OnGetCompressedHistograms,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LacrosChromeServiceDelegateImpl::OnGetCompressedHistograms(
    GetHistogramsCallback callback,
    const std::string& compressed_histograms) {
  DCHECK(!callback.is_null());
  std::move(callback).Run(compressed_histograms);
}

GURL LacrosChromeServiceDelegateImpl::GetActiveTabUrl() {
  Browser* browser = chrome::FindBrowserWithActiveWindow();
  GURL page_url;
  if (browser) {
    page_url = chrome::GetTargetTabUrl(
        browser->session_id(), browser->tab_strip_model()->active_index());
  }
  return page_url;
}

void LacrosChromeServiceDelegateImpl::OnSystemInformationReady(
    GetFeedbackDataCallback callback,
    std::unique_ptr<system_logs::SystemLogsResponse> sys_info) {
  base::Value system_log_entries(base::Value::Type::DICTIONARY);
  if (sys_info) {
    std::string user_email = feedback_util::GetSignedInUserEmail();
    const bool google_email = gaia::IsGoogleInternalAccountEmail(user_email);

    for (auto& it : *sys_info) {
      // TODO(crbug.com/1138703): This code is duplicated with the logic in
      // feedback_private_api.cc, refactor to remove the duplicated code.
      // We only send the list of all the crash report IDs if the user has a
      // @google.com email. We strip this here so that the system information
      // view properly reflects what we will be uploading to the server. It is
      // also stripped later on in the feedback processing for other code paths
      // that don't go through this.
      if (it.first == feedback::FeedbackReport::kAllCrashReportIdsKey &&
          !google_email) {
        continue;
      }

      system_log_entries.SetStringKey(std::move(it.first),
                                      std::move(it.second));
    }
  }

  DCHECK(!callback.is_null());
  std::move(callback).Run(std::move(system_log_entries));
}

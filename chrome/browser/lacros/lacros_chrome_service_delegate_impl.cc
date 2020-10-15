// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_chrome_service_delegate_impl.h"

#include "base/logging.h"
#include "chrome/browser/lacros/feedback_util.h"
#include "chrome/browser/lacros/system_logs/lacros_system_log_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/channel_info.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"

LacrosChromeServiceDelegateImpl::LacrosChromeServiceDelegateImpl() = default;

LacrosChromeServiceDelegateImpl::~LacrosChromeServiceDelegateImpl() = default;

void LacrosChromeServiceDelegateImpl::NewWindow() {
  // TODO(crbug.com/1102815): Find what profile should be used.
  Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  DCHECK(profile) << "No last used profile is found.";
  chrome::NewEmptyWindow(profile);
}

std::string LacrosChromeServiceDelegateImpl::GetChromeVersion() {
  return chrome::GetVersionString();
}

void LacrosChromeServiceDelegateImpl::GetFeedbackData(
    GetFeedbackDataCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK(get_feedback_data_callback_.is_null());
  get_feedback_data_callback_ = std::move(callback);

  // Self-deleting object.
  system_logs::SystemLogsFetcher* fetcher =
      system_logs::BuildLacrosSystemLogsFetcher(/*scrub_data=*/true);
  fetcher->Fetch(
      base::BindOnce(&LacrosChromeServiceDelegateImpl::OnSystemInformationReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LacrosChromeServiceDelegateImpl::OnSystemInformationReady(
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

    DCHECK(!get_feedback_data_callback_.is_null());
    std::move(get_feedback_data_callback_).Run(std::move(system_log_entries));
  }
}

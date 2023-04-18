// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/files_internals_ui_delegate.h"

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/extensions/file_manager/event_router.h"
#include "chrome/browser/ash/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/ash/file_manager/file_manager_pref_names.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/fusebox/fusebox_server.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "components/prefs/pref_service.h"

ChromeFilesInternalsUIDelegate::ChromeFilesInternalsUIDelegate(
    content::WebUI* web_ui)
    : web_ui_(web_ui) {}

ChromeFilesInternalsUIDelegate::~ChromeFilesInternalsUIDelegate() = default;

base::Value ChromeFilesInternalsUIDelegate::GetDebugJSON() const {
  base::Value::Dict dict;

  if (fusebox::Server* fusebox_server = fusebox::Server::GetInstance()) {
    dict.Set("fusebox", fusebox_server->GetDebugJSON());
  } else {
    dict.Set("fusebox", base::Value());
  }

  return base::Value(std::move(dict));
}

bool ChromeFilesInternalsUIDelegate::GetSmbfsEnableVerboseLogging() const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  return profile && profile->GetPrefs()->GetBoolean(
                        file_manager::prefs::kSmbfsEnableVerboseLogging);
}

void ChromeFilesInternalsUIDelegate::SetSmbfsEnableVerboseLogging(
    bool enabled) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  if (profile) {
    profile->GetPrefs()->SetBoolean(
        file_manager::prefs::kSmbfsEnableVerboseLogging, enabled);
  }
}

bool ChromeFilesInternalsUIDelegate::GetOfficeSetupComplete() const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  return profile && file_manager::file_tasks::OfficeSetupComplete(profile);
}

void ChromeFilesInternalsUIDelegate::SetOfficeSetupComplete(bool complete) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  if (profile) {
    file_manager::file_tasks::SetOfficeSetupComplete(profile, complete);
    // If setup complete is set to false, also update the preferences to signal
    // that the move confirmation dialog has never been shown.
    if (!complete) {
      file_manager::file_tasks::SetOfficeMoveConfirmationShownForDrive(profile,
                                                                       false);
      file_manager::file_tasks::SetOfficeMoveConfirmationShownForOneDrive(
          profile, false);
    }
  }
}

bool ChromeFilesInternalsUIDelegate::GetMoveConfirmationShownForDrive() const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  return profile &&
         file_manager::file_tasks::GetOfficeMoveConfirmationShownForDrive(
             profile);
}

bool ChromeFilesInternalsUIDelegate::GetMoveConfirmationShownForOneDrive()
    const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  return profile &&
         file_manager::file_tasks::GetOfficeMoveConfirmationShownForOneDrive(
             profile);
}

bool ChromeFilesInternalsUIDelegate::GetAlwaysMoveOfficeFilesToDrive() const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  return profile &&
         file_manager::file_tasks::AlwaysMoveOfficeFilesToDrive(profile);
}

void ChromeFilesInternalsUIDelegate::SetAlwaysMoveOfficeFilesToDrive(
    bool always_move) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  if (profile) {
    file_manager::file_tasks::SetAlwaysMoveOfficeFilesToDrive(profile,
                                                              always_move);
    // Also clear up the timestamp for when files are moved to the Cloud.
    file_manager::file_tasks::SetOfficeFileMovedToGoogleDrive(profile,
                                                              base::Time());
    // Spawn the Files app Window so it clears up its localStorage.
    auto url = ::file_manager::util::GetFileManagerURL().Resolve("");
    ::ash::SystemAppLaunchParams params;
    params.url = url;
    ::ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::FILE_MANAGER,
                                   params);
  }
}

bool ChromeFilesInternalsUIDelegate::GetAlwaysMoveOfficeFilesToOneDrive()
    const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  return profile &&
         file_manager::file_tasks::AlwaysMoveOfficeFilesToOneDrive(profile);
}

void ChromeFilesInternalsUIDelegate::SetAlwaysMoveOfficeFilesToOneDrive(
    bool always_move) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  if (profile) {
    file_manager::file_tasks::SetAlwaysMoveOfficeFilesToOneDrive(profile,
                                                                 always_move);
    // Also clear up the timestamp for when files are moved to the Cloud.
    file_manager::file_tasks::SetOfficeFileMovedToOneDrive(profile,
                                                           base::Time());
  }
}

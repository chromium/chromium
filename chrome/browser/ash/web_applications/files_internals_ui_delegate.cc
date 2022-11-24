// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/files_internals_ui_delegate.h"

#include "base/values.h"
#include "chrome/browser/ash/file_manager/file_manager_pref_names.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/fusebox/fusebox_server.h"
#include "chrome/browser/profiles/profile.h"
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
  }
}

bool ChromeFilesInternalsUIDelegate::GetAlwaysMoveOfficeFiles() const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  return profile && file_manager::file_tasks::AlwaysMoveOfficeFiles(profile);
}

void ChromeFilesInternalsUIDelegate::SetAlwaysMoveOfficeFiles(
    bool always_move) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  if (profile) {
    file_manager::file_tasks::SetAlwaysMoveOfficeFiles(profile, always_move);
  }
}

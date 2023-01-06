// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/system_info/system_info_answer_result.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"

namespace app_list {

SystemInfoAnswerResult::SystemInfoAnswerResult(Profile* profile,
                                               const std::u16string& query,
                                               const std::string& url_path)
    : profile_(profile), url_path_(url_path) {
  SetDisplayType(DisplayType::kAnswerCard);
}

SystemInfoAnswerResult::~SystemInfoAnswerResult() = default;

void SystemInfoAnswerResult::Open(int event_flags) {
  // TODO(b/263994165): Check if this answer type is OS Settings.
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(profile_,
                                                               url_path_);
}

}  // namespace app_list

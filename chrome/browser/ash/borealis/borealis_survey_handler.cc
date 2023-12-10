// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_survey_handler.h"

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind_internal.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"

namespace borealis {

BorealisSurveyHandler::BorealisSurveyHandler(
    Profile* profile,
    BorealisWindowManager* window_manager)
    : profile_(profile) {
  if (!base::FeatureList::IsEnabled(ash::kHatsBorealisGamesSurvey.feature)) {
    VLOG(1) << "Borealis survey feature is not enabled";
    return;
  }
  lifetime_observation_.Observe(window_manager);
}
BorealisSurveyHandler::~BorealisSurveyHandler() = default;

std::optional<int> BorealisSurveyHandler::GetGameId(const std::string& app_id) {
  // Attempt to get the Borealis app ID.
  // TODO(b/173977876): Implement this in a more reliable way.
  std::optional<int> game_id;
  std::optional<guest_os::GuestOsRegistryService::Registration> registration =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_)
          ->GetRegistration(app_id);
  if (registration.has_value()) {
    game_id = borealis::ParseSteamGameId(registration->Exec());
  }
  return game_id;
}

base::flat_map<std::string, std::string> BorealisSurveyHandler::GetSurveyData(
    std::string owner_id,
    const std::string app_id,
    std::string window_title,
    std::optional<int> game_id) {
  // Number of monitors
  int internal_displays = 0;
  int external_displays = 0;
  for (const display::Display& d :
       display::Screen::GetScreen()->GetAllDisplays()) {
    if (d.IsInternal()) {
      internal_displays++;
    } else {
      external_displays++;
    }
  }

  // Proton/SLR versions
  borealis::CompatToolInfo compat_tool_info;
  std::string output;
  if (borealis::GetCompatToolInfo(owner_id, &output)) {
    compat_tool_info = borealis::ParseCompatToolInfo(game_id, output);
  } else {
    LOG(WARNING) << "Failed to get compat tool version info:";
    LOG(WARNING) << output;
  }

  // Steam GameID
  if (!game_id.has_value() && compat_tool_info.game_id.has_value()) {
    game_id = compat_tool_info.game_id.value();
  }
  std::string game_id_value = "";
  if (game_id.has_value()) {
    game_id_value = base::StringPrintf("%d", game_id.value());
  }

  base::flat_map<std::string, std::string> survey_data = {
      {"appName", window_title},
      {"board", base::SysInfo::HardwareModelName()},
      {"specs",
       base::StringPrintf("%ldGB; %s",
                          (long)(base::SysInfo::AmountOfPhysicalMemory() /
                                 (1000 * 1000 * 1000)),
                          base::SysInfo::CPUModelName().c_str())},
      {"monitorsInternal", base::NumberToString(internal_displays)},
      {"monitorsExternal", base::NumberToString(external_displays)},
      {"proton", compat_tool_info.proton},
      {"steam", compat_tool_info.slr},
      {"gameId", game_id_value}};
  return survey_data;
}

void BorealisSurveyHandler::OnAppFinished(const std::string& app_id,
                                          aura::Window* last_window) {
  if (IsNonGameBorealisApp(app_id)) {
    return;
  }
  if (ash::HatsNotificationController::ShouldShowSurveyToProfile(
          profile_, ash::kHatsBorealisGamesSurvey)) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, base::MayBlock(),
        base::BindOnce(&GetSurveyData,
                       ash::ProfileHelper::GetUserIdHashFromProfile(profile_),
                       std::string(app_id),
                       base::UTF16ToUTF8(last_window->GetTitle()),
                       GetGameId(app_id)),
        base::BindOnce(&BorealisSurveyHandler::CreateNotification,
                       weak_factory_.GetWeakPtr()));
  }
}

void BorealisSurveyHandler::CreateNotification(
    base::flat_map<std::string, std::string> survey_data) {
  hats_notification_controller_ = new ash::HatsNotificationController(
      profile_, ash::kHatsBorealisGamesSurvey, survey_data,
      l10n_util::GetStringUTF16(IDS_BOREALIS_HATS_TITLE),
      l10n_util::GetStringUTF16(IDS_BOREALIS_HATS_BODY));
}

void BorealisSurveyHandler::OnWindowManagerDeleted(
    BorealisWindowManager* window_manager) {
  lifetime_observation_.Reset();
}

}  // namespace borealis

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_manager.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/personalization_app/search/search_handler.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

namespace ash {
namespace personalization_app {

namespace {

const HatsConfig& GetHatsConfig(HatsSurveyType hats_survey_type) {
  switch (hats_survey_type) {
    case HatsSurveyType::kAvatar:
      return kHatsPersonalizationAvatarSurvey;
    case HatsSurveyType::kScreensaver:
      return kHatsPersonalizationScreensaverSurvey;
    case HatsSurveyType::kWallpaper:
      return kHatsPersonalizationWallpaperSurvey;
  }
}

}  // namespace

PersonalizationAppManager::PersonalizationAppManager(
    content::BrowserContext* context)
    : context_(context), search_handler_(std::make_unique<SearchHandler>()) {}

PersonalizationAppManager::~PersonalizationAppManager() = default;

void PersonalizationAppManager::MaybeStartHatsTimer(
    HatsSurveyType hats_survey_type) {
  if (hats_timer_.IsRunning() || hats_notification_controller_) {
    return;
  }

  if (::ash::HatsNotificationController::ShouldShowSurveyToProfile(
          Profile::FromBrowserContext(context_),
          GetHatsConfig(hats_survey_type))) {
    // |base::Unretained| is safe to use because |this| owns |hats_timer_|.
    hats_timer_.Start(
        FROM_HERE, base::Seconds(60),
        base::BindOnce(&PersonalizationAppManager::OnHatsTimerDone,
                       base::Unretained(this), hats_survey_type));
  }
}

void PersonalizationAppManager::OnHatsTimerDone(
    HatsSurveyType hats_survey_type) {
  const base::flat_map<std::string, std::string>& product_specific_data = {
      {"is_personalization_hub_enabled",
       ::ash::features::IsPersonalizationHubEnabled() ? "true" : "false"}};

  Profile* profile = Profile::FromBrowserContext(context_);
  const HatsConfig& config = GetHatsConfig(hats_survey_type);

  hats_notification_controller_ =
      base::MakeRefCounted<::ash::HatsNotificationController>(
          profile, config, product_specific_data);
}

void PersonalizationAppManager::Shutdown() {
  search_handler_.reset();
}

}  // namespace personalization_app
}  // namespace ash

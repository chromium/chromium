// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_manager.h"

#include <memory>

#include "ash/webui/personalization_app/search/search_handler.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/enterprise_policy_delegate_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy_factory.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace ash::personalization_app {

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

class PersonalizationAppManagerImpl : public PersonalizationAppManager {
 public:
  PersonalizationAppManagerImpl(
      content::BrowserContext* context,
      local_search_service::LocalSearchServiceProxy& local_search_service_proxy)
      : profile_(Profile::FromBrowserContext(context)) {
    DCHECK(profile_);
    search_handler_ = std::make_unique<SearchHandler>(
        local_search_service_proxy, profile_->GetPrefs(),
        std::make_unique<EnterprisePolicyDelegateImpl>(context));
  }

  ~PersonalizationAppManagerImpl() override = default;

  void MaybeStartHatsTimer(HatsSurveyType hats_survey_type) override {
    if (hats_timer_.IsRunning() || hats_notification_controller_) {
      return;
    }

    if (::ash::HatsNotificationController::ShouldShowSurveyToProfile(
            profile_, GetHatsConfig(hats_survey_type))) {
      // |base::Unretained| is safe to use because |this| owns |hats_timer_|.
      hats_timer_.Start(
          FROM_HERE, base::Seconds(60),
          base::BindOnce(&PersonalizationAppManagerImpl::OnHatsTimerDone,
                         base::Unretained(this), hats_survey_type));
    }
  }

  SearchHandler* search_handler() override { return search_handler_.get(); }

 private:
  // KeyedService:
  void Shutdown() override { search_handler_.reset(); }

  // Callback to |hats_timer_|. Will show the given survey type.
  void OnHatsTimerDone(HatsSurveyType hats_survey_type) {
    const base::flat_map<std::string, std::string>& product_specific_data = {
        {"is_personalization_hub_enabled", "true"}};

    const HatsConfig& config = GetHatsConfig(hats_survey_type);

    hats_notification_controller_ =
        base::MakeRefCounted<::ash::HatsNotificationController>(
            profile_, config, product_specific_data);
  }

  const raw_ptr<Profile> profile_;

  base::OneShotTimer hats_timer_;
  scoped_refptr<HatsNotificationController> hats_notification_controller_;

  // Handles running search queries for Personalization App features. Only set
  // if |PersonalizationHub| feature is enabled.
  std::unique_ptr<SearchHandler> search_handler_;
};

}  // namespace

// static
std::unique_ptr<PersonalizationAppManager> PersonalizationAppManager::Create(
    content::BrowserContext* context,
    local_search_service::LocalSearchServiceProxy& local_search_service_proxy) {
  return std::make_unique<PersonalizationAppManagerImpl>(
      context, local_search_service_proxy);
}

}  // namespace ash::personalization_app

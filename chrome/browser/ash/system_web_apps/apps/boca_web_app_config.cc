// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/boca_web_app_config.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/boca_ui/boca_ui.h"
#include "base/version_info/channel.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/channel/channel_info.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash {

namespace {

// Implementation of the BocaUIDelegate to expose some
// chrome functions to //chromeos.
class ChromeBocaUIDelegate : public ash::boca::BocaUIDelegate {
 public:
  explicit ChromeBocaUIDelegate(Profile* profile) : profile_(profile) {}
  ~ChromeBocaUIDelegate() override = default;
  ChromeBocaUIDelegate(const ChromeBocaUIDelegate&) = delete;
  ChromeBocaUIDelegate& operator=(const ChromeBocaUIDelegate&) = delete;

  // ash::boca::ChromeBocaUIDelegate:
  void PopulateLoadTimeData(content::WebUIDataSource* source) override {
    const user_manager::User* user =
        ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile_);
    const PrefService* pref_service = profile_->GetPrefs();
    version_info::Channel channel = ash::GetChannel();
    source->AddBoolean("isDevChannel",
                       channel == version_info::Channel::DEV ||
                           channel == version_info::Channel::UNKNOWN);
    source->AddBoolean("isProducer", ash::boca_util::IsProducer(user));
    source->AddBoolean("isConsumer", ash::boca_util::IsConsumer(user));
    // Do not honor sub-feature flag for student as there is no way to config
    // it for consumer.
    source->AddBoolean(
        "spotlightEnabled",
        ash::boca_util::IsConsumer(user) ||
            pref_service->GetBoolean(
                prefs::kClassManagementToolsViewScreenEligibilitySetting));
    source->AddString("appLocale", g_browser_process->GetApplicationLocale());
    source->AddBoolean(
        "classroomEnabled",
        pref_service->GetBoolean(
            prefs::kClassManagementToolsClassroomEligibilitySetting));
    source->AddBoolean(
        "captionEnabled",
        ash::boca_util::IsConsumer(user) ||
            pref_service->GetBoolean(
                prefs::kClassManagementToolsCaptionEligibilitySetting));
    if (features::IsBocaSpotlightEnabled()) {
      source->AddString("spotlightUrlTemplate",
                        features::kBocaSpotlightUrlTemplate.Get());
    }
    source->AddBoolean("sessionControlsUpdate",
                       features::IsBocaLockPauseUpdateEnabled());
    source->AddBoolean("navSettingsDialog",
                       features::IsBocaNavSettingsDialogEnabled());
    source->AddBoolean("captionToggle", features::IsBocaCaptionToggleEnabled());

    source->AddBoolean("spotlightNativeClientUpdate",
                       features::IsBocaSpotlightRobotRequesterEnabled());
    source->AddBoolean("userFeedbackAllowed",
                       pref_service->GetBoolean(::prefs::kUserFeedbackAllowed));
    if (features::IsBocaConfigureMaxStudentsEnabled()) {
      source->AddInteger("maxNumStudentsAllowed",
                         features::kBocaMaxNumStudentsAllowed.Get());
    }
    source->AddBoolean("screenSharingTeacher",
                       features::IsBocaScreenSharingTeacherEnabled());
    source->AddBoolean("screenSharingStudent",
                       features::IsBocaScreenSharingStudentEnabled());
  }

 private:
  const raw_ptr<Profile> profile_;
};
}  // namespace

BocaUIConfig::BocaUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  ash::boca::kChromeBocaAppHost) {}

bool BocaUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return ash::boca_util::IsEnabled(
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(
          browser_context));
}

std::unique_ptr<content::WebUIController> BocaUIConfig::CreateWebUIController(
    content::WebUI* web_ui,
    const GURL& url) {
  auto* profile = Profile::FromWebUI(web_ui);
  auto delegate = std::make_unique<ChromeBocaUIDelegate>(profile);
  return std::make_unique<ash::boca::BocaUI>(
      web_ui, std::move(delegate),
      ash::boca_util::IsProducer(
          ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile)));
}
}  // namespace ash

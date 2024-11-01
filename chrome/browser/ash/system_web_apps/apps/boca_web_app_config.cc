// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/boca_web_app_config.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_ui/boca_ui.h"
#include "base/version_info/channel.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
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
    version_info::Channel channel = chrome::GetChannel();
    source->AddBoolean("isDevChannel", channel == version_info::Channel::DEV);
    source->AddBoolean("isProducer", ash::boca_util::IsProducer(user));
    source->AddBoolean("isConsumer", ash::boca_util::IsConsumer(user));
    source->AddString("appLocale", g_browser_process->GetApplicationLocale());
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

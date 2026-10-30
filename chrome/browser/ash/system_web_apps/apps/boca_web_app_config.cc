// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/boca_web_app_config.h"

#include "ash/webui/boca_ui/boca_ui.h"
#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ash/boca/boca_manager.h"
#include "chrome/browser/ash/boca/boca_manager_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/boca/on_task/on_task_session_manager.h"
#include "chromeos/ash/components/boca/on_task/on_task_system_web_app_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/application_locale_storage/application_locale_storage.h"

namespace ash {

BocaUIConfig::BocaUIConfig(
    const ApplicationLocaleStorage* application_locale_storage)
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  ash::boca::kChromeBocaAppHost),
      application_locale_storage_(CHECK_DEREF(application_locale_storage)) {}

BocaUIConfig::~BocaUIConfig() = default;

bool BocaUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return ash::boca_util::IsEnabled(
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(
          browser_context));
}

std::unique_ptr<content::WebUIController> BocaUIConfig::CreateWebUIController(
    content::WebUI* web_ui,
    const GURL& url) {
  auto* profile = Profile::FromWebUI(web_ui);
  BocaManager* const boca_manager = BocaManagerFactory::GetForProfile(profile);
  ash::boca::OnTaskSessionManager* const on_task_session_manager =
      boca_manager->GetOnTaskSessionManager();
  const user_manager::User* const user =
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile);
  return std::make_unique<ash::boca::BocaUI>(
      web_ui, application_locale_storage_.get(),
      boca_manager->GetBocaSessionManager(), ash::boca_util::IsProducer(user),
      HostContentSettingsMapFactory::GetForProfile(profile),
      on_task_session_manager
          ? on_task_session_manager->GetOnTaskSystemWebAppManager()
          : nullptr,
      user->GetAccountId().GetGaiaId().ToString(),
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetURLLoaderFactory(), profile->GetPrefs());
}
}  // namespace ash

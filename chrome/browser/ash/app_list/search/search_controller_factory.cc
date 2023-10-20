// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_controller_factory.h"

#include <stddef.h>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/webui/help_app_ui/help_app_manager.h"
#include "ash/webui/help_app_ui/help_app_manager_factory.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/search/app_search_provider.h"
#include "chrome/browser/ash/app_list/search/app_zero_state_provider.h"
#include "chrome/browser/ash/app_list/search/arc/arc_app_shortcuts_search_provider.h"
#include "chrome/browser/ash/app_list/search/arc/arc_playstore_search_provider.h"
#include "chrome/browser/ash/app_list/search/assistant_text_search_provider.h"
#include "chrome/browser/ash/app_list/search/desks_admin_template_provider.h"
#include "chrome/browser/ash/app_list/search/files/drive_search_provider.h"
#include "chrome/browser/ash/app_list/search/files/file_search_provider.h"
#include "chrome/browser/ash/app_list/search/files/zero_state_drive_provider.h"
#include "chrome/browser/ash/app_list/search/files/zero_state_file_provider.h"
#include "chrome/browser/ash/app_list/search/games/game_provider.h"
#include "chrome/browser/ash/app_list/search/help_app_provider.h"
#include "chrome/browser/ash/app_list/search/help_app_zero_state_provider.h"
#include "chrome/browser/ash/app_list/search/keyboard_shortcut_provider.h"
#include "chrome/browser/ash/app_list/search/local_image_search/local_image_search_provider.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_lacros_provider.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_provider.h"
#include "chrome/browser/ash/app_list/search/os_settings_provider.h"
#include "chrome/browser/ash/app_list/search/personalization_provider.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/app_list/search/system_info/system_info_card_provider.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_manager.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_manager_factory.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/services/settings_manager/os_settings_manager.h"
#include "chrome/browser/ui/webui/ash/settings/services/settings_manager/os_settings_manager_factory.h"
#include "components/session_manager/core/session_manager.h"

namespace app_list {

namespace {

// Maximum number of results to show for the given type.
constexpr size_t kMaxAppShortcutResults = 4;
constexpr size_t kMaxPlayStoreResults = 12;

}  // namespace

std::unique_ptr<SearchController> CreateSearchController(
    Profile* profile,
    AppListModelUpdater* model_updater,
    AppListControllerDelegate* list_controller,
    ash::AppListNotifier* notifier,
    ash::federated::FederatedServiceController* federated_service_controller) {
  auto controller = std::make_unique<SearchController>(
      model_updater, list_controller, notifier, profile,
      federated_service_controller);
  controller->Initialize();

  // Add search providers.
  controller->AddProvider(std::make_unique<AppSearchProvider>(
      controller->GetAppSearchDataSource()));
  controller->AddProvider(std::make_unique<AppZeroStateProvider>(
      controller->GetAppSearchDataSource()));

  if (crosapi::browser_util::IsLacrosEnabled()) {
    controller->AddProvider(std::make_unique<OmniboxLacrosProvider>(
        profile, list_controller, crosapi::CrosapiManager::Get()));
  } else {
    controller->AddProvider(
        std::make_unique<OmniboxProvider>(profile, list_controller));
  }

  controller->AddProvider(std::make_unique<AssistantTextSearchProvider>());

  // File search providers are added only when not in guest session and running
  // on Chrome OS.
  if (!profile->IsGuestSession()) {
    controller->AddProvider(std::make_unique<FileSearchProvider>(profile));
    controller->AddProvider(std::make_unique<DriveSearchProvider>(profile));
    if (search_features::isLauncherSystemInfoAnswerCardsEnabled()) {
      controller->AddProvider(
          std::make_unique<SystemInfoCardProvider>(profile));
    }
    if (search_features::IsLauncherImageSearchEnabled()) {
      controller->AddProvider(
          std::make_unique<LocalImageSearchProvider>(profile));
    }
  }

  if (app_list_features::IsLauncherPlayStoreSearchEnabled()) {
    controller->AddProvider(std::make_unique<ArcPlayStoreSearchProvider>(
        kMaxPlayStoreResults, profile, list_controller));
  }

  if (arc::IsArcAllowedForProfile(profile)) {
    controller->AddProvider(std::make_unique<ArcAppShortcutsSearchProvider>(
        kMaxAppShortcutResults, profile, list_controller));
  }

  if (base::GetFieldTrialParamByFeatureAsBool(
          ash::features::kProductivityLauncher, "enable_continue", false)) {
    controller->AddProvider(std::make_unique<ZeroStateFileProvider>(profile));

    controller->AddProvider(std::make_unique<ZeroStateDriveProvider>(
        profile, controller.get(),
        drive::DriveIntegrationServiceFactory::GetForProfile(profile),
        session_manager::SessionManager::Get()));
  }

  auto* os_settings_manager =
      ash::settings::OsSettingsManagerFactory::GetForProfile(profile);
  auto* app_service_proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  if (os_settings_manager && app_service_proxy) {
    controller->AddProvider(std::make_unique<OsSettingsProvider>(
        profile, os_settings_manager->search_handler(),
        os_settings_manager->hierarchy()));
  }

  controller->AddProvider(std::make_unique<KeyboardShortcutProvider>(profile));

  if (base::FeatureList::IsEnabled(ash::features::kHelpAppLauncherSearch)) {
    controller->AddProvider(std::make_unique<HelpAppProvider>(
        profile,
        ash::help_app::HelpAppManagerFactory::GetForBrowserContext(profile)
            ->search_handler()));
  }

  controller->AddProvider(
      std::make_unique<HelpAppZeroStateProvider>(profile, notifier));

  if (base::FeatureList::IsEnabled(ash::features::kAppLaunchAutomation)) {
    controller->AddProvider(
        std::make_unique<DesksAdminTemplateProvider>(profile, list_controller));
  }

  if (search_features::IsLauncherGameSearchEnabled()) {
    controller->AddProvider(
        std::make_unique<GameProvider>(profile, list_controller));
  }

  if (ash::personalization_app::CanSeeWallpaperOrPersonalizationApp(profile)) {
    auto* personalization_app_manager = ash::personalization_app::
        PersonalizationAppManagerFactory::GetForBrowserContext(profile);
    DCHECK(personalization_app_manager);

    if (personalization_app_manager) {
      controller->AddProvider(std::make_unique<PersonalizationProvider>(
          profile, personalization_app_manager->search_handler()));
    }
  }

  return controller;
}

}  // namespace app_list

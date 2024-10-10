// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_controller_factory.h"

#include <stddef.h>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/files/file_enumerator.h"
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
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_provider.h"
#include "chrome/browser/ash/app_list/search/os_settings_provider.h"
#include "chrome/browser/ash/app_list/search/personalization_provider.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/app_list/search/system_info/system_info_card_provider.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
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
  controller->AddProvider(std::make_unique<OmniboxProvider>(
      profile, list_controller, LauncherSearchProviderTypes()));
  controller->AddProvider(std::make_unique<AssistantTextSearchProvider>());

  // File search providers are added only when not in guest session and running
  // on Chrome OS.
  if (!profile->IsGuestSession()) {
    controller->AddProvider(std::make_unique<FileSearchProvider>(
        profile, base::FileEnumerator::FileType::FILES |
                     base::FileEnumerator::FileType::DIRECTORIES));
    controller->AddProvider(std::make_unique<DriveSearchProvider>(profile));
    if (search_features::IsLauncherSystemInfoAnswerCardsEnabled()) {
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

  if (ash::features::IsLauncherContinueSectionWithRecentsEnabled() ||
      base::GetFieldTrialParamByFeatureAsBool(
          ash::features::kProductivityLauncher, "enable_continue", false)) {
    controller->AddProvider(std::make_unique<ZeroStateFileProvider>(profile));

    controller->AddProvider(std::make_unique<ZeroStateDriveProvider>(
        profile, controller.get(),
        drive::DriveIntegrationServiceFactory::GetForProfile(profile),
        session_manager::SessionManager::Get()));
  }

  controller->AddProvider(std::make_unique<OsSettingsProvider>(profile));

  controller->AddProvider(std::make_unique<KeyboardShortcutProvider>(profile));

  if (base::FeatureList::IsEnabled(ash::features::kHelpAppLauncherSearch)) {
    controller->AddProvider(std::make_unique<HelpAppProvider>(profile));
  }

  controller->AddProvider(
      std::make_unique<HelpAppZeroStateProvider>(profile, notifier));

  controller->AddProvider(
      std::make_unique<DesksAdminTemplateProvider>(profile, list_controller));

  if (search_features::IsLauncherGameSearchEnabled()) {
    controller->AddProvider(
        std::make_unique<GameProvider>(profile, list_controller));
  }

  if (ash::personalization_app::CanSeeWallpaperOrPersonalizationApp(profile)) {
    controller->AddProvider(std::make_unique<PersonalizationProvider>(profile));
  }

  return controller;
}

int LauncherSearchProviderTypes() {
  // We use all the default providers except for the document provider,
  // which suggests Drive files on enterprise devices. This is disabled to
  // avoid duplication with search results from DriveFS.
  int providers = AutocompleteClassifier::DefaultOmniboxProviders() &
                  ~AutocompleteProvider::TYPE_DOCUMENT;

  // The open tab provider is not included in the default providers, so add
  // it in manually.
  providers |= AutocompleteProvider::TYPE_OPEN_TAB;

  return providers;
}

}  // namespace app_list

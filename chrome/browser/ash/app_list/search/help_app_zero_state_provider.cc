// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/help_app_zero_state_provider.h"

#include <memory>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/public/cpp/style/color_provider.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/ash/release_notes/release_notes_storage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"

namespace app_list {
namespace {

constexpr char kHelpAppDiscoverResult[] = "help-app://discover";
constexpr char kHelpAppUpdatesResult[] = "help-app://updates";

// Decrements the times left to show the Discover Tab suggestion chip in
// PrefService.
void DecreaseTimesLeftToShowDiscoverTabSuggestionChip(Profile* profile) {
  const int times_left_to_show = profile->GetPrefs()->GetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow);
  profile->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, times_left_to_show - 1);
}

// Sets the times left to show the Discover Tab suggestion chip to 0 in
// PrefService.
void StopShowingDiscoverTabSuggestionChip(Profile* profile) {
  profile->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 0);
}

}  // namespace

HelpAppZeroStateResult::HelpAppZeroStateResult(Profile* profile,
                                               const std::string& id,
                                               DisplayType display_type,
                                               const std::u16string& title,
                                               const std::u16string& details,
                                               const gfx::ImageSkia& icon)
    : profile_(profile) {
  DCHECK(profile_);
  set_id(id);
  SetCategory(Category::kHelp);
  SetTitle(title);
  if (!details.empty())
    SetDetails(details);
  SetResultType(ResultType::kZeroStateHelpApp);
  SetDisplayType(display_type);
  // Some chips have different metrics types.
  if (id == kHelpAppDiscoverResult) {
    SetMetricsType(ash::HELP_APP_DISCOVER);
  } else if (id == kHelpAppUpdatesResult) {
    SetMetricsType(ash::HELP_APP_UPDATES);
  } else {
    SetMetricsType(ash::HELP_APP_DEFAULT);
  }
  SetChipIcon(icon);
}

HelpAppZeroStateResult::~HelpAppZeroStateResult() = default;

void HelpAppZeroStateResult::Open(int event_flags) {
  // Note: event_flags is ignored, LaunchSWA doesn't need it.
  if (id() == kHelpAppDiscoverResult) {
    StopShowingDiscoverTabSuggestionChip(profile_);

    // Launch discover tab suggestion chip.
    ash::SystemAppLaunchParams params;
    params.url = GURL("chrome://help-app/discover");
    params.launch_source = apps::LaunchSource::kFromAppListRecommendation;
    ash::LaunchSystemWebAppAsync(
        profile_, ash::SystemWebAppType::HELP, params,
        std::make_unique<apps::WindowInfo>(display::kDefaultDisplayId));
    // NOTE: Launching the result may dismiss the app list, which may delete
    // this result.
  } else if (id() == kHelpAppUpdatesResult) {
    // Launch release notes suggestion chip.
    base::RecordAction(
        base::UserMetricsAction("ReleaseNotes.SuggestionChipLaunched"));

    ash::ReleaseNotesStorage(profile_).StopShowingSuggestionChip();

    ash::SystemAppLaunchParams params;
    params.url = GURL("chrome://help-app/updates");
    params.launch_source = apps::LaunchSource::kFromAppListRecommendation;
    ash::LaunchSystemWebAppAsync(
        profile_, ash::SystemWebAppType::HELP, params,
        std::make_unique<apps::WindowInfo>(display::kDefaultDisplayId));
    // NOTE: Launching the result may dismiss the app list, which may delete
    // this result.
  }
}

HelpAppZeroStateProvider::HelpAppZeroStateProvider(
    Profile* profile,
    ash::AppListNotifier* notifier)
    : profile_(profile) {
  DCHECK(profile_);

  app_registry_cache_observer_.Observe(
      &apps::AppServiceProxyFactory::GetForProfile(profile)
           ->AppRegistryCache());
  LoadIcon();

  if (notifier) {
    notifier_observer_.Observe(notifier);
  }
}

HelpAppZeroStateProvider::~HelpAppZeroStateProvider() = default;

void HelpAppZeroStateProvider::StartZeroState() {
  SearchProvider::Results search_results;

  if (ash::ReleaseNotesStorage(profile_).ShouldShowSuggestionChip()) {
    // With productivity launcher enabled, release notes are shown in continue
    // section.
    auto* color_provider = ash::ColorProvider::Get();
    // NOTE: Color provider may not be set in unit tests.
    SkColor icon_color =
        color_provider
            ? color_provider->GetContentLayerColor(
                  ash::ColorProvider::ContentLayerType::kButtonIconColorPrimary)
            : gfx::kGoogleGrey900;
    gfx::ImageSkia icon = gfx::CreateVectorIcon(
        ash::kReleaseNotesChipIcon, app_list::kSystemIconDimension, icon_color);
    search_results.emplace_back(std::make_unique<HelpAppZeroStateResult>(
        profile_, kHelpAppUpdatesResult, DisplayType::kContinue,
        l10n_util::GetStringUTF16(IDS_HELP_APP_WHATS_NEW_CONTINUE_TASK_TITLE),
        l10n_util::GetStringUTF16(IDS_HELP_APP_WHATS_NEW_CONTINUE_TASK_DETAILS),
        icon));
  }

  SwapResults(&search_results);
}

ash::AppListSearchResultType HelpAppZeroStateProvider::ResultType() const {
  return ash::AppListSearchResultType::kZeroStateHelpApp;
}

void HelpAppZeroStateProvider::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() == web_app::kHelpAppId && update.ReadinessChanged() &&
      update.Readiness() == apps::Readiness::kReady) {
    LoadIcon();
  }
}

void HelpAppZeroStateProvider::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

void HelpAppZeroStateProvider::OnImpression(
    ash::AppListNotifier::Location location,
    const std::vector<ash::AppListNotifier::Result>& results,
    const std::u16string& query) {
  if (location != ash::AppListNotifier::Location::kContinue) {
    return;
  }

  for (const auto& result : results) {
    if (result.id == kHelpAppDiscoverResult) {
      DecreaseTimesLeftToShowDiscoverTabSuggestionChip(profile_);
    } else if (result.id == kHelpAppUpdatesResult) {
      ash::ReleaseNotesStorage(profile_)
          .DecreaseTimesLeftToShowSuggestionChip();
    }
  }
}

void HelpAppZeroStateProvider::OnLoadIcon(apps::IconValuePtr icon_value) {
  if (icon_value && icon_value->icon_type == apps::IconType::kStandard) {
    icon_ = icon_value->uncompressed;
  }
}

void HelpAppZeroStateProvider::LoadIcon() {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  proxy->LoadIcon(
      proxy->AppRegistryCache().GetAppType(web_app::kHelpAppId),
      web_app::kHelpAppId, apps::IconType::kStandard,
      ash::SharedAppListConfig::instance().suggestion_chip_icon_dimension(),
      /*allow_placeholder_icon=*/false,
      base::BindOnce(&HelpAppZeroStateProvider::OnLoadIcon,
                     weak_factory_.GetWeakPtr()));
}

}  // namespace app_list

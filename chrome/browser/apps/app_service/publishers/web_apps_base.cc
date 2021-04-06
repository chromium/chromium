// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/web_apps_base.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_util.h"
#endif

namespace {

constexpr char kTextPlain[] = "text/plain";

// Only supporting important permissions for now.
const ContentSettingsType kSupportedPermissionTypes[] = {
    ContentSettingsType::MEDIASTREAM_MIC,
    ContentSettingsType::MEDIASTREAM_CAMERA,
    ContentSettingsType::GEOLOCATION,
    ContentSettingsType::NOTIFICATIONS,
};

apps::mojom::InstallSource GetHighestPriorityInstallSource(
    const web_app::WebApp* web_app) {
  switch (web_app->GetHighestPrioritySource()) {
    case web_app::Source::kSystem:
      return apps::mojom::InstallSource::kSystem;
    case web_app::Source::kPolicy:
      return apps::mojom::InstallSource::kPolicy;
    case web_app::Source::kWebAppStore:
      return apps::mojom::InstallSource::kUser;
    case web_app::Source::kSync:
      return apps::mojom::InstallSource::kUser;
    case web_app::Source::kDefault:
      return apps::mojom::InstallSource::kDefault;
  }
}

apps::mojom::IntentFilterPtr CreateShareFileFilter(
    const std::vector<std::string>& intent_actions,
    const std::vector<std::string>& content_types) {
  DCHECK(!content_types.empty());
  auto intent_filter = apps::mojom::IntentFilter::New();

  std::vector<apps::mojom::ConditionValuePtr> action_condition_values;
  for (auto& action : intent_actions) {
    action_condition_values.push_back(apps_util::MakeConditionValue(
        action, apps::mojom::PatternMatchType::kNone));
  }
  if (!action_condition_values.empty()) {
    auto action_condition =
        apps_util::MakeCondition(apps::mojom::ConditionType::kAction,
                                 std::move(action_condition_values));
    intent_filter->conditions.push_back(std::move(action_condition));
  }

  std::vector<apps::mojom::ConditionValuePtr> mime_type_condition_values;
  for (auto& mime_type : content_types) {
    mime_type_condition_values.push_back(apps_util::MakeConditionValue(
        mime_type, apps::mojom::PatternMatchType::kMimeType));
  }
  if (!mime_type_condition_values.empty()) {
    auto mime_type_condition =
        apps_util::MakeCondition(apps::mojom::ConditionType::kMimeType,
                                 std::move(mime_type_condition_values));
    intent_filter->conditions.push_back(std::move(mime_type_condition));
  }

  return intent_filter;
}

}  // namespace

namespace apps {

WebAppsBase::WebAppsBase(
    const mojo::Remote<apps::mojom::AppService>& app_service,
    Profile* profile)
    : profile_(profile),
      app_service_(nullptr),
      app_type_(apps::mojom::AppType::kWeb) {
// After moving the ordinary Web Apps to Lacros chrome, the remaining web
// apps in ash Chrome will be only System Web Apps. Change the app type
// to kSystemWeb for this case and the kWeb app type will be published from
// the publisher for Lacros web apps.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (crosapi::browser_util::IsLacrosEnabled() &&
      base::FeatureList::IsEnabled(features::kLacrosWebApps)) {
    app_type_ = apps::mojom::AppType::kSystemWeb;
  }
#endif

  Initialize(app_service);
}

WebAppsBase::~WebAppsBase() = default;

void WebAppsBase::Shutdown() {
  if (provider_) {
    registrar_observer_.RemoveAll();
    content_settings_observer_.RemoveAll();
  }
}

const web_app::WebApp* WebAppsBase::GetWebApp(
    const web_app::AppId& app_id) const {
  // GetRegistrar() might return nullptr if the legacy bookmark apps registry is
  // enabled. This may happen in migration browser tests.
  return GetRegistrar() ? GetRegistrar()->GetAppById(app_id) : nullptr;
}

void WebAppsBase::OnWebAppInstalled(const web_app::AppId& app_id) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (web_app && Accepts(app_id)) {
    Publish(Convert(web_app, apps::mojom::Readiness::kReady), subscribers_);
  }
}

void WebAppsBase::OnWebAppWillBeUninstalled(const web_app::AppId& app_id) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app || !Accepts(app_id)) {
    return;
  }

  // Construct an App with only the information required to identify an
  // uninstallation.
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = app_type_;
  app->app_id = web_app->app_id();
  // TODO(loyso): Plumb uninstall source (reason) here.
  app->readiness = apps::mojom::Readiness::kUninstalledByUser;

  SetShowInFields(app, web_app);
  Publish(std::move(app), subscribers_);

  if (app_service_) {
    app_service_->RemovePreferredApp(app_type_, web_app->app_id());
  }
}

apps::mojom::AppPtr WebAppsBase::ConvertImpl(const web_app::WebApp* web_app,
                                             apps::mojom::Readiness readiness) {
  apps::mojom::AppPtr app = PublisherBase::MakeApp(
      app_type_, web_app->app_id(), readiness, web_app->name(),
      GetHighestPriorityInstallSource(web_app));

  app->description = web_app->description();
  app->additional_search_terms = web_app->additional_search_terms();
  app->last_launch_time = web_app->last_launch_time();
  app->install_time = web_app->install_time();

  // Web App's publisher_id the start url.
  app->publisher_id = web_app->start_url().spec();

  // app->version is left empty here.
  PopulatePermissions(web_app, &app->permissions);

  SetShowInFields(app, web_app);

  // Get the intent filters for PWAs.
  PopulateIntentFilters(*web_app, app->intent_filters);

  return app;
}

IconEffects WebAppsBase::GetIconEffects(const web_app::WebApp* web_app) {
  IconEffects icon_effects = IconEffects::kNone;
  if (!web_app->is_locally_installed()) {
    icon_effects =
        static_cast<IconEffects>(icon_effects | IconEffects::kBlocked);
  }
  icon_effects =
      static_cast<IconEffects>(icon_effects | IconEffects::kRoundCorners);
  return icon_effects;
}

content::WebContents* WebAppsBase::LaunchAppWithIntentImpl(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    int64_t display_id) {
  if (!profile_) {
    return nullptr;
  }

  const web_app::WebAppRegistrar& registrar = *WebAppsBase::GetRegistrar();
  if (registrar.GetAppById(app_id)->capture_links() ==
      blink::mojom::CaptureLinks::kExistingClientNavigate) {
    content::WebContents* web_contents =
        provider()->ui_manager().NavigateExistingWindow(
            app_id, intent->url ? intent->url.value()
                                : registrar.GetAppLaunchUrl(app_id));
    if (web_contents) {
      return web_contents;
    }
  }

  auto params = apps::CreateAppLaunchParamsForIntent(
      app_id, event_flags, GetAppLaunchSource(launch_source), display_id,
      web_app::ConvertDisplayModeToAppLaunchContainer(
          registrar.GetAppEffectiveDisplayMode(app_id)),
      std::move(intent));
  return LaunchAppWithParams(std::move(params));
}

content::WebContents* WebAppsBase::LaunchAppWithParams(AppLaunchParams params) {
  return web_app_launch_manager_->OpenApplication(std::move(params));
}

void WebAppsBase::Initialize(
    const mojo::Remote<apps::mojom::AppService>& app_service) {
  DCHECK(profile_);
  if (!web_app::AreWebAppsEnabled(profile_)) {
    return;
  }

  provider_ = web_app::WebAppProvider::Get(profile_);
  DCHECK(provider_);

  registrar_observer_.Add(&provider_->registrar());
  content_settings_observer_.Add(
      HostContentSettingsMapFactory::GetForProfile(profile_));

  web_app_launch_manager_ =
      std::make_unique<web_app::WebAppLaunchManager>(profile_);

  PublisherBase::Initialize(app_service, app_type_);
  app_service_ = app_service.get();
}

const web_app::WebAppRegistrar* WebAppsBase::GetRegistrar() const {
  DCHECK(provider_);
  return provider_->registrar().AsWebAppRegistrar();
}

void WebAppsBase::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  DCHECK(provider_);

  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebAppsBase::StartPublishingWebApps,
                                AsWeakPtr(), std::move(subscriber_remote)));
}

void WebAppsBase::LoadIcon(const std::string& app_id,
                           apps::mojom::IconKeyPtr icon_key,
                           apps::mojom::IconType icon_type,
                           int32_t size_hint_in_dip,
                           bool allow_placeholder_icon,
                           LoadIconCallback callback) {
  DCHECK(provider_);

  if (icon_key) {
    LoadIconFromWebApp(profile_, icon_type, size_hint_in_dip, app_id,
                       static_cast<IconEffects>(icon_key->icon_effects),
                       std::move(callback));
    return;
  }
  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void WebAppsBase::Launch(const std::string& app_id,
                         int32_t event_flags,
                         apps::mojom::LaunchSource launch_source,
                         apps::mojom::WindowInfoPtr window_info) {
  if (!profile_) {
    return;
  }

  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
    case apps::mojom::LaunchSource::kFromParentalControls:
      break;
    case apps::mojom::LaunchSource::kFromAppListGrid:
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      UMA_HISTOGRAM_ENUMERATION("Extensions.AppLaunch",
                                extension_misc::APP_LAUNCH_APP_LIST_MAIN,
                                extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);

      break;
    case apps::mojom::LaunchSource::kFromAppListQuery:
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
      UMA_HISTOGRAM_ENUMERATION("Extensions.AppLaunch",
                                extension_misc::APP_LAUNCH_APP_LIST_SEARCH,
                                extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);
      break;
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
    case apps::mojom::LaunchSource::kFromShelf:
    case apps::mojom::LaunchSource::kFromFileManager:
    case apps::mojom::LaunchSource::kFromLink:
    case apps::mojom::LaunchSource::kFromOmnibox:
    case apps::mojom::LaunchSource::kFromChromeInternal:
    case apps::mojom::LaunchSource::kFromKeyboard:
    case apps::mojom::LaunchSource::kFromOtherApp:
    case apps::mojom::LaunchSource::kFromMenu:
    case apps::mojom::LaunchSource::kFromInstalledNotification:
    case apps::mojom::LaunchSource::kFromTest:
    case apps::mojom::LaunchSource::kFromArc:
    case apps::mojom::LaunchSource::kFromSharesheet:
    case apps::mojom::LaunchSource::kFromReleaseNotesNotification:
    case apps::mojom::LaunchSource::kFromFullRestore:
    case apps::mojom::LaunchSource::kFromSmartTextContextMenu:
      break;
  }

  web_app::DisplayMode display_mode =
      GetRegistrar()->GetAppEffectiveDisplayMode(app_id);

  AppLaunchParams params = apps::CreateAppIdLaunchParamsWithEventFlags(
      web_app->app_id(), event_flags, GetAppLaunchSource(launch_source),
      window_info ? window_info->display_id : display::kInvalidDisplayId,
      /*fallback_container=*/
      web_app::ConvertDisplayModeToAppLaunchContainer(display_mode));

  // The app will be launched for the currently active profile.
  LaunchAppWithParams(std::move(params));
}

void WebAppsBase::LaunchAppWithFiles(const std::string& app_id,
                                     apps::mojom::LaunchContainer container,
                                     int32_t event_flags,
                                     apps::mojom::LaunchSource launch_source,
                                     apps::mojom::FilePathsPtr file_paths) {
  apps::AppLaunchParams params(
      app_id, container, ui::DispositionFromEventFlags(event_flags),
      GetAppLaunchSource(launch_source), display::kDefaultDisplayId);
  for (const auto& file_path : file_paths->file_paths) {
    params.launch_files.push_back(file_path);
  }

  // The app will be launched for the currently active profile.
  LaunchAppWithParams(std::move(params));
}

void WebAppsBase::LaunchAppWithIntent(const std::string& app_id,
                                      int32_t event_flags,
                                      apps::mojom::IntentPtr intent,
                                      apps::mojom::LaunchSource launch_source,
                                      apps::mojom::WindowInfoPtr window_info) {
  LaunchAppWithIntentImpl(
      app_id, event_flags, std::move(intent), launch_source,
      window_info ? window_info->display_id : display::kInvalidDisplayId);
}

void WebAppsBase::SetPermission(const std::string& app_id,
                                apps::mojom::PermissionPtr permission) {
  if (!profile_) {
    return;
  }

  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  DCHECK(host_content_settings_map);

  const GURL url = web_app->start_url();

  ContentSettingsType permission_type =
      static_cast<ContentSettingsType>(permission->permission_id);
  if (!base::Contains(kSupportedPermissionTypes, permission_type)) {
    return;
  }

  DCHECK_EQ(permission->value_type,
            apps::mojom::PermissionValueType::kTriState);
  ContentSetting permission_value = CONTENT_SETTING_DEFAULT;
  switch (static_cast<apps::mojom::TriState>(permission->value)) {
    case apps::mojom::TriState::kAllow:
      permission_value = CONTENT_SETTING_ALLOW;
      break;
    case apps::mojom::TriState::kAsk:
      permission_value = CONTENT_SETTING_ASK;
      break;
    case apps::mojom::TriState::kBlock:
      permission_value = CONTENT_SETTING_BLOCK;
      break;
    default:  // Return if value is invalid.
      return;
  }

  host_content_settings_map->SetContentSettingDefaultScope(
      url, url, permission_type, permission_value);
}

void WebAppsBase::OpenNativeSettings(const std::string& app_id) {
  if (!profile_) {
    return;
  }

  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  chrome::ShowSiteSettings(profile_, web_app->start_url());
}

void WebAppsBase::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  // If content_type is not one of the supported permissions, do nothing.
  if (!base::Contains(kSupportedPermissionTypes, content_type)) {
    return;
  }

  if (!profile_) {
    return;
  }

  const web_app::WebAppRegistrar* registrar = GetRegistrar();
  // Can be nullptr in tests.
  if (!registrar) {
    return;
  }

  for (const web_app::WebApp& web_app : registrar->GetApps()) {
    if (primary_pattern.Matches(web_app.start_url()) &&
        Accepts(web_app.app_id())) {
      apps::mojom::AppPtr app = apps::mojom::App::New();
      app->app_type = app_type_;
      app->app_id = web_app.app_id();
      PopulatePermissions(&web_app, &app->permissions);

      Publish(std::move(app), subscribers_);
    }
  }
}

void WebAppsBase::OnWebAppLastLaunchTimeChanged(
    const std::string& app_id,
    const base::Time& last_launch_time) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (web_app && Accepts(app_id)) {
    apps::mojom::AppPtr app = apps::mojom::App::New();
    app->app_type = app_type_;
    app->app_id = app_id;
    app->last_launch_time = web_app->last_launch_time();
    Publish(std::move(app), subscribers_);
  }
}

void WebAppsBase::OnWebAppManifestUpdated(const web_app::AppId& app_id,
                                          base::StringPiece old_name) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (web_app && Accepts(app_id)) {
    Publish(Convert(web_app, apps::mojom::Readiness::kReady), subscribers_);
  }
}

void WebAppsBase::OnAppRegistrarDestroyed() {
  registrar_observer_.RemoveAll();
}

void WebAppsBase::OnWebAppLocallyInstalledStateChanged(
    const web_app::AppId& app_id,
    bool is_locally_installed) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app)
    return;
  auto app = apps::mojom::App::New();
  app->app_type = app_type_;
  app->app_id = app_id;
  app->icon_key = icon_key_factory().MakeIconKey(GetIconEffects(web_app));
  Publish(std::move(app), subscribers_);
}

void WebAppsBase::SetShowInFields(apps::mojom::AppPtr& app,
                                  const web_app::WebApp* web_app) {
  if (web_app->chromeos_data().has_value()) {
    auto& chromeos_data = web_app->chromeos_data().value();
    app->show_in_launcher = chromeos_data.show_in_launcher
                                ? apps::mojom::OptionalBool::kTrue
                                : apps::mojom::OptionalBool::kFalse;
    app->show_in_shelf = app->show_in_search =
        chromeos_data.show_in_search ? apps::mojom::OptionalBool::kTrue
                                     : apps::mojom::OptionalBool::kFalse;
    app->show_in_management = chromeos_data.show_in_management
                                  ? apps::mojom::OptionalBool::kTrue
                                  : apps::mojom::OptionalBool::kFalse;
    return;
  }

  // Show the app everywhere by default.
  auto show = apps::mojom::OptionalBool::kTrue;
  app->show_in_launcher = show;
  app->show_in_shelf = show;
  app->show_in_search = show;
  app->show_in_management = show;
}

void WebAppsBase::PopulatePermissions(
    const web_app::WebApp* web_app,
    std::vector<mojom::PermissionPtr>* target) {
  const GURL url = web_app->start_url();

  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  DCHECK(host_content_settings_map);

  for (ContentSettingsType type : kSupportedPermissionTypes) {
    ContentSetting setting =
        host_content_settings_map->GetContentSetting(url, url, type);

    // Map ContentSettingsType to an apps::mojom::TriState value
    apps::mojom::TriState setting_val;
    switch (setting) {
      case CONTENT_SETTING_ALLOW:
        setting_val = apps::mojom::TriState::kAllow;
        break;
      case CONTENT_SETTING_ASK:
        setting_val = apps::mojom::TriState::kAsk;
        break;
      case CONTENT_SETTING_BLOCK:
        setting_val = apps::mojom::TriState::kBlock;
        break;
      default:
        setting_val = apps::mojom::TriState::kAsk;
    }

    content_settings::SettingInfo setting_info;
    host_content_settings_map->GetWebsiteSetting(url, url, type, &setting_info);

    auto permission = apps::mojom::Permission::New();
    permission->permission_id = static_cast<uint32_t>(type);
    permission->value_type = apps::mojom::PermissionValueType::kTriState;
    permission->value = static_cast<uint32_t>(setting_val);
    permission->is_managed =
        setting_info.source == content_settings::SETTING_SOURCE_POLICY;

    target->push_back(std::move(permission));
  }
}

void WebAppsBase::ConvertWebApps(apps::mojom::Readiness readiness,
                                 std::vector<apps::mojom::AppPtr>* apps_out) {
  const web_app::WebAppRegistrar* registrar = GetRegistrar();
  // Can be nullptr in tests.
  if (!registrar)
    return;

  for (const web_app::WebApp& web_app : registrar->GetApps()) {
    if (Accepts(web_app.app_id())) {
      apps_out->push_back(Convert(&web_app, readiness));
    }
  }
}

void WebAppsBase::StartPublishingWebApps(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote) {
  std::vector<apps::mojom::AppPtr> apps;
  ConvertWebApps(apps::mojom::Readiness::kReady, &apps);

  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps), app_type_,
                     true /* should_notify_initialized */);

  subscribers_.Add(std::move(subscriber));
}

void PopulateIntentFilters(const web_app::WebApp& web_app,
                           std::vector<mojom::IntentFilterPtr>& target) {
  if (web_app.scope().is_empty())
    return;

  target.push_back(apps_util::CreateIntentFilterForUrlScope(
      web_app.scope(),
      base::FeatureList::IsEnabled(features::kIntentHandlingSharing)));

  if (!base::FeatureList::IsEnabled(features::kIntentHandlingSharing) ||
      !web_app.share_target().has_value()) {
    return;
  }

  const apps::ShareTarget& share_target = web_app.share_target().value();

  if (!share_target.params.text.empty()) {
    // The share target accepts navigator.share() calls with text.
    target.push_back(
        CreateShareFileFilter({apps_util::kIntentActionSend}, {kTextPlain}));
  }

  std::vector<std::string> content_types;
  for (const auto& files_entry : share_target.params.files) {
    for (const auto& file_type : files_entry.accept) {
      // Skip any file_type that is not a MIME type.
      if (file_type.empty() || file_type[0] == '.' ||
          std::count(file_type.begin(), file_type.end(), '/') != 1) {
        continue;
      }

      content_types.push_back(file_type);
    }
  }

  if (!content_types.empty()) {
    const std::vector<std::string> intent_actions(
        {apps_util::kIntentActionSend, apps_util::kIntentActionSendMultiple});
    target.push_back(CreateShareFileFilter(intent_actions, content_types));
  }
}

}  // namespace apps

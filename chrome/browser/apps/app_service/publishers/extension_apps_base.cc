// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/one_shot_event.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_enable_flow.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_util.h"
#include "chrome/browser/ash/app_list/extension_app_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/ui_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/switches.h"
#include "ui/base/window_open_disposition_utils.h"
#include "url/url_constants.h"

// TODO(crbug.com/40569217): life cycle events. Extensions can be installed and
// uninstalled. ExtensionAppsBase should implement extensions::InstallObserver
// and be able to show download progress in the UI, a la
// ExtensionAppModelBuilder. This might involve using an
// extensions::InstallTracker. It might also need the equivalent of a
// ShelfExtensionAppUpdater.

// TODO(crbug.com/40569217): consider that, per khmel@, "in some places Chrome
// apps is not used and raw extension app without any effect is displayed...
// Search where ChromeAppIcon or ChromeAppIconLoader is used compared with
// direct loading the ExtensionIcon".

namespace {

apps::InstallReason GetInstallReason(const Profile* profile,
                                     const extensions::Extension* extension) {
  if (extensions::Manifest::IsComponentLocation(extension->location())) {
    return apps::InstallReason::kSystem;
  }

  if (extensions::Manifest::IsPolicyLocation(extension->location())) {
    return apps::InstallReason::kPolicy;
  }

  if (extension->was_installed_by_oem()) {
    return apps::InstallReason::kOem;
  }

  if (extension->was_installed_by_default()) {
    return apps::InstallReason::kDefault;
  }

  return apps::InstallReason::kUser;
}

}  // namespace

namespace apps {

ExtensionAppsBase::ExtensionAppsBase(AppServiceProxy* proxy, AppType app_type)
    : AppPublisher(proxy), profile_(proxy->profile()), app_type_(app_type) {}

ExtensionAppsBase::~ExtensionAppsBase() = default;

void ExtensionAppsBase::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  // If the extension doesn't belong to this publisher, do nothing.
  if (!Accepts(extension)) {
    return;
  }

  enable_flow_map_.erase(extension->id());

  // Construct an App with only the information required to identify an
  // uninstallation.
  auto app = std::make_unique<App>(app_type(), extension->id());
  app->readiness = reason == extensions::UNINSTALL_REASON_MIGRATED
                       ? Readiness::kUninstalledByNonUser
                       : Readiness::kUninstalledByUser;
  AppPublisher::Publish(std::move(app));
}

void ExtensionAppsBase::SetShowInFields(const extensions::Extension* extension,
                                        App& app) {
  auto show =
      ShouldShow(extension, profile_) && ShouldShownInLauncher(extension);
  app.show_in_launcher = show;
  app.show_in_shelf = show;
  app.show_in_search = show;
  app.show_in_management = show;
  app.handles_intents = show;
}

AppPtr ExtensionAppsBase::CreateAppImpl(const extensions::Extension* extension,
                                        Readiness readiness) {
  auto install_reason = GetInstallReason(profile_, extension);
  auto app = AppPublisher::MakeApp(app_type(), extension->id(), readiness,
                                   extension->name(), install_reason,
                                   install_reason == InstallReason::kSystem
                                       ? InstallSource::kSystem
                                       : InstallSource::kChromeWebStore);
  app->short_name = extension->short_name();
  app->description = extension->description();
  app->version = extension->GetVersionForDisplay();
  app->installer_package_id =
      apps::PackageId(apps::PackageType::kChromeApp, extension->id());
  app->policy_ids = {extension->id()};

  if (profile_) {
    auto* prefs = extensions::ExtensionPrefs::Get(profile_);
    if (prefs && prefs->GetInstalledExtensionInfo(extension->id())) {
      app->last_launch_time = prefs->GetLastLaunchTime(extension->id());
      // TODO(anunoy): Determine if this value should be set to the extension's
      // first install time vs last update time.
      app->install_time = prefs->GetLastUpdateTime(extension->id());
    }
  }

  app->is_platform_app = extension->is_platform_app();

  SetShowInFields(extension, *app);

  const extensions::ManagementPolicy* policy =
      extensions::ExtensionSystem::Get(profile())->management_policy();
  DCHECK(policy);
  app->allow_uninstall = policy->UserMayModifySettings(extension, nullptr) &&
                         !policy->MustRemainInstalled(extension, nullptr);
  app->allow_close = true;
  return app;
}

IconEffects ExtensionAppsBase::GetIconEffects(
    const extensions::Extension* extension) {
  IconEffects icon_effects = IconEffects::kNone;
  if (!extensions::util::IsAppLaunchable(extension->id(), profile_)) {
    icon_effects =
        static_cast<IconEffects>(icon_effects | IconEffects::kBlocked);
  }

  return icon_effects;
}

content::WebContents* ExtensionAppsBase::LaunchAppWithIntentImpl(
    const std::string& app_id,
    int32_t event_flags,
    IntentPtr intent,
    LaunchSource launch_source,
    WindowInfoPtr window_info,
    LaunchCallback callback) {
  const auto* extension = MaybeGetExtension(app_id);
  if (!extension || !extensions::util::IsAppLaunchable(app_id, profile_)) {
    std::move(callback).Run(ConvertBoolToLaunchResult(/*success=*/false));
    return nullptr;
  }

  if (!extensions::util::IsAppLaunchableWithoutEnabling(app_id, profile_)) {
    RunExtensionEnableFlow(
        app_id, base::BindOnce(&ExtensionAppsBase::LaunchAppWithIntent,
                               weak_factory_.GetWeakPtr(), app_id, event_flags,
                               std::move(intent), launch_source,
                               std::move(window_info), std::move(callback)));
    return nullptr;
  }

  auto params = apps::CreateAppLaunchParamsForIntent(
      app_id, event_flags, launch_source,
      window_info ? window_info->display_id : display::kInvalidDisplayId,
      extensions::GetLaunchContainer(extensions::ExtensionPrefs::Get(profile_),
                                     extension),
      std::move(intent), profile_);
  std::move(callback).Run(ConvertBoolToLaunchResult(/*success=*/true));
  return LaunchImpl(std::move(params));
}

content::WebContents* ExtensionAppsBase::LaunchImpl(AppLaunchParams&& params) {
  return ::OpenApplication(profile_, std::move(params));
}

void ExtensionAppsBase::LaunchAppWithParamsImpl(AppLaunchParams&& params,
                                                LaunchCallback callback) {
  LaunchImpl(std::move(params));

  // TODO(crbug.com/40787924): Add launch return value.
  std::move(callback).Run(LaunchResult());
}

const extensions::Extension* ExtensionAppsBase::MaybeGetExtension(
    const std::string& app_id) {
  DCHECK(profile_);
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  DCHECK(registry);
  const extensions::Extension* extension =
      registry->GetInstalledExtension(app_id);
  if (!extension || !Accepts(extension)) {
    return nullptr;
  }

  return extension;
}

void ExtensionAppsBase::Initialize() {
  RegisterPublisher(app_type());

  prefs_observation_.Observe(extensions::ExtensionPrefs::Get(profile_));
  registry_observation_.Observe(extensions::ExtensionRegistry::Get(profile_));

  DCHECK(profile_);

  // Publish apps after all extensions have been loaded, to include all apps
  // including the disabled apps.
  extensions::ExtensionSystem::Get(profile_)->ready().Post(
      FROM_HERE, base::BindOnce(&ExtensionAppsBase::OnExtensionsReady,
                                weak_factory_.GetWeakPtr()));
}

AppLaunchParams ExtensionAppsBase::ModifyAppLaunchParams(
    const std::string& app_id,
    LaunchSource launch_source,
    AppLaunchParams params) {
  // Does nothing in this base class.
  return params;
}

void ExtensionAppsBase::OnExtensionsReady() {
  std::vector<AppPtr> apps;
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  CreateAppVector(registry->enabled_extensions(), Readiness::kReady, &apps);
  CreateAppVector(registry->disabled_extensions(), Readiness::kDisabledByUser,
                  &apps);
  CreateAppVector(registry->terminated_extensions(), Readiness::kTerminated,
                  &apps);
  AppPublisher::Publish(std::move(apps), app_type(),
                        /*should_notify_initialized=*/true);

  // blocklisted_extensions and blocked_extensions, corresponding to
  // kDisabledByBlocklist and kDisabledByPolicy, are deliberately ignored.
  //
  // If making changes to which sets are consulted, also change ShouldShow,
  // OnHideWebStoreIconPrefChanged.
}

void ExtensionAppsBase::LoadIcon(const std::string& app_id,
                                 const IconKey& icon_key,
                                 IconType icon_type,
                                 int32_t size_hint_in_dip,
                                 bool allow_placeholder_icon,
                                 apps::LoadIconCallback callback) {
  LoadIconFromExtension(icon_type, size_hint_in_dip, profile_, app_id,
                        static_cast<IconEffects>(icon_key.icon_effects),
                        std::move(callback));
}

void ExtensionAppsBase::Launch(const std::string& app_id,
                               int32_t event_flags,
                               LaunchSource launch_source,
                               WindowInfoPtr window_info) {
  const auto* extension = MaybeGetExtension(app_id);
  if (!extension || !extensions::util::IsAppLaunchable(app_id, profile_)) {
    return;
  }

  if (!extensions::util::IsAppLaunchableWithoutEnabling(app_id, profile_)) {
    RunExtensionEnableFlow(
        app_id, base::BindOnce(&ExtensionAppsBase::Launch,
                               weak_factory_.GetWeakPtr(), app_id, event_flags,
                               launch_source, std::move(window_info)));
    return;
  }

  switch (launch_source) {
    case apps::LaunchSource::kUnknown:
    case apps::LaunchSource::kFromParentalControls:
      break;
    case apps::LaunchSource::kFromAppListGrid:
    case apps::LaunchSource::kFromAppListGridContextMenu:
      extensions::RecordAppListMainLaunch(extension);
      break;
    case apps::LaunchSource::kFromAppListQuery:
    case apps::LaunchSource::kFromAppListQueryContextMenu:
      extensions::RecordAppListSearchLaunch(extension);
      break;
    case apps::LaunchSource::kFromAppListRecommendation:
    case apps::LaunchSource::kFromShelf:
    case apps::LaunchSource::kFromFileManager:
    case apps::LaunchSource::kFromLink:
    case apps::LaunchSource::kFromOmnibox:
    case apps::LaunchSource::kFromChromeInternal:
    case apps::LaunchSource::kFromKeyboard:
    case apps::LaunchSource::kFromOtherApp:
    case apps::LaunchSource::kFromMenu:
    case apps::LaunchSource::kFromInstalledNotification:
    case apps::LaunchSource::kFromTest:
    case apps::LaunchSource::kFromArc:
    case apps::LaunchSource::kFromSharesheet:
    case apps::LaunchSource::kFromReleaseNotesNotification:
    case apps::LaunchSource::kFromFullRestore:
    case apps::LaunchSource::kFromSmartTextContextMenu:
    case apps::LaunchSource::kFromDiscoverTabNotification:
    case apps::LaunchSource::kFromManagementApi:
    case apps::LaunchSource::kFromKiosk:
    case apps::LaunchSource::kFromCommandLine:
    case apps::LaunchSource::kFromBackgroundMode:
    case apps::LaunchSource::kFromNewTabPage:
    case apps::LaunchSource::kFromIntentUrl:
    case apps::LaunchSource::kFromOsLogin:
    case apps::LaunchSource::kFromProtocolHandler:
    case apps::LaunchSource::kFromUrlHandler:
    case apps::LaunchSource::kFromLockScreen:
    case apps::LaunchSource::kFromAppHomePage:
    case apps::LaunchSource::kFromReparenting:
    case apps::LaunchSource::kFromProfileMenu:
    case apps::LaunchSource::kFromSysTrayCalendar:
    case apps::LaunchSource::kFromInstaller:
    case apps::LaunchSource::kFromFirstRun:
    case apps::LaunchSource::kFromWelcomeTour:
    case apps::LaunchSource::kFromFocusMode:
    case apps::LaunchSource::kFromSparky:
    case apps::LaunchSource::kFromNavigationCapturing:
      break;
  }

  // The app will be created for the currently active profile.
  AppLaunchParams params = CreateAppLaunchParamsWithEventFlags(
      profile_, extension, event_flags, launch_source,
      window_info ? window_info->display_id : display::kInvalidDisplayId);
  LaunchImpl(ModifyAppLaunchParams(app_id, launch_source, std::move(params)));
}

void ExtensionAppsBase::LaunchAppWithFiles(
    const std::string& app_id,
    int32_t event_flags,
    LaunchSource launch_source,
    std::vector<base::FilePath> file_paths) {
  const auto* extension = MaybeGetExtension(app_id);
  AppLaunchParams params(
      app_id,
      extensions::GetLaunchContainer(extensions::ExtensionPrefs::Get(profile_),
                                     extension),
      ui::DispositionFromEventFlags(event_flags), launch_source,
      display::kDefaultDisplayId);
  params.launch_files = std::move(file_paths);
  LaunchImpl(std::move(params));
}

void ExtensionAppsBase::LaunchAppWithIntent(const std::string& app_id,
                                            int32_t event_flags,
                                            IntentPtr intent,
                                            LaunchSource launch_source,
                                            WindowInfoPtr window_info,
                                            LaunchCallback callback) {
  LaunchAppWithIntentImpl(app_id, event_flags, std::move(intent), launch_source,
                          std::move(window_info), std::move(callback));
}

void ExtensionAppsBase::LaunchAppWithParams(AppLaunchParams&& params,
                                            LaunchCallback callback) {
  auto app_id = params.app_id;
  const auto* extension = MaybeGetExtension(app_id);
  if (!extension || !extensions::util::IsAppLaunchable(app_id, profile_)) {
    return;
  }

  if (!extensions::util::IsAppLaunchableWithoutEnabling(app_id, profile_)) {
    RunExtensionEnableFlow(
        app_id, base::BindOnce(&ExtensionAppsBase::LaunchAppWithParams,
                               weak_factory_.GetWeakPtr(), std::move(params),
                               std::move(callback)));
    return;
  }

  LaunchAppWithParamsImpl(std::move(params), std::move(callback));
}

void ExtensionAppsBase::Uninstall(const std::string& app_id,
                                  UninstallSource uninstall_source,
                                  bool clear_site_data,
                                  bool report_abuse) {
  // TODO(crbug.com/40100977): We need to add the error code, which could be
  // used by ExtensionFunction, ManagementUninstallFunctionBase on the callback
  // OnExtensionUninstallDialogClosed
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionRegistry::Get(profile())->GetInstalledExtension(
          app_id);
  if (!extension.get()) {
    return;
  }

  // If the extension doesn't belong to this publisher, do nothing.
  if (!Accepts(extension.get())) {
    return;
  }

  std::u16string error;
  extensions::ExtensionSystem::Get(profile())
      ->extension_service()
      ->UninstallExtension(
          app_id, GetExtensionUninstallReason(uninstall_source), &error);

  if (!report_abuse) {
    return;
  }

  // If the extension specifies a custom uninstall page via
  // chrome.runtime.setUninstallURL, then at uninstallation its uninstall
  // page opens. To ensure that the CWS Report Abuse page is the active
  // tab at uninstallation, navigates to the url to report abuse.
  constexpr char kReferrerId[] = "chrome-remove-extension-dialog";
  NavigateParams params(
      profile(), extension_urls::GetWebstoreReportAbuseUrl(app_id, kReferrerId),
      ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

void ExtensionAppsBase::OpenNativeSettings(const std::string& app_id) {
  const auto* extension = MaybeGetExtension(app_id);
  if (!extension) {
    return;
  }

  if (extension->is_hosted_app()) {
    chrome::ShowSiteSettings(
        profile_, extensions::AppLaunchInfo::GetFullLaunchURL(extension));

  } else if (extensions::ui_util::ShouldDisplayInExtensionSettings(
                 *extension)) {
    Browser* browser = chrome::FindTabbedBrowser(profile_, false);
    if (!browser) {
      browser = Browser::Create(Browser::CreateParams(profile_, true));
    }

    chrome::ShowExtensions(browser, extension->id());
  }
}

void ExtensionAppsBase::OnExtensionLastLaunchTimeChanged(
    const std::string& app_id,
    const base::Time& last_launch_time) {
  const auto* extension = MaybeGetExtension(app_id);
  if (!extension) {
    return;
  }

  auto app = std::make_unique<App>(app_type(), extension->id());
  app->last_launch_time = last_launch_time;
  AppPublisher::Publish(std::move(app));
}

void ExtensionAppsBase::OnExtensionPrefsWillBeDestroyed(
    extensions::ExtensionPrefs* prefs) {
  DCHECK(prefs_observation_.IsObservingSource(prefs));
  prefs_observation_.Reset();
}

void ExtensionAppsBase::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (!Accepts(extension)) {
    return;
  }

  AppPublisher::Publish(CreateApp(extension, Readiness::kReady));
}

void ExtensionAppsBase::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  if (!Accepts(extension)) {
    return;
  }

  Readiness readiness = Readiness::kUnknown;

  switch (reason) {
    case extensions::UnloadedExtensionReason::DISABLE:
      readiness = Readiness::kDisabledByUser;
      break;
    case extensions::UnloadedExtensionReason::BLOCKLIST:
      readiness = Readiness::kDisabledByBlocklist;
      break;
    case extensions::UnloadedExtensionReason::TERMINATE:
      readiness = Readiness::kTerminated;
      break;
    case extensions::UnloadedExtensionReason::UNINSTALL:
      // App readiness will be updated by OnExtensionUninstalled(). We defer to
      // that method to ensure the correct kUninstalledBy* enum is set.
      return;
    default:
      return;
  }

  auto app = std::make_unique<App>(app_type(), extension->id());
  app->readiness = readiness;
  AppPublisher::Publish(std::move(app));
}

void ExtensionAppsBase::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    bool is_update) {
  // If the extension doesn't belong to this publisher, do nothing.
  if (!Accepts(extension)) {
    return;
  }

  // TODO(crbug.com/40569217): Does the is_update case need to be handled
  // differently? E.g. by only passing through fields that have changed.
  AppPublisher::Publish(CreateApp(extension, Readiness::kReady));
}

bool ExtensionAppsBase::RunExtensionEnableFlow(const std::string& app_id,
                                               base::OnceClosure callback) {
  if (extensions::util::IsAppLaunchableWithoutEnabling(app_id, profile_)) {
    return false;
  }

  if (enable_flow_map_.find(app_id) == enable_flow_map_.end()) {
    enable_flow_map_[app_id] =
        std::make_unique<ExtensionAppsEnableFlow>(profile_, app_id);
  }

  enable_flow_map_[app_id]->Run(
      base::BindOnce(&ExtensionAppsBase::ExtensionEnableFlowFinished,
                     weak_factory_.GetWeakPtr(), std::move(callback), app_id));
  return true;
}

void ExtensionAppsBase::ExtensionEnableFlowFinished(base::OnceClosure callback,
                                                    const std::string& app_id,
                                                    bool enabled) {
  // If the extension was not enabled, we intentionally drop the callback on the
  // floor and do nothing with it.
  if (enabled) {
    std::move(callback).Run();
  }
  enable_flow_map_.erase(app_id);
}

// static
bool ExtensionAppsBase::ShouldShow(const extensions::Extension* extension,
                                   Profile* profile) {
  if (!profile) {
    return false;
  }

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  const std::string& app_id = extension->id();
  // These three extension sets are the same three consulted by Connect.
  // Importantly, it will exclude previously installed but currently
  // uninstalled extensions.
  return registry->enabled_extensions().Contains(app_id) ||
         registry->disabled_extensions().Contains(app_id) ||
         registry->terminated_extensions().Contains(app_id);
}

void ExtensionAppsBase::CreateAppVector(
    const extensions::ExtensionSet& extensions,
    Readiness readiness,
    std::vector<AppPtr>* apps_out) {
  for (const auto& extension : extensions) {
    if (Accepts(extension.get())) {
      apps_out->push_back(CreateApp(extension.get(), readiness));
    }
  }
}

}  // namespace apps

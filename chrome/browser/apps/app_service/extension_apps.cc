// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/extension_apps.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/extensions/gfx_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/extension_app_utils.h"
#include "chrome/browser/ui/app_list/extension_uninstaller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/services/app_service/public/cpp/intent_filter_util.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "components/arc/arc_service_manager.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/switches.h"
#include "url/url_constants.h"

// TODO(crbug.com/826982): life cycle events. Extensions can be installed and
// uninstalled. ExtensionApps should implement extensions::InstallObserver and
// be able to show download progress in the UI, a la ExtensionAppModelBuilder.
// This might involve using an extensions::InstallTracker. It might also need
// the equivalent of a LauncherExtensionAppUpdater.

// TODO(crbug.com/826982): do we also need to watch prefs, the same as
// ExtensionAppModelBuilder?

// TODO(crbug.com/826982): consider that, per khmel@, "in some places Chrome
// apps is not used and raw extension app without any effect is displayed...
// Search where ChromeAppIcon or ChromeAppIconLoader is used compared with
// direct loading the ExtensionIcon".

namespace {

// Only supporting important permissions for now.
const ContentSettingsType kSupportedPermissionTypes[] = {
    ContentSettingsType::MEDIASTREAM_MIC,
    ContentSettingsType::MEDIASTREAM_CAMERA,
    ContentSettingsType::GEOLOCATION,
    ContentSettingsType::NOTIFICATIONS,
};

std::string GetSourceFromAppListSource(ash::ShelfLaunchSource source) {
  switch (source) {
    case ash::LAUNCH_FROM_APP_LIST:
      return std::string(extension_urls::kLaunchSourceAppList);
    case ash::LAUNCH_FROM_APP_LIST_SEARCH:
      return std::string(extension_urls::kLaunchSourceAppListSearch);
    default:
      return std::string();
  }
}

ash::ShelfLaunchSource ConvertLaunchSource(
    apps::mojom::LaunchSource launch_source) {
  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
    case apps::mojom::LaunchSource::kFromParentalControls:
      return ash::LAUNCH_FROM_UNKNOWN;
    case apps::mojom::LaunchSource::kFromAppListGrid:
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      return ash::LAUNCH_FROM_APP_LIST;
    case apps::mojom::LaunchSource::kFromAppListQuery:
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
      return ash::LAUNCH_FROM_APP_LIST_SEARCH;
    case apps::mojom::LaunchSource::kFromShelf:
      return ash::LAUNCH_FROM_SHELF;
    case apps::mojom::LaunchSource::kFromFileManager:
    case apps::mojom::LaunchSource::kFromLink:
    case apps::mojom::LaunchSource::kFromOmnibox:
      return ash::LAUNCH_FROM_UNKNOWN;
  }
}

apps::AppLaunchParams CreateAppLaunchParamsForIntent(
    const std::string& app_id,
    const apps::mojom::IntentPtr& intent) {
  apps::AppLaunchParams params(
      app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      apps::mojom::AppLaunchSource::kSourceNone);

  if (intent->scheme.has_value() && intent->host.has_value() &&
      intent->path.has_value()) {
    params.source = apps::mojom::AppLaunchSource::kSourceIntentUrl;
    params.override_url =
        GURL(intent->scheme.value() + url::kStandardSchemeSeparator +
             intent->host.value() + intent->path.value());
    DCHECK(params.override_url.is_valid());
  }

  return params;
}

// Get the LaunchId for a given |app_window|. Set launch_id default value to an
// empty string. If showInShelf parameter is true and the window key is not
// empty, its value is appended to the launch_id. Otherwise, if the window key
// is empty, the session_id is used.
std::string GetLaunchId(extensions::AppWindow* app_window) {
  std::string launch_id;
  if (app_window->extension_id() == extension_misc::kChromeCameraAppId)
    return launch_id;

  if (app_window->show_in_shelf()) {
    if (!app_window->window_key().empty()) {
      launch_id = app_window->window_key();
    } else {
      launch_id = base::StringPrintf("%d", app_window->session_id().id());
    }
  }
  return launch_id;
}

}  // namespace

namespace apps {

// Attempts to enable and launch an extension app.
class ExtensionAppsEnableFlow : public ExtensionEnableFlowDelegate {
 public:
  ExtensionAppsEnableFlow(Profile* profile, const std::string& app_id)
      : profile_(profile), app_id_(app_id) {}

  ~ExtensionAppsEnableFlow() override {}

  using Callback = base::OnceCallback<void()>;

  void Run(Callback callback) {
    callback_ = std::move(callback);

    if (!flow_) {
      flow_ = std::make_unique<ExtensionEnableFlow>(profile_, app_id_, this);
      flow_->StartForNativeWindow(nullptr);
    }
  }

 private:
  // ExtensionEnableFlowDelegate overrides.
  void ExtensionEnableFlowFinished() override {
    flow_.reset();
    // Automatically launch app after enabling.
    if (!callback_.is_null()) {
      std::move(callback_).Run();
    }
  }

  void ExtensionEnableFlowAborted(bool user_initiated) override {
    flow_.reset();
  }

  Profile* profile_;
  std::string app_id_;
  Callback callback_;
  std::unique_ptr<ExtensionEnableFlow> flow_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAppsEnableFlow);
};

void ExtensionApps::RecordUninstallCanceledAction(Profile* profile,
                                                  const std::string& app_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id);
  if (!extension) {
    return;
  }

  if (extension->from_bookmark()) {
    UMA_HISTOGRAM_ENUMERATION(
        "Webapp.UninstallDialogAction",
        extensions::ExtensionUninstallDialog::CLOSE_ACTION_CANCELED,
        extensions::ExtensionUninstallDialog::CLOSE_ACTION_LAST);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Extensions.UninstallDialogAction",
        extensions::ExtensionUninstallDialog::CLOSE_ACTION_CANCELED,
        extensions::ExtensionUninstallDialog::CLOSE_ACTION_LAST);
  }
}

ExtensionApps::ExtensionApps(
    const mojo::Remote<apps::mojom::AppService>& app_service,
    Profile* profile,
    apps::mojom::AppType app_type,
    apps::InstanceRegistry* instance_registry)
    : profile_(profile),
      app_type_(app_type),
      instance_registry_(instance_registry),
      app_service_(nullptr) {
  Initialize(app_service);
}

ExtensionApps::~ExtensionApps() {
  app_window_registry_.RemoveAll();

  // In unit tests, AppServiceProxy might be ReInitializeForTesting, so
  // ExtensionApps might be destroyed without calling Shutdown, so arc_prefs_
  // needs to be removed from observer in the destructor function.
  if (arc_prefs_) {
    arc_prefs_->RemoveObserver(this);
    arc_prefs_ = nullptr;
  }
}

void ExtensionApps::FlushMojoCallsForTesting() {
  receiver_.FlushForTesting();
}

void ExtensionApps::Shutdown() {
  if (arc_prefs_) {
    arc_prefs_->RemoveObserver(this);
    arc_prefs_ = nullptr;
  }

  if (profile_) {
    content_settings_observer_.RemoveAll();
  }
}

void ExtensionApps::ObserveArc() {
  // Observe the ARC apps to set the badge on the equivalent Chrome app's icon.
  if (arc_prefs_) {
    arc_prefs_->RemoveObserver(this);
  }

  arc_prefs_ = ArcAppListPrefs::Get(profile_);
  if (arc_prefs_) {
    arc_prefs_->AddObserver(this);
  }
}

void ExtensionApps::Initialize(
    const mojo::Remote<apps::mojom::AppService>& app_service) {
  DCHECK(profile_);
  DCHECK_NE(apps::mojom::AppType::kUnknown, app_type_);
  app_service->RegisterPublisher(receiver_.BindNewPipeAndPassRemote(),
                                 app_type_);

  prefs_observer_.Add(extensions::ExtensionPrefs::Get(profile_));
  registry_observer_.Add(extensions::ExtensionRegistry::Get(profile_));
  app_window_registry_.Add(extensions::AppWindowRegistry::Get(profile_));
  content_settings_observer_.Add(
      HostContentSettingsMapFactory::GetForProfile(profile_));
  app_service_ = app_service.get();
}

bool ExtensionApps::Accepts(const extensions::Extension* extension) {
  // Hangouts is a special extension, which shows an window, so it should be
  // added to the AppService to show the icon on the shelf, when launching the
  // hangouts.
  if (extension->id() == extension_misc::kProdHangoutsExtensionId) {
    return app_type_ == apps::mojom::AppType::kExtension;
  }

  if (!extension->is_app() || IsBlacklisted(extension->id())) {
    return false;
  }

  switch (app_type_) {
    case apps::mojom::AppType::kExtension:
      return !extension->from_bookmark();
    case apps::mojom::AppType::kWeb:
      return extension->from_bookmark();
    default:
      NOTREACHED();
      return false;
  }
}

void ExtensionApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;
  if (profile_) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile_);
    ConvertVector(registry->enabled_extensions(),
                  apps::mojom::Readiness::kReady, &apps);
    ConvertVector(registry->disabled_extensions(),
                  apps::mojom::Readiness::kDisabledByUser, &apps);
    ConvertVector(registry->terminated_extensions(),
                  apps::mojom::Readiness::kTerminated, &apps);
    // blacklisted_extensions and blocked_extensions, corresponding to
    // kDisabledByBlacklist and kDisabledByPolicy, are deliberately ignored.
    //
    // If making changes to which sets are consulted, also change ShouldShow.
  }
  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps));
  subscribers_.Add(std::move(subscriber));
}

void ExtensionApps::LoadIcon(const std::string& app_id,
                             apps::mojom::IconKeyPtr icon_key,
                             apps::mojom::IconCompression icon_compression,
                             int32_t size_hint_in_dip,
                             bool allow_placeholder_icon,
                             LoadIconCallback callback) {
  if (icon_key) {
    LoadIconFromExtension(icon_compression, size_hint_in_dip, profile_, app_id,
                          static_cast<IconEffects>(icon_key->icon_effects),
                          std::move(callback));
    return;
  }
  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void ExtensionApps::Launch(const std::string& app_id,
                           int32_t event_flags,
                           apps::mojom::LaunchSource launch_source,
                           int64_t display_id) {
  if (!profile_) {
    return;
  }

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
          app_id);
  if (!extension || !extensions::util::IsAppLaunchable(app_id, profile_) ||
      RunExtensionEnableFlow(app_id, event_flags, launch_source, display_id)) {
    return;
  }

  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
    case apps::mojom::LaunchSource::kFromParentalControls:
      break;
    case apps::mojom::LaunchSource::kFromAppListGrid:
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      extensions::RecordAppListMainLaunch(extension);
      break;
    case apps::mojom::LaunchSource::kFromAppListQuery:
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
      extensions::RecordAppListSearchLaunch(extension);
      break;
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
    case apps::mojom::LaunchSource::kFromShelf:
    case apps::mojom::LaunchSource::kFromFileManager:
    case apps::mojom::LaunchSource::kFromLink:
    case apps::mojom::LaunchSource::kFromOmnibox:
      break;
  }

  // The app will be created for the currently active profile.
  AppLaunchParams params = CreateAppLaunchParamsWithEventFlags(
      profile_, extension, event_flags,
      apps::mojom::AppLaunchSource::kSourceAppLauncher, display_id);
  ash::ShelfLaunchSource source = ConvertLaunchSource(launch_source);
  if ((source == ash::LAUNCH_FROM_APP_LIST ||
       source == ash::LAUNCH_FROM_APP_LIST_SEARCH) &&
      app_id == extensions::kWebStoreAppId) {
    // Get the corresponding source string.
    std::string source_value = GetSourceFromAppListSource(source);

    // Set an override URL to include the source.
    GURL extension_url = extensions::AppLaunchInfo::GetFullLaunchURL(extension);
    params.override_url = net::AppendQueryParameter(
        extension_url, extension_urls::kWebstoreSourceField, source_value);
  }

  apps::LaunchService::Get(profile_)->OpenApplication(params);
}

void ExtensionApps::LaunchAppWithIntent(const std::string& app_id,
                                        apps::mojom::IntentPtr intent,
                                        apps::mojom::LaunchSource launch_source,
                                        int64_t display_id) {
  if (!profile_) {
    return;
  }

  AppLaunchParams params = CreateAppLaunchParamsForIntent(app_id, intent);

  apps::LaunchService::Get(profile_)->OpenApplication(params);
}

void ExtensionApps::SetPermission(const std::string& app_id,
                                  apps::mojom::PermissionPtr permission) {
  if (!profile_) {
    return;
  }

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
          app_id);

  if (!extension->from_bookmark()) {
    return;
  }

  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  DCHECK(host_content_settings_map);

  const GURL url = extensions::AppLaunchInfo::GetFullLaunchURL(extension);

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
      url, url, permission_type, std::string() /* resource identifier */,
      permission_value);
}

void ExtensionApps::PromptUninstall(const std::string& app_id) {
  if (!profile_) {
    return;
  }

  // ExtensionUninstaller deletes itself when done or aborted.
  ExtensionUninstaller* uninstaller =
      new ExtensionUninstaller(profile_, app_id);
  uninstaller->Run();
}

void ExtensionApps::Uninstall(const std::string& app_id,
                              bool clear_site_data,
                              bool report_abuse) {
  // TODO(crbug.com/1009248): We need to add the error code, which could be used
  // by ExtensionFunction, ManagementUninstallFunctionBase on the callback
  // OnExtensionUninstallDialogClosed
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
          app_id);
  if (!extension) {
    return;
  }

  base::string16 error;
  extensions::ExtensionSystem::Get(profile_)
      ->extension_service()
      ->UninstallExtension(app_id, extensions::UNINSTALL_REASON_USER_INITIATED,
                           &error);

  if (extension->from_bookmark()) {
    if (!clear_site_data) {
      UMA_HISTOGRAM_ENUMERATION(
          "Webapp.UninstallDialogAction",
          extensions::ExtensionUninstallDialog::CLOSE_ACTION_UNINSTALL,
          extensions::ExtensionUninstallDialog::CLOSE_ACTION_LAST);
      return;
    }

    UMA_HISTOGRAM_ENUMERATION(
        "Webapp.UninstallDialogAction",
        extensions::ExtensionUninstallDialog::
            CLOSE_ACTION_UNINSTALL_AND_CHECKBOX_CHECKED,
        extensions::ExtensionUninstallDialog::CLOSE_ACTION_LAST);

    constexpr bool kClearCookies = true;
    constexpr bool kClearStorage = true;
    constexpr bool kClearCache = true;
    constexpr bool kAvoidClosingConnections = false;
    content::ClearSiteData(
        base::BindRepeating(
            [](content::BrowserContext* browser_context) {
              return browser_context;
            },
            base::Unretained(profile_)),
        url::Origin::Create(
            extensions::AppLaunchInfo::GetFullLaunchURL(extension)),
        kClearCookies, kClearStorage, kClearCache, kAvoidClosingConnections,
        base::DoNothing());
  } else {
    if (!report_abuse) {
      UMA_HISTOGRAM_ENUMERATION(
          "Extensions.UninstallDialogAction",
          extensions::ExtensionUninstallDialog::CLOSE_ACTION_UNINSTALL,
          extensions::ExtensionUninstallDialog::CLOSE_ACTION_LAST);
      return;
    }

    UMA_HISTOGRAM_ENUMERATION(
        "Extensions.UninstallDialogAction",
        extensions::ExtensionUninstallDialog::
            CLOSE_ACTION_UNINSTALL_AND_CHECKBOX_CHECKED,
        extensions::ExtensionUninstallDialog::CLOSE_ACTION_LAST);

    // If the extension specifies a custom uninstall page via
    // chrome.runtime.setUninstallURL, then at uninstallation its uninstall
    // page opens. To ensure that the CWS Report Abuse page is the active
    // tab at uninstallation, navigates to the url to report abuse.
    constexpr char kReferrerId[] = "chrome-remove-extension-dialog";
    NavigateParams params(
        profile_,
        extension_urls::GetWebstoreReportAbuseUrl(app_id, kReferrerId),
        ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    Navigate(&params);
  }
}

void ExtensionApps::PauseApp(const std::string& app_id) {
  if (paused_apps_.find(app_id) != paused_apps_.end()) {
    return;
  }

  paused_apps_.insert(app_id);
  SetIconEffect(app_id);

  // TODO(crbug.com/1011235): If the app is running, Stop the app.
}

void ExtensionApps::UnpauseApps(const std::string& app_id) {
  if (paused_apps_.find(app_id) == paused_apps_.end()) {
    return;
  }

  paused_apps_.erase(app_id);
  SetIconEffect(app_id);
}

void ExtensionApps::OpenNativeSettings(const std::string& app_id) {
  if (!profile_) {
    return;
  }

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
          app_id);

  if (!extension) {
    return;
  }

  if (extension->is_hosted_app()) {
    chrome::ShowSiteSettings(
        profile_, extensions::AppLaunchInfo::GetFullLaunchURL(extension));

  } else if (extension->ShouldDisplayInExtensionSettings()) {
    Browser* browser = chrome::FindTabbedBrowser(profile_, false);
    if (browser) {
      chrome::ShowExtensions(browser, extension->id());
    }
    // TODO(crbug.com/826982): Either create new browser if one isn't found, or
    // make a version of chrome::ShowExtensions which accepts a Profile
    // instead of a Browser, similar to chrome::ShowSiteSettings.
  }
}

void ExtensionApps::OnPreferredAppSet(
    const std::string& app_id,
    apps::mojom::IntentFilterPtr intent_filter,
    apps::mojom::IntentPtr intent) {
  NOTIMPLEMENTED();
}

void ExtensionApps::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  // If content_type is not one of the supported permissions, do nothing.
  if (!base::Contains(kSupportedPermissionTypes, content_type)) {
    return;
  }

  if (!profile_) {
    return;
  }

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);

  if (!registry) {
    return;
  }

  std::unique_ptr<extensions::ExtensionSet> extensions =
      registry->GenerateInstalledExtensionsSet(
          extensions::ExtensionRegistry::ENABLED |
          extensions::ExtensionRegistry::DISABLED |
          extensions::ExtensionRegistry::TERMINATED);

  for (const auto& extension : *extensions) {
    const GURL& url =
        extensions::AppLaunchInfo::GetFullLaunchURL(extension.get());

    if (extension->from_bookmark() && primary_pattern.Matches(url) &&
        Accepts(extension.get())) {
      apps::mojom::AppPtr app = apps::mojom::App::New();
      app->app_type = apps::mojom::AppType::kWeb;
      app->app_id = extension->id();
      PopulatePermissions(extension.get(), &app->permissions);

      Publish(std::move(app));
    }
  }
}

void ExtensionApps::OnAppWindowAdded(extensions::AppWindow* app_window) {
  RegisterInstance(app_window, InstanceState::kStarted);
}

void ExtensionApps::OnAppWindowShown(extensions::AppWindow* app_window,
                                     bool was_hidden) {
  RegisterInstance(app_window,
                   static_cast<InstanceState>(InstanceState::kStarted |
                                              InstanceState::kRunning));
}

void ExtensionApps::OnExtensionLastLaunchTimeChanged(
    const std::string& app_id,
    const base::Time& last_launch_time) {
  if (!profile_) {
    return;
  }

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  const extensions::Extension* extension =
      registry->GetInstalledExtension(app_id);
  if (!extension || !Accepts(extension)) {
    return;
  }

  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = app_type_;
  app->app_id = extension->id();
  app->last_launch_time = last_launch_time;

  Publish(std::move(app));
}

void ExtensionApps::OnExtensionPrefsWillBeDestroyed(
    extensions::ExtensionPrefs* prefs) {
  prefs_observer_.Remove(prefs);
}

void ExtensionApps::OnExtensionLoaded(content::BrowserContext* browser_context,
                                      const extensions::Extension* extension) {
  if (!Accepts(extension)) {
    return;
  }

  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = app_type_;
  app->app_id = extension->id();
  app->readiness = apps::mojom::Readiness::kReady;
  app->name = extension->name();
  Publish(std::move(app));
}

void ExtensionApps::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  if (!Accepts(extension)) {
    return;
  }

  apps::mojom::Readiness readiness = apps::mojom::Readiness::kUnknown;

  switch (reason) {
    case extensions::UnloadedExtensionReason::DISABLE:
      readiness = apps::mojom::Readiness::kDisabledByUser;
      break;
    case extensions::UnloadedExtensionReason::BLACKLIST:
      readiness = apps::mojom::Readiness::kDisabledByBlacklist;
      break;
    case extensions::UnloadedExtensionReason::TERMINATE:
      readiness = apps::mojom::Readiness::kTerminated;
      break;
    case extensions::UnloadedExtensionReason::UNINSTALL:
      readiness = apps::mojom::Readiness::kUninstalledByUser;
      break;
    default:
      return;
  }

  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = app_type_;
  app->app_id = extension->id();
  app->readiness = readiness;
  Publish(std::move(app));
}

void ExtensionApps::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    bool is_update) {
  // If the extension doesn't belong to this publisher, do nothing.
  if (!Accepts(extension)) {
    return;
  }

  // TODO(crbug.com/826982): Does the is_update case need to be handled
  // differently? E.g. by only passing through fields that have changed.
  Publish(Convert(extension, apps::mojom::Readiness::kReady));
}

void ExtensionApps::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  // If the extension doesn't belong to this publisher, do nothing.
  if (!Accepts(extension)) {
    return;
  }

  enable_flow_map_.erase(extension->id());
  paused_apps_.erase(extension->id());

  // Construct an App with only the information required to identify an
  // uninstallation.
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = app_type_;
  app->app_id = extension->id();
  app->readiness = apps::mojom::Readiness::kUninstalledByUser;

  SetShowInFields(app, extension, profile_);
  Publish(std::move(app));

  if (!app_service_) {
    return;
  }
  app_service_->RemovePreferredApp(app_type_, extension->id());
}

void ExtensionApps::Publish(apps::mojom::AppPtr app) {
  for (auto& subscriber : subscribers_) {
    std::vector<apps::mojom::AppPtr> apps;
    apps.push_back(app.Clone());
    subscriber->OnApps(std::move(apps));
  }
}

void ExtensionApps::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  ApplyChromeBadge(package_info.package_name);
}

void ExtensionApps::OnPackageRemoved(const std::string& package_name,
                                     bool uninstalled) {
  ApplyChromeBadge(package_name);
}

void ExtensionApps::OnPackageListInitialRefreshed() {
  if (!arc_prefs_) {
    return;
  }
  for (const auto& app_name : arc_prefs_->GetPackagesFromPrefs()) {
    ApplyChromeBadge(app_name);
  }
}

void ExtensionApps::OnArcAppListPrefsDestroyed() {
  arc_prefs_ = nullptr;
}

// static
bool ExtensionApps::IsBlacklisted(const std::string& app_id) {
  // We blacklist (meaning we don't publish the app, in the App Service sense)
  // some apps that are already published by other app publishers.
  //
  // This sense of "blacklist" is separate from the extension registry's
  // kDisabledByBlacklist concept, which is when SafeBrowsing will send out a
  // blacklist of malicious extensions to disable.

  // The Play Store is conceptually provided by the ARC++ publisher, but
  // because it (the Play Store icon) is also the UI for enabling Android apps,
  // we also want to show the app icon even before ARC++ is enabled. Prior to
  // the App Service, as a historical implementation quirk, the Play Store both
  // has an "ARC++ app" component and an "Extension app" component, and both
  // share the same App ID.
  //
  // In the App Service world, there should be a unique app publisher for any
  // given app. In this case, the ArcApps publisher publishes the Play Store
  // app, and the ExtensionApps publisher does not.
  return app_id == arc::kPlayStoreAppId;
}

// static
void ExtensionApps::SetShowInFields(apps::mojom::AppPtr& app,
                                    const extensions::Extension* extension,
                                    Profile* profile) {
  if (ShouldShow(extension, profile)) {
    auto show = app_list::ShouldShowInLauncher(extension, profile)
                    ? apps::mojom::OptionalBool::kTrue
                    : apps::mojom::OptionalBool::kFalse;
    app->show_in_launcher = show;
    app->show_in_search = show;
    app->show_in_management = show;

    if (show == apps::mojom::OptionalBool::kFalse) {
      return;
    }

    auto* web_app_provider = web_app::WebAppProvider::Get(profile);

    // WebAppProvider is null for SignInProfile
    if (!web_app_provider) {
      return;
    }

    if (web_app_provider->system_web_app_manager().IsSystemWebApp(
            extension->id())) {
      app->show_in_management = apps::mojom::OptionalBool::kFalse;
    }
  } else {
    app->show_in_launcher = apps::mojom::OptionalBool::kFalse;
    app->show_in_search = apps::mojom::OptionalBool::kFalse;
    app->show_in_management = apps::mojom::OptionalBool::kFalse;
  }
}

// static
bool ExtensionApps::ShouldShow(const extensions::Extension* extension,
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

void ExtensionApps::PopulatePermissions(
    const extensions::Extension* extension,
    std::vector<mojom::PermissionPtr>* target) {
  const GURL url = extensions::AppLaunchInfo::GetFullLaunchURL(extension);

  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  DCHECK(host_content_settings_map);

  for (ContentSettingsType type : kSupportedPermissionTypes) {
    ContentSetting setting = host_content_settings_map->GetContentSetting(
        url, url, type, std::string() /* resource_identifier */);

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
    host_content_settings_map->GetWebsiteSetting(url, url, type, std::string(),
                                                 &setting_info);

    auto permission = apps::mojom::Permission::New();
    permission->permission_id = static_cast<uint32_t>(type);
    permission->value_type = apps::mojom::PermissionValueType::kTriState;
    permission->value = static_cast<uint32_t>(setting_val);
    permission->is_managed =
        setting_info.source == content_settings::SETTING_SOURCE_POLICY;

    target->push_back(std::move(permission));
  }
}

apps::mojom::InstallSource GetInstallSource(
    const Profile* profile,
    const extensions::Extension* extension) {
  if (extensions::Manifest::IsComponentLocation(extension->location()) ||
      web_app::ExternallyInstalledWebAppPrefs::HasAppIdWithInstallSource(
          profile->GetPrefs(), extension->id(),
          web_app::ExternalInstallSource::kSystemInstalled)) {
    return apps::mojom::InstallSource::kSystem;
  }

  if (extensions::Manifest::IsPolicyLocation(extension->location()) ||
      web_app::ExternallyInstalledWebAppPrefs::HasAppIdWithInstallSource(
          profile->GetPrefs(), extension->id(),
          web_app::ExternalInstallSource::kExternalPolicy)) {
    return apps::mojom::InstallSource::kPolicy;
  }

  if (extension->was_installed_by_oem()) {
    return apps::mojom::InstallSource::kOem;
  }

  if (extension->was_installed_by_default() ||
      web_app::ExternallyInstalledWebAppPrefs::HasAppIdWithInstallSource(
          profile->GetPrefs(), extension->id(),
          web_app::ExternalInstallSource::kExternalDefault)) {
    return apps::mojom::InstallSource::kDefault;
  }

  return apps::mojom::InstallSource::kUser;
}

void ExtensionApps::PopulateIntentFilters(
    const base::Optional<GURL>& app_scope,
    std::vector<mojom::IntentFilterPtr>* target) {
  if (app_scope != base::nullopt) {
    target->push_back(
        apps_util::CreateIntentFilterForUrlScope(app_scope.value()));
  }
}

apps::mojom::AppPtr ExtensionApps::Convert(
    const extensions::Extension* extension,
    apps::mojom::Readiness readiness) {
  apps::mojom::AppPtr app = apps::mojom::App::New();

  app->app_type = app_type_;
  app->app_id = extension->id();
  app->readiness = readiness;
  app->name = extension->name();
  app->short_name = extension->short_name();
  app->description = extension->description();
  app->version = extension->GetVersionForDisplay();
  app->icon_key = icon_key_factory_.MakeIconKey(GetIconEffects(extension));

  if (profile_) {
    auto* prefs = extensions::ExtensionPrefs::Get(profile_);
    if (prefs) {
      app->last_launch_time = prefs->GetLastLaunchTime(extension->id());
      app->install_time = prefs->GetInstallTime(extension->id());
    }
  }

  // Extensions where |from_bookmark| is true wrap websites and use web
  // permissions.
  if (extension->from_bookmark()) {
    PopulatePermissions(extension, &app->permissions);
  }

  app->install_source = GetInstallSource(profile_, extension);

  app->is_platform_app = extension->is_platform_app()
                             ? apps::mojom::OptionalBool::kTrue
                             : apps::mojom::OptionalBool::kFalse;
  app->recommendable = apps::mojom::OptionalBool::kTrue;
  app->searchable = apps::mojom::OptionalBool::kTrue;
  app->paused = apps::mojom::OptionalBool::kFalse;
  SetShowInFields(app, extension, profile_);

  // Get the intent filters for PWAs.
  if (extension->from_bookmark()) {
    auto* web_app_provider = web_app::WebAppProvider::Get(profile_);

    if (web_app_provider) {
      PopulateIntentFilters(
          web_app_provider->registrar().GetAppScope(extension->id()),
          &app->intent_filters);
    }
  }

  return app;
}

void ExtensionApps::ConvertVector(const extensions::ExtensionSet& extensions,
                                  apps::mojom::Readiness readiness,
                                  std::vector<apps::mojom::AppPtr>* apps_out) {
  for (const auto& extension : extensions) {
    if (Accepts(extension.get())) {
      apps_out->push_back(Convert(extension.get(), readiness));
    }
  }
}

bool ExtensionApps::RunExtensionEnableFlow(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    int64_t display_id) {
  if (extensions::util::IsAppLaunchableWithoutEnabling(app_id, profile_)) {
    return false;
  }

  if (enable_flow_map_.find(app_id) == enable_flow_map_.end()) {
    enable_flow_map_[app_id] =
        std::make_unique<ExtensionAppsEnableFlow>(profile_, app_id);
  }

  enable_flow_map_[app_id]->Run(
      base::BindOnce(&ExtensionApps::Launch, weak_factory_.GetWeakPtr(), app_id,
                     event_flags, launch_source, display_id));
  return true;
}

IconEffects ExtensionApps::GetIconEffects(
    const extensions::Extension* extension) {
  IconEffects icon_effects = IconEffects::kNone;
#if defined(OS_CHROMEOS)
  icon_effects =
      static_cast<IconEffects>(icon_effects | IconEffects::kResizeAndPad);
  if (extensions::util::ShouldApplyChromeBadge(profile_, extension->id())) {
    icon_effects = static_cast<IconEffects>(icon_effects | IconEffects::kBadge);
  }
#endif
  if (!extensions::util::IsAppLaunchable(extension->id(), profile_)) {
    icon_effects = static_cast<IconEffects>(icon_effects | IconEffects::kGray);
  }
  if (extension->from_bookmark()) {
    icon_effects =
        static_cast<IconEffects>(icon_effects | IconEffects::kRoundCorners);
  }
  if (paused_apps_.find(extension->id()) != paused_apps_.end()) {
    icon_effects =
        static_cast<IconEffects>(icon_effects | IconEffects::kPaused);
  }
  return icon_effects;
}

void ExtensionApps::ApplyChromeBadge(const std::string& package_name) {
  const std::vector<std::string> extension_ids =
      extensions::util::GetEquivalentInstalledExtensions(profile_,
                                                         package_name);

  for (auto& app_id : extension_ids) {
    SetIconEffect(app_id);
  }
}

void ExtensionApps::SetIconEffect(const std::string& app_id) {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  DCHECK(registry);
  const extensions::Extension* extension =
      registry->GetInstalledExtension(app_id);
  if (!extension || !Accepts(extension)) {
    return;
  }

  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = app_type_;
  app->app_id = app_id;
  app->icon_key = icon_key_factory_.MakeIconKey(GetIconEffects(extension));
  Publish(std::move(app));
}

void ExtensionApps::RegisterInstance(extensions::AppWindow* app_window,
                                     InstanceState new_state) {
  if (!base::FeatureList::IsEnabled(features::kAppServiceInstanceRegistry)) {
    return;
  }

  if (!instance_registry_ || !app_window) {
    return;
  }
  const extensions::Extension* extension = app_window->GetExtension();
  if (!extension) {
    return;
  }
  if (!Accepts(extension)) {
    return;
  }

  InstanceState state = InstanceState::kUnknown;
  instance_registry_->ForOneInstance(
      app_window->GetNativeWindow(),
      [&state](const apps::InstanceUpdate& update) { state = update.State(); });

  // If |state| has been marked as |new_state|, we don't need to update.
  if ((state & new_state) == new_state) {
    return;
  }

  std::vector<std::unique_ptr<apps::Instance>> deltas;
  auto instance = std::make_unique<apps::Instance>(
      app_window->extension_id(), app_window->GetNativeWindow());
  instance->SetLaunchId(GetLaunchId(app_window));
  instance->UpdateState(static_cast<InstanceState>(state | new_state),
                        base::Time::Now());
  deltas.push_back(std::move(instance));
  instance_registry_->OnInstances(deltas);
}

}  // namespace apps

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/management/chrome_management_api_delegate.h"

#include <memory>
#include <utility>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "components/favicon/core/favicon_service.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/api/management/management_api_constants.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"
#include "extensions/common/api/management.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"

namespace {
using InstallOrLaunchWebAppCallback =
    extensions::ManagementAPIDelegate::InstallOrLaunchWebAppCallback;
using InstallOrLaunchWebAppResult =
    extensions::ManagementAPIDelegate::InstallOrLaunchWebAppResult;
using InstallableCheckResult = web_app::InstallableCheckResult;

class ManagementSetEnabledFunctionInstallPromptDelegate
    : public extensions::InstallPromptDelegate {
 public:
  ManagementSetEnabledFunctionInstallPromptDelegate(
      content::WebContents* web_contents,
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      base::OnceCallback<void(bool)> callback)
      : install_prompt_(new ExtensionInstallPrompt(web_contents)),
        callback_(std::move(callback)) {
    ExtensionInstallPrompt::PromptType type =
        ExtensionInstallPrompt::GetReEnablePromptTypeForExtension(
            browser_context, extension);
    install_prompt_->ShowDialog(
        base::BindOnce(&ManagementSetEnabledFunctionInstallPromptDelegate::
                           OnInstallPromptDone,
                       weak_factory_.GetWeakPtr()),
        extension, nullptr,
        std::make_unique<ExtensionInstallPrompt::Prompt>(type),
        ExtensionInstallPrompt::GetDefaultShowDialogCallback());
  }

  ManagementSetEnabledFunctionInstallPromptDelegate(
      const ManagementSetEnabledFunctionInstallPromptDelegate&) = delete;
  ManagementSetEnabledFunctionInstallPromptDelegate& operator=(
      const ManagementSetEnabledFunctionInstallPromptDelegate&) = delete;

  ~ManagementSetEnabledFunctionInstallPromptDelegate() override {}

 private:
  void OnInstallPromptDone(
      ExtensionInstallPrompt::DoneCallbackPayload payload) {
    // This dialog doesn't support the "withhold permissions" checkbox.
    DCHECK_NE(
        payload.result,
        ExtensionInstallPrompt::Result::ACCEPTED_WITH_WITHHELD_PERMISSIONS);
    std::move(callback_).Run(payload.result ==
                             ExtensionInstallPrompt::Result::ACCEPTED);
  }

  // Used for prompting to re-enable items with permissions escalation updates.
  std::unique_ptr<ExtensionInstallPrompt> install_prompt_;

  base::OnceCallback<void(bool)> callback_;

  base::WeakPtrFactory<ManagementSetEnabledFunctionInstallPromptDelegate>
      weak_factory_{this};
};

class ManagementUninstallFunctionUninstallDialogDelegate
    : public extensions::ExtensionUninstallDialog::Delegate,
      public extensions::UninstallDialogDelegate {
 public:
  ManagementUninstallFunctionUninstallDialogDelegate(
      extensions::ManagementUninstallFunctionBase* function,
      const extensions::Extension* target_extension,
      bool show_programmatic_uninstall_ui)
      : function_(function) {
    ChromeExtensionFunctionDetails details(function);
    extension_uninstall_dialog_ = extensions::ExtensionUninstallDialog::Create(
        Profile::FromBrowserContext(function->browser_context()),
        details.GetNativeWindowForUI(), this);
    bool uninstall_from_webstore =
        (function->extension() &&
         function->extension()->id() == extensions::kWebStoreAppId) ||
        function->source_url().DomainIs(
            extension_urls::GetNewWebstoreLaunchURL().host());
    extensions::UninstallSource source;
    extensions::UninstallReason reason;
    if (uninstall_from_webstore) {
      source = extensions::UNINSTALL_SOURCE_CHROME_WEBSTORE;
      reason = extensions::UNINSTALL_REASON_CHROME_WEBSTORE;
    } else if (function->source_context_type() ==
               extensions::mojom::ContextType::kWebUi) {
      source = extensions::UNINSTALL_SOURCE_CHROME_EXTENSIONS_PAGE;
      // TODO: Update this to a new reason; it shouldn't be lumped in with
      // other uninstalls if it's from the chrome://extensions page.
      reason = extensions::UNINSTALL_REASON_MANAGEMENT_API;
    } else {
      source = extensions::UNINSTALL_SOURCE_EXTENSION;
      reason = extensions::UNINSTALL_REASON_MANAGEMENT_API;
    }
    if (show_programmatic_uninstall_ui) {
      extension_uninstall_dialog_->ConfirmUninstallByExtension(
          target_extension, function->extension(), reason, source);
    } else {
      extension_uninstall_dialog_->ConfirmUninstall(target_extension, reason,
                                                    source);
    }
  }

  ManagementUninstallFunctionUninstallDialogDelegate(
      const ManagementUninstallFunctionUninstallDialogDelegate&) = delete;
  ManagementUninstallFunctionUninstallDialogDelegate& operator=(
      const ManagementUninstallFunctionUninstallDialogDelegate&) = delete;

  ~ManagementUninstallFunctionUninstallDialogDelegate() override {}

  // ExtensionUninstallDialog::Delegate implementation.
  void OnExtensionUninstallDialogClosed(bool did_start_uninstall,
                                        const std::u16string& error) override {
    function_->OnExtensionUninstallDialogClosed(did_start_uninstall, error);
  }

 private:
  raw_ptr<extensions::ManagementUninstallFunctionBase> function_;
  std::unique_ptr<extensions::ExtensionUninstallDialog>
      extension_uninstall_dialog_;
};

void OnGenerateAppForLinkCompleted(
    extensions::ManagementGenerateAppForLinkFunction* function,
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  const bool install_success =
      code == webapps::InstallResultCode::kSuccessNewInstall;
  function->FinishCreateWebApp(app_id, install_success);
}

class ChromeAppForLinkDelegate : public extensions::AppForLinkDelegate {
 public:
  ChromeAppForLinkDelegate() {}

  ChromeAppForLinkDelegate(const ChromeAppForLinkDelegate&) = delete;
  ChromeAppForLinkDelegate& operator=(const ChromeAppForLinkDelegate&) = delete;

  ~ChromeAppForLinkDelegate() override {}

  void OnFaviconForApp(
      extensions::ManagementGenerateAppForLinkFunction* function,
      content::BrowserContext* context,
      const std::string& title,
      const GURL& launch_url,
      const favicon_base::FaviconImageResult& image_result) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Avoid accessing the WebAppProvider when web apps are enabled in Lacros
    // (and thus disabled in Ash).
    if (web_app::IsWebAppsCrosapiEnabled()) {
      function->FinishCreateWebApp(std::string(),
                                   /*install_success=*/false);
      return;
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    // GenerateAppForLink API doesn't allow a manifest ID to be specified, so
    // just use the launch_url for both manifest ID and start URL. This is a
    // reasonable behavior for "DIY apps" generated for a specific URL but
    // should be fixed if used for installing existing "Crafted Apps" (ie.
    // apps with an existing manifest that should be used for updates).
    GURL start_url = launch_url;
    webapps::ManifestId manifest_id =
        web_app::GenerateManifestIdFromStartUrlOnly(start_url);
    auto web_app_info =
        std::make_unique<web_app::WebAppInstallInfo>(manifest_id, start_url);
    web_app_info->title = base::UTF8ToUTF16(title);
    web_app_info->display_mode = web_app::DisplayMode::kBrowser;
    web_app_info->user_display_mode = web_app::mojom::UserDisplayMode::kBrowser;

    if (!image_result.image.IsEmpty()) {
      web_app_info->icon_bitmaps.any[image_result.image.Width()] =
          image_result.image.AsBitmap();
    }

    auto* provider = web_app::WebAppProvider::GetForWebApps(
        Profile::FromBrowserContext(context));

    provider->scheduler().InstallFromInfoWithParams(
        std::move(web_app_info),
        /*overwrite_existing_manifest_fields=*/false,
        webapps::WebappInstallSource::MANAGEMENT_API,
        base::BindOnce(OnGenerateAppForLinkCompleted,
                       base::RetainedRef(function)),
        web_app::WebAppInstallParams());
  }

  extensions::api::management::ExtensionInfo CreateExtensionInfoFromWebApp(
      const extensions::ExtensionId& app_id,
      content::BrowserContext* context) override {
    auto* provider = web_app::WebAppProvider::GetForWebApps(
        Profile::FromBrowserContext(context));
    DCHECK(provider);
    const web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();

    extensions::api::management::ExtensionInfo info;
    info.id = app_id;
    info.name = registrar.GetAppShortName(app_id);
    info.enabled = registrar.IsInstallState(
        app_id, {web_app::proto::INSTALLED_WITH_OS_INTEGRATION});
    info.install_type =
        extensions::api::management::ExtensionInstallType::kOther;
    info.is_app = true;
    info.type = extensions::api::management::ExtensionType::kHostedApp;
    info.app_launch_url = registrar.GetAppStartUrl(app_id).spec();

    info.icons.emplace();
    std::vector<apps::IconInfo> manifest_icons =
        registrar.GetAppIconInfos(app_id);
    info.icons->reserve(manifest_icons.size());
    for (const apps::IconInfo& web_app_icon_info : manifest_icons) {
      extensions::api::management::IconInfo icon_info;
      if (web_app_icon_info.square_size_px)
        icon_info.size = *web_app_icon_info.square_size_px;
      icon_info.url = web_app_icon_info.url.spec();
      info.icons->push_back(std::move(icon_info));
    }

    switch (registrar.GetAppDisplayMode(app_id)) {
      case web_app::DisplayMode::kBrowser:
        info.launch_type =
            extensions::api::management::LaunchType::kOpenAsRegularTab;
        break;
      case web_app::DisplayMode::kMinimalUi:
      case web_app::DisplayMode::kStandalone:
        info.launch_type =
            extensions::api::management::LaunchType::kOpenAsWindow;
        break;
      case web_app::DisplayMode::kFullscreen:
        info.launch_type =
            extensions::api::management::LaunchType::kOpenFullScreen;
        break;
      // These modes are not supported by the extension app backend.
      case web_app::DisplayMode::kWindowControlsOverlay:
      case web_app::DisplayMode::kTabbed:
      case web_app::DisplayMode::kBorderless:
      case web_app::DisplayMode::kPictureInPicture:
      case web_app::DisplayMode::kUndefined:
        info.launch_type = extensions::api::management::LaunchType::kNone;
        break;
    }

    return info;
  }

  // Used for favicon loading tasks.
  base::CancelableTaskTracker cancelable_task_tracker_;
};

void LaunchWebApp(const webapps::AppId& app_id, Profile* profile) {
  // Look at prefs to find the right launch container. If the user has not set a
  // preference, the default launch value will be returned.
  // TODO(crbug.com/40098656): Make AppLaunchParams launch container Optional or
  // add a "default" launch container enum value.
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  DCHECK(provider);
  std::optional<web_app::mojom::UserDisplayMode> display_mode =
      provider->registrar_unsafe().GetAppUserDisplayMode(app_id);
  auto launch_container = apps::LaunchContainer::kLaunchContainerWindow;
  if (display_mode == web_app::mojom::UserDisplayMode::kBrowser) {
    launch_container = apps::LaunchContainer::kLaunchContainerTab;
  }

  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    // If the profile doesn't have an App Service Proxy available, that means
    // this extension has been explicitly permitted to run in an incognito
    // context. Treat this as if the extension is running in the original
    // profile, so it is allowed to access apps in the original profile.
    profile = profile->GetOriginalProfile();
  }

  apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithParams(
      apps::AppLaunchParams(app_id, launch_container,
                            WindowOpenDisposition::NEW_FOREGROUND_TAB,
                            apps::LaunchSource::kFromManagementApi));
}

void OnWebAppInstallCompleted(InstallOrLaunchWebAppCallback callback,
                              const webapps::AppId& app_id,
                              webapps::InstallResultCode code) {
  InstallOrLaunchWebAppResult result =
      IsSuccess(code) ? InstallOrLaunchWebAppResult::kSuccess
                      : InstallOrLaunchWebAppResult::kUnknownError;
  std::move(callback).Run(result);
}

void OnWebAppInstallabilityChecked(
    base::WeakPtr<Profile> profile,
    InstallOrLaunchWebAppCallback callback,
    std::unique_ptr<content::WebContents> web_contents,
    InstallableCheckResult result,
    std::optional<webapps::AppId> app_id) {
  if (!profile) {
    return;
  }
  switch (result) {
    case InstallableCheckResult::kAlreadyInstalled:
      DCHECK(app_id);
      LaunchWebApp(*app_id, profile.get());
      std::move(callback).Run(InstallOrLaunchWebAppResult::kSuccess);
      return;
    case InstallableCheckResult::kNotInstallable:
      std::move(callback).Run(InstallOrLaunchWebAppResult::kInvalidWebApp);
      return;
    case InstallableCheckResult::kInstallable:
      content::WebContents* containing_contents = web_contents.get();
      chrome::ScopedTabbedBrowserDisplayer displayer(profile.get());
      const GURL& url = web_contents->GetLastCommittedURL();
      chrome::AddWebContents(displayer.browser(), nullptr,
                             std::move(web_contents), url,
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             blink::mojom::WindowFeatures());
      web_app::CreateWebAppFromManifest(
          containing_contents, webapps::WebappInstallSource::MANAGEMENT_API,
          base::BindOnce(&OnWebAppInstallCompleted, std::move(callback)));
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

extensions::SupervisedUserExtensionsDelegate*
GetSupervisedUserExtensionsDelegateFromContext(
    content::BrowserContext* context) {
  extensions::SupervisedUserExtensionsDelegate*
      supervised_user_extensions_delegate =
          extensions::ManagementAPI::GetFactoryInstance()
              ->Get(context)
              ->GetSupervisedUserExtensionsDelegate();
  CHECK(supervised_user_extensions_delegate);
  return supervised_user_extensions_delegate;
}
}  // namespace

ChromeManagementAPIDelegate::ChromeManagementAPIDelegate() = default;

ChromeManagementAPIDelegate::~ChromeManagementAPIDelegate() = default;

bool ChromeManagementAPIDelegate::LaunchAppFunctionDelegate(
    const extensions::Extension* extension,
    content::BrowserContext* context) const {
  // Look at prefs to find the right launch container.
  // If the user has not set a preference, the default launch value will be
  // returned.
  // TODO(crbug.com/40098656): Make AppLaunchParams launch container Optional or
  // add a "default" launch container enum value.
  apps::LaunchContainer launch_container =
      GetLaunchContainer(extensions::ExtensionPrefs::Get(context), extension);
  Profile* profile = Profile::FromBrowserContext(context);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (extensions::IsExtensionUnsupportedDeprecatedApp(profile,
                                                      extension->id())) {
    return false;
  }
#endif
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    // If the profile doesn't have an App Service Proxy available, that means
    // this extension has been explicitly permitted to run in an incognito
    // context. Treat this as if the extension is running in the original
    // profile, so it is allowed to access apps in the original profile.
    profile = profile->GetOriginalProfile();
  }
  apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithParams(
      apps::AppLaunchParams(extension->id(), launch_container,
                            WindowOpenDisposition::NEW_FOREGROUND_TAB,
                            apps::LaunchSource::kFromManagementApi));

  extensions::RecordAppLaunchType(extension_misc::APP_LAUNCH_EXTENSION_API,
                                  extension->GetType());
  return true;
}

GURL ChromeManagementAPIDelegate::GetFullLaunchURL(
    const extensions::Extension* extension) const {
  return extensions::AppLaunchInfo::GetFullLaunchURL(extension);
}

extensions::LaunchType ChromeManagementAPIDelegate::GetLaunchType(
    const extensions::ExtensionPrefs* prefs,
    const extensions::Extension* extension) const {
  return extensions::GetLaunchType(prefs, extension);
}

std::unique_ptr<extensions::InstallPromptDelegate>
ChromeManagementAPIDelegate::SetEnabledFunctionDelegate(
    content::WebContents* web_contents,
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    base::OnceCallback<void(bool)> callback) const {
  return std::make_unique<ManagementSetEnabledFunctionInstallPromptDelegate>(
      web_contents, browser_context, extension, std::move(callback));
}

std::unique_ptr<extensions::UninstallDialogDelegate>
ChromeManagementAPIDelegate::UninstallFunctionDelegate(
    extensions::ManagementUninstallFunctionBase* function,
    const extensions::Extension* target_extension,
    bool show_programmatic_uninstall_ui) const {
  return std::unique_ptr<extensions::UninstallDialogDelegate>(
      new ManagementUninstallFunctionUninstallDialogDelegate(
          function, target_extension, show_programmatic_uninstall_ui));
}

bool ChromeManagementAPIDelegate::CreateAppShortcutFunctionDelegate(
    extensions::ManagementCreateAppShortcutFunction* function,
    const extensions::Extension* extension,
    std::string* error) const {
  Browser* browser = chrome::FindBrowserWithProfile(
      Profile::FromBrowserContext(function->browser_context()));
  if (!browser) {
    // Shouldn't happen if we have user gesture.
    *error = extension_management_api_constants::kNoBrowserToCreateShortcut;
    return false;
  }

  chrome::ShowCreateChromeAppShortcutsDialog(
      browser->window()->GetNativeWindow(), browser->profile(), extension,
      base::BindOnce(&extensions::ManagementCreateAppShortcutFunction::
                         OnCloseShortcutPrompt,
                     function));

  return true;
}

std::unique_ptr<extensions::AppForLinkDelegate>
ChromeManagementAPIDelegate::GenerateAppForLinkFunctionDelegate(
    extensions::ManagementGenerateAppForLinkFunction* function,
    content::BrowserContext* context,
    const std::string& title,
    const GURL& launch_url) const {
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(Profile::FromBrowserContext(context),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  DCHECK(favicon_service);

  ChromeAppForLinkDelegate* delegate = new ChromeAppForLinkDelegate;

  favicon_service->GetFaviconImageForPageURL(
      launch_url,
      base::BindOnce(&ChromeAppForLinkDelegate::OnFaviconForApp,
                     base::Unretained(delegate), base::RetainedRef(function),
                     context, title, launch_url),
      &delegate->cancelable_task_tracker_);

  return std::unique_ptr<extensions::AppForLinkDelegate>(delegate);
}

bool ChromeManagementAPIDelegate::CanContextInstallWebApps(
    content::BrowserContext* context) const {
  return web_app::AreWebAppsUserInstallable(
      Profile::FromBrowserContext(context));
}

void ChromeManagementAPIDelegate::InstallOrLaunchReplacementWebApp(
    content::BrowserContext* context,
    const GURL& web_app_url,
    InstallOrLaunchWebAppCallback callback) const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Avoid accessing the WebAppProvider when web apps are enabled in Lacros (and
  // thus disabled in Ash).
  if (web_app::IsWebAppsCrosapiEnabled()) {
    std::move(callback).Run(InstallOrLaunchWebAppResult::kUnknownError);
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  Profile* profile = Profile::FromBrowserContext(context);
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  DCHECK(provider);

  // Launch the app if web_app_url happens to match start_url. If not, the app
  // could still be installed with different start_url.
  webapps::AppId app_id = web_app::GenerateAppIdFromManifestId(web_app_url);
  if (provider->registrar_unsafe().IsInstallState(
          app_id, {web_app::proto::INSTALLED_WITHOUT_OS_INTEGRATION,
                   web_app::proto::INSTALLED_WITH_OS_INTEGRATION})) {
    LaunchWebApp(
        web_app::GenerateAppId(/*manifest_id=*/std::nullopt, web_app_url),
        profile);
    std::move(callback).Run(InstallOrLaunchWebAppResult::kSuccess);
    return;
  }

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(profile));
  web_app::CreateWebAppInstallTabHelpers(web_contents.get());

  base::WeakPtr<content::WebContents> web_contents_ptr =
      web_contents->GetWeakPtr();
  provider->scheduler().FetchInstallabilityForChromeManagement(
      web_app_url, web_contents_ptr,
      base::BindOnce(&OnWebAppInstallabilityChecked, profile->GetWeakPtr(),
                     std::move(callback), std::move(web_contents)));
}

void ChromeManagementAPIDelegate::EnableExtension(
    content::BrowserContext* context,
    const extensions::ExtensionId& extension_id) const {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(context)->GetExtensionById(
          extension_id, extensions::ExtensionRegistry::EVERYTHING);
  // The extension must exist as this method is invoked on enabling an extension
  // from the extensions management page (see `ManagementSetEnabledFunction`).
  CHECK(extension);

  extensions::SupervisedUserExtensionsDelegate* extensions_delegate =
      GetSupervisedUserExtensionsDelegateFromContext(context);
  extensions_delegate->MaybeRecordPermissionsIncreaseMetrics(*extension);
  extensions_delegate->RecordExtensionEnablementUmaMetrics(/*enabled=*/true);

  // If the extension was disabled for a permissions increase, the Management
  // API will have displayed a re-enable prompt to the user, so we know it's
  // safe to grant permissions here.
  extensions::ExtensionSystem::Get(context)
      ->extension_service()
      ->GrantPermissionsAndEnableExtension(extension);
}

void ChromeManagementAPIDelegate::DisableExtension(
    content::BrowserContext* context,
    const extensions::Extension* source_extension,
    const extensions::ExtensionId& extension_id,
    extensions::disable_reason::DisableReason disable_reason) const {
  extensions::SupervisedUserExtensionsDelegate* extensions_delegate =
      GetSupervisedUserExtensionsDelegateFromContext(context);
  extensions_delegate->RecordExtensionEnablementUmaMetrics(/*enabled=*/false);
  extensions::ExtensionSystem::Get(context)
      ->extension_service()
      ->DisableExtensionWithSource(source_extension, extension_id,
                                   disable_reason);
}

bool ChromeManagementAPIDelegate::UninstallExtension(
    content::BrowserContext* context,
    const extensions::ExtensionId& transient_extension_id,
    extensions::UninstallReason reason,
    std::u16string* error) const {
  return extensions::ExtensionSystem::Get(context)
      ->extension_service()
      ->UninstallExtension(transient_extension_id, reason, error);
}

void ChromeManagementAPIDelegate::SetLaunchType(
    content::BrowserContext* context,
    const extensions::ExtensionId& extension_id,
    extensions::LaunchType launch_type) const {
  extensions::SetLaunchType(context, extension_id, launch_type);
}

GURL ChromeManagementAPIDelegate::GetIconURL(
    const extensions::Extension* extension,
    int icon_size,
    ExtensionIconSet::Match match,
    bool grayscale) const {
  return extensions::ExtensionIconSource::GetIconURL(extension, icon_size,
                                                     match, grayscale);
}

GURL ChromeManagementAPIDelegate::GetEffectiveUpdateURL(
    const extensions::Extension& extension,
    content::BrowserContext* context) const {
  extensions::ExtensionManagement* extension_management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(context);
  return extension_management->GetEffectiveUpdateURL(extension);
}

void ChromeManagementAPIDelegate::ShowMv2DeprecationReEnableDialog(
    content::BrowserContext* context,
    content::WebContents* web_contents,
    const extensions::Extension& extension,
    base::OnceCallback<void(bool)> done_callback) const {
  // Extension should only be disabled due to MV2 deprecation in the "disable"
  // experiment stage.
  auto* mv2_experiment_manager =
      extensions::ManifestV2ExperimentManager::Get(context);
  CHECK_EQ(mv2_experiment_manager->GetCurrentExperimentStage(),
           extensions::MV2ExperimentStage::kDisableWithReEnable);

  // Tests can auto confirm the re-enable dialog.
  auto confirm_value =
      extensions::ScopedTestDialogAutoConfirm::GetAutoConfirmValue();
  switch (confirm_value) {
    case extensions::ScopedTestDialogAutoConfirm::NONE:
      // Continue, auto confirm has not been set.
      break;
    case extensions::ScopedTestDialogAutoConfirm::CANCEL:
      CHECK_IS_TEST();
      std::move(done_callback).Run(/*enable_allowed=*/false);
      return;
    case extensions::ScopedTestDialogAutoConfirm::ACCEPT:
    case extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION:
      CHECK_IS_TEST();
      std::move(done_callback).Run(/*enable_allowed=*/true);
      return;
  }

  gfx::NativeWindow parent =
      web_contents ? web_contents->GetTopLevelNativeWindow() : nullptr;
  extensions::ShowMv2DeprecationReEnableDialog(
      parent, extension.id(), extension.name(), std::move(done_callback));
}

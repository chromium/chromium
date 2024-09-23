// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/pwa_handler.h"

#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/concurrent_callbacks.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/url_constants.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace {

namespace errors {

// Returns a common error when WebApp component is unavailable in the scenario,
// e.g. in incognito mode.
protocol::Response WebAppUnavailable() {
  return protocol::Response::ServerError(
      "Webapps are not available in current profile.");
}

// Returns a common error when the to-be-installed webapp has a different
// manifest id than the required.
protocol::Response InconsistentManifestId(const std::string& in_manifest_id,
                                          const std::string& url_or_appid) {
  return protocol::Response::InvalidRequest(
      base::StrCat({"Expected manifest id ", in_manifest_id,
                    " does not match input url or app id ", url_or_appid}));
}

}  // namespace errors

using FileHandlers =
    std::unique_ptr<protocol::Array<protocol::PWA::FileHandler>>;

base::expected<FileHandlers, protocol::Response> GetFileHandlersFromApp(
    const webapps::AppId app_id,
    const std::string in_manifest_id,
    web_app::AppLock& app_lock,
    base::Value::Dict& debug_value) {
  const web_app::WebApp* web_app = app_lock.registrar().GetAppById(app_id);
  if (web_app == nullptr) {
    return base::unexpected(protocol::Response::InvalidParams(
        base::StrCat({"Unknown web-app manifest id ", in_manifest_id})));
  }

  using protocol::PWA::FileHandler;
  using protocol::PWA::FileHandlerAccept;
  auto file_handlers = std::make_unique<protocol::Array<FileHandler>>();
  for (const auto& input_handler : web_app->current_os_integration_states()
                                       .file_handling()
                                       .file_handlers()) {
    auto accepts = std::make_unique<protocol::Array<FileHandlerAccept>>();
    for (const auto& input_accept : input_handler.accept()) {
      auto file_extensions = std::make_unique<protocol::Array<std::string>>();
      for (const auto& input_file_extension : input_accept.file_extensions()) {
        file_extensions->push_back(input_file_extension);
      }
      accepts->push_back(FileHandlerAccept::Create()
                             .SetMediaType(input_accept.mimetype())
                             .SetFileExtensions(std::move(file_extensions))
                             .Build());
    }
    file_handlers->push_back(FileHandler::Create()
                                 .SetAction(input_handler.action())
                                 .SetAccepts(std::move(accepts))
                                 .SetDisplayName(input_handler.display_name())
                                 .Build());
  }
  return base::ok(std::move(file_handlers));
}

// A shared way to handle the callback of WebAppUiManager::LaunchApp.
base::expected<std::string, protocol::Response> GetTargetIdFromLaunch(
    const std::string& in_manifest_id,
    const std::optional<GURL>& url,
    base::WeakPtr<Browser> browser,
    base::WeakPtr<content::WebContents> web_contents,
    apps::LaunchContainer container) {
  // The callback will always be provided with a valid Browser
  // instance, but the web_contents is associated with the newly
  // opened web app, and it can be used to indicate the success of the
  // launch operation.
  // See web_app::WebAppLaunchUtils::LaunchWebApp() for more
  // information.
  if (web_contents) {
    return content::DevToolsAgentHost::GetOrCreateForTab(web_contents.get())
        ->GetId();
  }
  std::string msg = "Failed to launch " + in_manifest_id;
  if (url) {
    msg += " from url " + url->spec();
  }
  return base::unexpected(protocol::Response::InvalidRequest(msg));
}

}  // namespace

PWAHandler::PWAHandler(protocol::UberDispatcher* dispatcher,
                       const std::string& target_id)
    : target_id_(target_id) {
  protocol::PWA::Dispatcher::wire(dispatcher, this);
}

PWAHandler::~PWAHandler() = default;

// TODO(crbug.com/331214986): Consider if the API should allow setting a browser
// context id as the profile id to override the default behavior.
Profile* PWAHandler::GetProfile() const {
  auto host = content::DevToolsAgentHost::GetForId(target_id_);
  if (host && host->GetBrowserContext()) {
    return Profile::FromBrowserContext(host->GetBrowserContext());
  }
  return ProfileManager::GetLastUsedProfile();
}

web_app::WebAppCommandScheduler* PWAHandler::GetScheduler() const {
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(GetProfile());
  if (provider) {
    return &provider->scheduler();
  }
  return nullptr;
}

content::WebContents* PWAHandler::GetWebContents() const {
  auto host = content::DevToolsAgentHost::GetForId(target_id_);
  if (!host) {
    return nullptr;
  }
  return host->GetWebContents();
}

void PWAHandler::GetOsAppState(
    const std::string& in_manifest_id,
    std::unique_ptr<GetOsAppStateCallback> callback) {
  Profile* profile = GetProfile();
  const webapps::AppId app_id =
      web_app::GenerateAppIdFromManifestId(GURL{in_manifest_id});
  int badge_count = 0;
  {
    badging::BadgeManager* badge_manager =
        badging::BadgeManagerFactory::GetForProfile(profile);
    CHECK(badge_manager);
    std::optional<badging::BadgeManager::BadgeValue> badge =
        badge_manager->GetBadgeValue(app_id);
    if (badge && *badge) {
      badge_count = base::ClampedNumeric<int32_t>(**badge);
    }
  }
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile);
  if (!provider) {
    std::move(callback)->sendFailure(errors::WebAppUnavailable());
    return;
  }
  provider->scheduler().ScheduleCallbackWithResult(
      "PWAHandler::GetOsAppState", web_app::AppLockDescription(app_id),
      base::BindOnce(&GetFileHandlersFromApp, app_id, in_manifest_id),
      base::BindOnce(
          [](int badge_count, std::unique_ptr<GetOsAppStateCallback> callback,
             base::expected<FileHandlers, protocol::Response>&& file_handlers) {
            if (file_handlers.has_value()) {
              std::move(callback)->sendSuccess(
                  badge_count, std::move(file_handlers).value());
            } else {
              std::move(callback)->sendFailure(file_handlers.error());
            }
          },
          badge_count, std::move(callback)),
      base::expected<FileHandlers, protocol::Response>(
          base::unexpected(protocol::Response::ServerError(
              base::StrCat({"web-app is shutting down when querying manifest ",
                            in_manifest_id})))));
}

// Install from the manifest_id only. Require a WebContents.
void PWAHandler::InstallFromManifestId(
    const std::string& in_manifest_id,
    std::unique_ptr<InstallCallback> callback) {
  content::WebContents* contents = GetWebContents();
  if (contents == nullptr) {
    std::move(callback)->sendFailure(protocol::Response::InvalidRequest(
        base::StrCat({"The devtools session has no associated web page when "
                      "installing ",
                      in_manifest_id})));
    return;
  }
  auto* scheduler = GetScheduler();
  if (!scheduler) {
    std::move(callback)->sendFailure(errors::WebAppUnavailable());
    return;
  }
  scheduler->FetchManifestAndInstall(
      webapps::WebappInstallSource::DEVTOOLS, contents->GetWeakPtr(),
      base::BindOnce(
          [](const std::string& in_manifest_id,
             content::WebContents* initiator_web_contents,
             std::unique_ptr<web_app::WebAppInstallInfo> web_app_info,
             web_app::WebAppInstallationAcceptanceCallback
                 acceptance_callback) {
            // Returning false here ended up causing the kUserInstallDeclined,
            // so the error message needs to be updated accordingly.
            // TODO(crbug.com/331214986): Modify the DialogCallback to allow
            // clients providing their own error code rather than always
            // returning kUserInstallDeclined. And maybe change it to a more
            // neutral name other than "Dialog" to avoid implying the use of UI.
            const bool manifest_match =
                (web_app_info->manifest_id().spec() == in_manifest_id);
            std::move(acceptance_callback)
                .Run(manifest_match, std::move(web_app_info));
          },
          in_manifest_id),
      base::BindOnce(
          [](const std::string& in_manifest_id,
             std::unique_ptr<InstallCallback> callback,
             const webapps::AppId& app_id, webapps::InstallResultCode code) {
            if (webapps::IsSuccess(code)) {
              std::move(callback)->sendSuccess();
            } else {
              // See the comment above.
              if (code == webapps::InstallResultCode::kUserInstallDeclined) {
                return std::move(callback)->sendFailure(
                    errors::InconsistentManifestId(in_manifest_id, app_id));
              }
              std::move(callback)->sendFailure(
                  protocol::Response::InvalidRequest(
                      base::StrCat({"Failed to install ", in_manifest_id, ": ",
                                    base::ToString(code)})));
            }
          },
          in_manifest_id, std::move(callback)),
      web_app::FallbackBehavior::kCraftedManifestOnly);
}

void PWAHandler::InstallFromUrl(const std::string& in_manifest_id,
                                const std::string& in_install_url_or_bundle_url,
                                std::unique_ptr<InstallCallback> callback) {
  GURL url{in_install_url_or_bundle_url};
  // Technically unnecessary, but let's check it anyway to avoid leaking
  // unexpected schemes.
  if (!url.is_valid()) {
    std::move(callback)->sendFailure(
        protocol::Response::InvalidParams(base::StrCat(
            {"Invalid installUrlOrBundleUrl ", in_install_url_or_bundle_url})));
    return;
  }
  // TODO(crbug.com/337872319): Support installing isolated apps on chrome-os.
  if (!url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback)->sendFailure(
        protocol::Response::MethodNotFound(base::StrCat(
            {"Installing webapp from url ", in_install_url_or_bundle_url,
             " with scheme [", url.scheme(), "] is not supported yet."})));
    return;
  }
  auto* scheduler = GetScheduler();
  if (!scheduler) {
    std::move(callback)->sendFailure(errors::WebAppUnavailable());
    return;
  }
  scheduler->FetchInstallInfoFromInstallUrl(
      GURL{in_manifest_id}, url,
      base::BindOnce(&PWAHandler::InstallFromInstallInfo,
                     weak_ptr_factory_.GetWeakPtr(), in_manifest_id,
                     in_install_url_or_bundle_url, std::move(callback)));
}

void PWAHandler::InstallFromInstallInfo(
    const std::string& in_manifest_id,
    const std::string& in_install_url_or_bundle_url,
    std::unique_ptr<InstallCallback> callback,
    std::unique_ptr<web_app::WebAppInstallInfo> web_app_info) {
  if (!web_app_info) {
    std::move(callback)->sendFailure(protocol::Response::InvalidRequest(
        base::StrCat({"Couldn't fetch install info for ", in_manifest_id,
                      " from ", in_install_url_or_bundle_url})));
    return;
  }
  if (web_app_info->manifest_id().spec() != in_manifest_id) {
    std::move(callback)->sendFailure(errors::InconsistentManifestId(
        in_manifest_id, in_install_url_or_bundle_url));
    return;
  }
  auto* scheduler = GetScheduler();
  if (!scheduler) {
    std::move(callback)->sendFailure(errors::WebAppUnavailable());
    return;
  }
  scheduler->InstallFromInfoWithParams(
      std::move(web_app_info), false, webapps::WebappInstallSource::DEVTOOLS,
      base::BindOnce(
          [](const std::string& in_manifest_id,
             const std::string& in_install_url_or_bundle_url,
             std::unique_ptr<InstallCallback> callback,
             const webapps::AppId& app_id, webapps::InstallResultCode code) {
            if (webapps::IsSuccess(code)) {
              std::move(callback)->sendSuccess();
            } else {
              std::move(callback)->sendFailure(
                  protocol::Response::InvalidRequest(
                      base::StrCat({"Failed to install ", in_manifest_id,
                                    " from ", in_install_url_or_bundle_url,
                                    ": ", base::ToString(code)})));
            }
          },
          in_manifest_id, in_install_url_or_bundle_url, std::move(callback)),
      // TODO(crbug.com/331214986): Create command-line flag to fake all os
      // integration for Chrome.
      web_app::WebAppInstallParams{});
}

void PWAHandler::Install(
    const std::string& in_manifest_id,
    protocol::Maybe<std::string> in_install_url_or_bundle_url,
    std::unique_ptr<InstallCallback> callback) {
  if (in_install_url_or_bundle_url) {
    InstallFromUrl(in_manifest_id,
                   std::move(in_install_url_or_bundle_url).value(),
                   std::move(callback));
  } else {
    InstallFromManifestId(in_manifest_id, std::move(callback));
  }
}

void PWAHandler::Uninstall(const std::string& in_manifest_id,
                           std::unique_ptr<UninstallCallback> callback) {
  const webapps::AppId app_id =
      web_app::GenerateAppIdFromManifestId(GURL{in_manifest_id});
  auto* scheduler = GetScheduler();
  if (!scheduler) {
    std::move(callback)->sendFailure(errors::WebAppUnavailable());
    return;
  }
  scheduler->RemoveUserUninstallableManagements(
      app_id, webapps::WebappUninstallSource::kDevtools,
      base::BindOnce(
          [](const std::string& in_manifest_id,
             std::unique_ptr<UninstallCallback> callback,
             webapps::UninstallResultCode result) {
            if (webapps::UninstallSucceeded(result)) {
              std::move(callback)->sendSuccess();
            } else {
              std::move(callback)->sendFailure(
                  protocol::Response::InvalidRequest(
                      base::StrCat({"Failed to uninstall ", in_manifest_id,
                                    ": ", base::ToString(result)})));
            }
          },
          in_manifest_id, std::move(callback)));
}

void PWAHandler::Launch(const std::string& in_manifest_id,
                        protocol::Maybe<std::string> in_url,
                        std::unique_ptr<LaunchCallback> callback) {
  const webapps::AppId app_id =
      web_app::GenerateAppIdFromManifestId(GURL{in_manifest_id});
  const auto url =
      (in_url ? std::optional<GURL>{in_url.value()} : std::nullopt);
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(GetProfile());
  if (!provider) {
    std::move(callback)->sendFailure(errors::WebAppUnavailable());
    return;
  }
  if (url) {
    if (!url->is_valid()) {
      std::move(callback)->sendFailure(protocol::Response::InvalidParams(
          base::StrCat({"Invalid input url ", in_url.value()})));
      return;
    }

    // TODO(crbug.com/338406726): Remove after launches correctly fail when url
    // is out of scope.
    bool is_in_scope;
    if (base::FeatureList::IsEnabled(
            blink::features::kWebAppEnableScopeExtensions)) {
      is_in_scope =
          provider->registrar_unsafe().IsUrlInAppExtendedScope(*url, app_id);
    } else {
      is_in_scope = provider->registrar_unsafe().IsUrlInAppScope(*url, app_id);
    }
    if (!is_in_scope) {
      std::move(callback)->sendFailure(
          protocol::Response::InvalidParams(base::StrCat(
              {"Requested url ", url->spec(),
               " is not in the scope of the web app ", in_manifest_id})));
      return;
    }
  }
  provider->scheduler().LaunchApp(
      app_id, url,
      base::BindOnce(
          [](const std::string& in_manifest_id, const std::optional<GURL>& url,
             std::unique_ptr<LaunchCallback> callback,
             base::WeakPtr<Browser> browser,
             base::WeakPtr<content::WebContents> web_contents,
             apps::LaunchContainer container) {
            auto result = GetTargetIdFromLaunch(in_manifest_id, url, browser,
                                                web_contents, container);
            if (result.has_value()) {
              std::move(callback)->sendSuccess(std::move(result).value());
            } else {
              std::move(callback)->sendFailure(std::move(result).error());
            }
          },
          in_manifest_id, url, std::move(callback)));
}

void PWAHandler::LaunchFilesInApp(
    const std::string& in_manifest_id,
    std::unique_ptr<protocol::Array<std::string>> in_files,
    std::unique_ptr<LaunchFilesInAppCallback> callback) {
  const GURL manifest_id = GURL{in_manifest_id};
  const webapps::AppId app_id =
      web_app::GenerateAppIdFromManifestId(manifest_id);
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(GetProfile());
  if (!provider) {
    std::move(callback)->sendFailure(errors::WebAppUnavailable());
    return;
  }
  if (!in_files || in_files->empty()) {
    std::move(callback)->sendFailure(
        protocol::Response::InvalidParams(base::StrCat(
            {"PWA.launchFilesInApp needs input files to be opened by web app ",
             in_manifest_id})));
    return;
  }
  std::vector<base::FilePath> files{in_files->size()};
  for (const auto& file : *in_files) {
    // TODO(b/331214986): Support platform-dependent file path encoding.
    // TODO(b/331214986): May want to fail if the files do not exist.
    files.push_back(base::FilePath::FromUTF8Unsafe(file));
  }
  const web_app::WebAppFileHandlerManager::LaunchInfos launch_infos =
      provider->os_integration_manager()
          .file_handler_manager()
          .GetMatchingFileHandlerUrls(app_id, files);
  if (launch_infos.empty()) {
    std::move(callback)->sendFailure(protocol::Response::InvalidParams(
        base::StrCat({"Files are not supported by web app ", in_manifest_id})));
    return;
  }

  base::ConcurrentCallbacks<base::expected<std::string, protocol::Response>>
      concurrent;
  for (const auto& [url, paths] : launch_infos) {
    provider->scheduler().LaunchApp(
        app_id, *base::CommandLine::ForCurrentProcess(),
        /* current_directory */ base::FilePath{},
        /* url_handler_launch_url */ std::nullopt,
        /* protocol_handler_launch_url */ std::nullopt,
        /* file_launch_url */ url, paths,
        base::BindOnce(
            [](const std::string& in_manifest_id,
               base::OnceCallback<void(
                   base::expected<std::string, protocol::Response>)> callback,
               base::WeakPtr<Browser> browser,
               base::WeakPtr<content::WebContents> web_contents,
               apps::LaunchContainer container) {
              std::move(callback).Run(
                  GetTargetIdFromLaunch(in_manifest_id, std::optional<GURL>{},
                                        browser, web_contents, container));
            },
            in_manifest_id, concurrent.CreateCallback()));
  }
  std::move(concurrent)
      .Done(base::BindOnce(
          [](std::unique_ptr<LaunchFilesInAppCallback> callback,
             std::vector<base::expected<std::string, protocol::Response>>
                 results) {
            auto ids = std::make_unique<protocol::Array<std::string>>();
            std::string errors;
            for (auto& result : results) {
              if (result.has_value()) {
                ids->push_back(std::move(result).value());
              } else {
                errors += std::move(result).error().Message();
                errors += "; ";
              }
            }
            if (errors.empty()) {
              std::move(callback)->sendSuccess(std::move(ids));
            } else {
              std::move(callback)->sendFailure(
                  protocol::Response::InvalidRequest(std::move(errors)));
            }
          },
          std::move(callback)));
}

protocol::Response PWAHandler::OpenCurrentPageInApp(
    const std::string& in_manifest_id) {
  content::WebContents* contents = GetWebContents();
  if (contents == nullptr) {
    return protocol::Response::InvalidRequest(
        base::StrCat({"The devtools session has no associated web page when "
                      "opening ",
                      in_manifest_id, " in its app."}));
  }
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(GetProfile());
  if (!provider) {
    return errors::WebAppUnavailable();
  }

  const webapps::AppId app_id =
      web_app::GenerateAppIdFromManifestId(GURL{in_manifest_id});
  // Since this logic is only needed on MacOS, for the sake of simplicity the
  // unsafe access of the registrar is fine at the moment instead of wrapping it
  // in a command.
  const bool shortcut_created = [provider, &app_id]() {
    auto state =
        provider->registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
    if (!state.has_value()) {
      return false;
    }
    return state->has_shortcut();
  }();
  if (!provider->ui_manager().CanReparentAppTabToWindow(app_id,
                                                        shortcut_created)) {
    return protocol::Response::InvalidParams(
        base::StrCat({"The web app ", in_manifest_id,
                      " cannot be opened in its app. Check if the app is "
                      "correctly installed."}));
  }
  Browser* browser = provider->ui_manager().ReparentAppTabToWindow(
      contents, app_id, shortcut_created);
  if (browser == nullptr) {
    return protocol::Response::InvalidRequest(base::StrCat(
        {"The current page ", contents->GetLastCommittedURL().spec(),
         " cannot be opened in the web app ", in_manifest_id}));
  }
  return protocol::Response::Success();
}

void PWAHandler::ChangeAppUserSettings(
    const std::string& in_manifest_id,
    protocol::Maybe<bool> in_link_capturing,
    protocol::Maybe<protocol::PWA::DisplayMode> in_display_mode,
    std::unique_ptr<ChangeAppUserSettingsCallback> callback) {
  const webapps::AppId app_id =
      web_app::GenerateAppIdFromManifestId(GURL{in_manifest_id});

  // Always checks the availability of web app system to ensure the consistency
  // of the API behavior.
  auto* scheduler = GetScheduler();
  if (!scheduler) {
    std::move(callback)->sendFailure(errors::WebAppUnavailable());
    return;
  }

  std::optional<web_app::mojom::UserDisplayMode> user_display_mode{};
  if (in_display_mode) {
    if (in_display_mode.value() == protocol::PWA::DisplayModeEnum::Standalone) {
      user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
    } else if (in_display_mode.value() ==
               protocol::PWA::DisplayModeEnum::Browser) {
      user_display_mode = web_app::mojom::UserDisplayMode::kBrowser;
    } else {
      std::move(callback)->sendFailure(
          protocol::Response::InvalidParams(base::StrCat(
              {"Unrecognized displayMode ", in_display_mode.value(),
               " when changing user settings of web app ", in_manifest_id})));
      return;
    }
  }

#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/339453269): Implement changeUserAppSettings/LinkCapturing on
  // ChromeOS.
  // TL:DR; the ChromeOS uses apps::AppServiceProxyFactory instead, and the
  // SetSupportedLinksPreference would associate all the supported links to the
  // app.
  if (in_link_capturing) {
    std::move(callback)->sendFailure(protocol::Response::InvalidRequest(
        "Changing AppUserSettings/LinkCapturing on ChromeOS is not supported "
        "yet."));
    return;
  }
#endif

  base::ConcurrentCallbacks<std::optional<std::string>> concurrent;
  scheduler->ScheduleCallbackWithResult(
      // TODO(crbug.com/339453269): Find a way to forward the error of the set
      // operation back here.
      "PWAHandler::ChangeAppUserSettings", web_app::AppLockDescription(app_id),
      base::BindOnce(
          [](const webapps::AppId& app_id, web_app::AppLock& app_lock,
             base::Value::Dict& debug_value) -> std::optional<std::string> {
            // Only consider apps that are installed with or without OS
            // integration. Apps coming via sync should not be considered.
            if (app_lock.registrar().IsInstallState(
                    app_id, {web_app::proto::InstallState::
                                 INSTALLED_WITH_OS_INTEGRATION,
                             web_app::proto::InstallState::
                                 INSTALLED_WITHOUT_OS_INTEGRATION})) {
              return std::nullopt;
            }
            return "WebApp is not installed";
          },
          app_id),
      concurrent.CreateCallback(),
      /* result_on_shutdown= */
      std::optional<std::string>{std::in_place,
                                 "WebApp system is shuting down."});
  if (in_link_capturing) {
    scheduler->SetAppCapturesSupportedLinksDisableOverlapping(
        app_id, in_link_capturing.value(),
        base::BindOnce(concurrent.CreateCallback(),
                       std::optional<std::string>{}));
  }
  if (user_display_mode) {
    // TODO(crbug.com/331214986): Create command-line flag to fake all os
    // integration for Chrome.
    scheduler->SetUserDisplayMode(app_id, user_display_mode.value(),
                                  base::BindOnce(concurrent.CreateCallback(),
                                                 std::optional<std::string>{}));
  }

  std::move(concurrent)
      .Done(base::BindOnce(
          [](const std::string& in_manifest_id,
             std::unique_ptr<ChangeAppUserSettingsCallback> callback,
             std::vector<std::optional<std::string>> results) {
            std::string errors;
            for (const auto& result : results) {
              if (result) {
                errors.append(result.value()).append(";");
              }
            }
            if (errors.empty()) {
              std::move(callback)->sendSuccess();
            } else {
              std::move(callback)->sendFailure(
                  protocol::Response::InvalidRequest(base::StrCat(
                      {"Failed to change the user settings of web app ",
                       in_manifest_id, ". ", errors})));
            }
          },
          in_manifest_id, std::move(callback)));
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/pwa_handler.h"

#include <expected>
#include <sstream>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/to_string.h"
#include "base/types/expected.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
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
      std::string("Expected manifest id " + in_manifest_id +
                  " does not match input url or app id " + url_or_appid));
}

}  // namespace errors

using GetFileHandlersFromAppReturnType =
    base::expected<std::unique_ptr<protocol::Array<protocol::PWA::FileHandler>>,
                   protocol::Response>;

GetFileHandlersFromAppReturnType GetFileHandlersFromApp(
    const webapps::AppId app_id,
    const std::string in_manifest_id,
    web_app::AppLock& app_lock,
    base::Value::Dict& debug_value) {
  const web_app::WebApp* web_app = app_lock.registrar().GetAppById(app_id);
  if (web_app == nullptr) {
    return base::unexpected(protocol::Response::InvalidParams(
        std::string("Unknown web-app manifest id ") + in_manifest_id));
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

}  // namespace

PWAHandler::PWAHandler(protocol::UberDispatcher* dispatcher,
                       const std::string& target_id)
    : target_id_(target_id) {
  protocol::PWA::Dispatcher::wire(dispatcher, this);
}

PWAHandler::~PWAHandler() = default;

// TODO(b/331214986): Consider if the API should allow setting a browser
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
             GetFileHandlersFromAppReturnType&& file_handlers) {
            if (file_handlers.has_value()) {
              std::move(callback)->sendSuccess(
                  badge_count, std::move(file_handlers).value());
            } else {
              std::move(callback)->sendFailure(file_handlers.error());
            }
          },
          badge_count, std::move(callback)),
      GetFileHandlersFromAppReturnType(
          base::unexpected(protocol::Response::ServerError(
              std::string("web-app is shutting down when querying manifest ") +
              in_manifest_id))));
}

// Install from the manifest_id only. Require a WebContents.
void PWAHandler::InstallFromManifestId(
    const std::string& in_manifest_id,
    std::unique_ptr<InstallCallback> callback) {
  content::WebContents* contents = GetWebContents();
  if (contents == nullptr) {
    std::move(callback)->sendFailure(protocol::Response::InvalidRequest(
        std::string("The devtools session has no associated web page when "
                    "installing ") +
        in_manifest_id));
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
                (web_app_info->manifest_id.spec() == in_manifest_id);
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
                      std::string("Failed to install ") + in_manifest_id +
                      ": " + base::ToString(code)));
            }
          },
          in_manifest_id, std::move(callback)),
      web_app::FallbackBehavior::kCraftedManifestOnly);
}

void PWAHandler::InstallFromUrl(const std::string& in_manifest_id,
                                std::string in_install_url_or_bundle_url,
                                std::unique_ptr<InstallCallback> callback) {
  GURL url{in_install_url_or_bundle_url};
  // Technically unnecessary, but let's check it anyway to avoid leaking
  // unexpected schemes.
  if (!url.is_valid()) {
    std::move(callback)->sendFailure(protocol::Response::InvalidParams(
        std::string("Invalid installUrlOrBundleUrl ") +
        in_install_url_or_bundle_url));
    return;
  }
  // TODO(crbug.com/337872319): Support installing isolated apps on chrome-os.
  if (!url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback)->sendFailure(protocol::Response::MethodNotFound(
        std::string("Installing webapp from url " +
                    in_install_url_or_bundle_url + " with scheme [") +
        url.scheme() + "] is not supported yet."));
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
    std::string in_manifest_id,
    std::string in_install_url_or_bundle_url,
    std::unique_ptr<InstallCallback> callback,
    std::unique_ptr<web_app::WebAppInstallInfo> web_app_info) {
  if (!web_app_info) {
    std::move(callback)->sendFailure(protocol::Response::InvalidRequest(
        std::string("Couldn't fetch install info for ") + in_manifest_id +
        " from " + in_install_url_or_bundle_url));
    return;
  }
  if (web_app_info->manifest_id.spec() != in_manifest_id) {
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
                      std::string("Failed to install ") + in_manifest_id +
                      " from " + in_install_url_or_bundle_url + ": " +
                      base::ToString(code)));
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
                      std::string("Failed to uninstall ") + in_manifest_id +
                      ": " + base::ToString(result)));
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
          std::string("Invalid input url " + in_url.value())));
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
      std::move(callback)->sendFailure(protocol::Response::InvalidParams(
          std::string("Requested url ") + url->spec() +
          " is not in the scope of the web app " + in_manifest_id));
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
            // The callback will always be provided with a valid Browser
            // instance, but the web_contents is associated with the newly
            // opened web app, and it can be used to indicate the success of the
            // launch operation.
            // See web_app::WebAppLaunchUtils::LaunchWebApp() for more
            // information.
            if (web_contents) {
              std::move(callback)->sendSuccess(
                  content::DevToolsAgentHost::GetOrCreateForTab(
                      web_contents.get())
                      ->GetId());
              return;
            }
            std::string msg = "Failed to launch " + in_manifest_id;
            if (url) {
              msg += " from url " + url->spec();
            }
            std::move(callback)->sendFailure(
                protocol::Response::InvalidRequest(msg));
          },
          in_manifest_id, url, std::move(callback)));
}

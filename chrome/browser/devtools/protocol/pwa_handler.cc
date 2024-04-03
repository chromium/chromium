// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/pwa_handler.h"

#include <expected>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/numerics/clamped_math.h"
#include "base/types/expected.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/devtools_agent_host.h"
#include "url/gurl.h"

namespace {

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

Profile* PWAHandler::GetProfile() const {
  auto host = content::DevToolsAgentHost::GetForId(target_id_);
  if (host && host->GetBrowserContext()) {
    return Profile::FromBrowserContext(host->GetBrowserContext());
  }
  return ProfileManager::GetLastUsedProfile();
}

void PWAHandler::GetOsAppState(
    const std::string& in_manifest_id,
    std::unique_ptr<GetOsAppStateCallback> callback) {
  // TODO(b/331214986): Consider if the API should allow setting a browser
  // context id as the profile id to override the default behavior.
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
  CHECK(provider);
  provider->scheduler().ScheduleCallbackWithResult(
      "PWAHandler::GetOsAppState", web_app::AppLockDescription(app_id),
      base::BindOnce(&GetFileHandlersFromApp, app_id, in_manifest_id),
      base::BindOnce(
          [](std::unique_ptr<GetOsAppStateCallback> callback, int badge_count,
             GetFileHandlersFromAppReturnType&& file_handlers) {
            if (file_handlers.has_value()) {
              std::move(callback)->sendSuccess(
                  badge_count, std::move(file_handlers).value());
            } else {
              std::move(callback)->sendFailure(file_handlers.error());
            }
          },
          std::move(callback), badge_count),
      GetFileHandlersFromAppReturnType(
          base::unexpected(protocol::Response::ServerError(
              std::string("web-app is shutting down when querying manifest ") +
              in_manifest_id))));
}

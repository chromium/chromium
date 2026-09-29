// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/devtools/protocol/page_handler.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/payments/content/payment_request_web_contents_manager.h"
#include "components/subresource_filter/content/browser/devtools_interaction_tracker.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/gfx/image/image.h"

PageHandler::PageHandler(scoped_refptr<content::DevToolsAgentHost> agent_host,
                         content::WebContents* web_contents,
                         protocol::UberDispatcher* dispatcher,
                         bool is_trusted)
    : agent_host_(agent_host),
      web_contents_(web_contents->GetWeakPtr()),
      is_trusted_(is_trusted) {
  protocol::Page::Dispatcher::wire(dispatcher, this);
}

PageHandler::~PageHandler() {
  Disable();
}

void PageHandler::ToggleAdBlocking(bool enabled) {
  if (!web_contents_) {
    return;
  }

  // Create the DevtoolsInteractionTracker lazily (note that this call is a
  // no-op if the object was already created).
  subresource_filter::DevtoolsInteractionTracker::CreateForWebContents(
      web_contents_.get());

  subresource_filter::DevtoolsInteractionTracker::FromWebContents(
      web_contents_.get())
      ->ToggleForceActivation(enabled);
}

protocol::Response PageHandler::Enable(
    std::optional<bool> enable_file_chooser_opened_event) {
  enabled_ = true;
  // Do not mark the command as handled. Let it fall through instead, so that
  // the handler in content gets a chance to process the command.
  return protocol::Response::FallThrough();
}

protocol::Response PageHandler::Disable() {
  enabled_ = false;
  ToggleAdBlocking(false /* enable */);
  SetSPCTransactionMode(protocol::Page::SetSPCTransactionMode::ModeEnum::None);
  // Do not mark the command as handled. Let it fall through instead, so that
  // the handler in content gets a chance to process the command.
  return protocol::Response::FallThrough();
}

protocol::Response PageHandler::SetAdBlockingEnabled(bool enabled) {
  if (!enabled_) {
    return protocol::Response::ServerError("Page domain is disabled.");
  }
  ToggleAdBlocking(enabled);
  return protocol::Response::Success();
}

protocol::Response PageHandler::SetSPCTransactionMode(
    const protocol::String& mode) {
  if (!is_trusted_) {
    return protocol::Response::ServerError(
        "Permission denied: Page.setSPCTransactionMode requires a trusted "
        "client");
  }
  if (!web_contents_) {
    return protocol::Response::ServerError("No web contents to host a dialog.");
  }

  payments::SPCTransactionMode spc_mode = payments::SPCTransactionMode::kNone;
  if (mode == protocol::Page::SetSPCTransactionMode::ModeEnum::AutoAccept) {
    spc_mode = payments::SPCTransactionMode::kAutoAccept;
  } else if (mode == protocol::Page::SetSPCTransactionMode::ModeEnum::
                         AutoChooseToAuthAnotherWay) {
    spc_mode = payments::SPCTransactionMode::kAutoAuthAnotherWay;
  } else if (mode ==
             protocol::Page::SetSPCTransactionMode::ModeEnum::AutoReject) {
    spc_mode = payments::SPCTransactionMode::kAutoReject;
  } else if (mode ==
             protocol::Page::SetSPCTransactionMode::ModeEnum::AutoOptOut) {
    spc_mode = payments::SPCTransactionMode::kAutoOptOut;
  } else if (mode != protocol::Page::SetSPCTransactionMode::ModeEnum::None) {
    return protocol::Response::ServerError("Unrecognized mode value");
  }

  auto* payment_request_manager =
      payments::PaymentRequestWebContentsManager::GetOrCreateForWebContents(
          web_contents_.get());
  payment_request_manager->SetSPCTransactionMode(spc_mode);
  return protocol::Response::Success();
}

protocol::Response PageHandler::SetRPHRegistrationMode(
    const protocol::String& mode) {
  if (!web_contents_) {
    return protocol::Response::ServerError("No web contents to host a dialog.");
  }
  if (!is_trusted_) {
    return protocol::Response::ServerError(
        "Permission denied: Page.setRPHRegistrationMode requires a trusted "
        "client");
  }

  custom_handlers::RphRegistrationMode rph_mode =
      custom_handlers::RphRegistrationMode::kNone;
  if (mode == protocol::Page::SetRPHRegistrationMode::ModeEnum::AutoAccept) {
    rph_mode = custom_handlers::RphRegistrationMode::kAutoAccept;
  } else if (mode ==
             protocol::Page::SetRPHRegistrationMode::ModeEnum::AutoReject) {
    rph_mode = custom_handlers::RphRegistrationMode::kAutoReject;
  } else if (mode != protocol::Page::SetRPHRegistrationMode::ModeEnum::None) {
    return protocol::Response::ServerError("Unrecognized mode value");
  }

  custom_handlers::ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          web_contents_->GetBrowserContext());
  registry->SetRphRegistrationMode(rph_mode);
  return protocol::Response::Success();
}

void PageHandler::GetInstallabilityErrors(
    std::unique_ptr<GetInstallabilityErrorsCallback> callback) {
  auto errors = std::make_unique<protocol::Array<std::string>>();
  webapps::InstallableManager* manager =
      web_contents_
          ? webapps::InstallableManager::FromWebContents(web_contents_.get())
          : nullptr;
  if (!manager) {
    callback->sendFailure(
        protocol::Response::ServerError("Unable to fetch errors for target"));
    return;
  }
  manager->GetAllErrors(base::BindOnce(&PageHandler::GotInstallabilityErrors,
                                       std::move(callback)));
}

// static
void PageHandler::GotInstallabilityErrors(
    std::unique_ptr<GetInstallabilityErrorsCallback> callback,
    std::vector<content::InstallabilityError> installability_errors) {
  auto result_installability_errors =
      std::make_unique<protocol::Array<protocol::Page::InstallabilityError>>();
  for (const auto& installability_error : installability_errors) {
    auto installability_error_arguments = std::make_unique<
        protocol::Array<protocol::Page::InstallabilityErrorArgument>>();
    for (const auto& error_argument :
         installability_error.installability_error_arguments) {
      installability_error_arguments->emplace_back(
          protocol::Page::InstallabilityErrorArgument::Create()
              .SetName(error_argument.name)
              .SetValue(error_argument.value)
              .Build());
    }
    result_installability_errors->emplace_back(
        protocol::Page::InstallabilityError::Create()
            .SetErrorId(installability_error.error_id)
            .SetErrorArguments(std::move(installability_error_arguments))
            .Build());
  }
  callback->sendSuccess(std::move(result_installability_errors));
}

void PageHandler::GetManifestIcons(
    std::unique_ptr<GetManifestIconsCallback> callback) {
  webapps::InstallableManager* manager =
      web_contents_
          ? webapps::InstallableManager::FromWebContents(web_contents_.get())
          : nullptr;

  if (!manager) {
    callback->sendFailure(
        protocol::Response::ServerError("Unable to fetch icons for target"));
    return;
  }

  manager->GetPrimaryIcon(
      base::BindOnce(&PageHandler::GotManifestIcons, std::move(callback)));
}

void PageHandler::GotManifestIcons(
    std::unique_ptr<GetManifestIconsCallback> callback,
    const SkBitmap* primary_icon) {
  std::optional<protocol::Binary> primaryIconAsBinary;

  if (primary_icon && !primary_icon->empty()) {
    primaryIconAsBinary = protocol::Binary::fromRefCounted(
        gfx::Image::CreateFrom1xBitmap(*primary_icon).As1xPNGBytes());
  }

  callback->sendSuccess(std::move(primaryIconAsBinary));
}

void PageHandler::GetAppId(std::unique_ptr<GetAppIdCallback> callback) {
  webapps::InstallableManager* manager =
      web_contents_
          ? webapps::InstallableManager::FromWebContents(web_contents_.get())
          : nullptr;

  if (!manager) {
    callback->sendFailure(
        protocol::Response::ServerError("Unable to fetch app id for target"));
    return;
  }

  webapps::InstallableParams params;
  manager->GetData(params, base::BindOnce(&PageHandler::OnDidGetManifest,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          std::move(callback)));
}

void PageHandler::OnDidGetManifest(std::unique_ptr<GetAppIdCallback> callback,
                                   const webapps::InstallableData& data) {
  std::optional<std::string> bundle_id;
  std::optional<std::string> parent_app_name;

  if (web_contents_) {
    const GURL& current_url = web_contents_->GetLastCommittedURL();
    if (current_url.SchemeIs(webapps::kIsolatedAppScheme)) {
      if (auto url_info = web_app::IsolatedWebAppUrlInfo::Create(current_url);
          url_info.has_value()) {
        bundle_id = url_info->web_bundle_id().id();
      }
    }

    auto* provider =
        web_app::WebAppProvider::GetForWebContents(web_contents_.get());
    if (provider) {
      const web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();
      std::optional<webapps::AppId> app_id =
          registrar.FindBestAppWithUrlInScope(
              current_url, web_app::WebAppFilter::IsIsolatedApp() |
                               web_app::WebAppFilter::IsIsolatedSubApp());
      if (app_id.has_value()) {
        parent_app_name = registrar.GetParentAppShortName(*app_id);
      }
    }
  }

  if (data.manifest_url->is_empty()) {
    callback->sendSuccess(std::nullopt, std::nullopt, bundle_id,
                          parent_app_name);
    return;
  }
  // Either both the id and start_url are present, or they are both empty.
  std::string current_app_id_str;
  std::string recommended_manifest_id_path_only;
  if (data.manifest->id.is_valid()) {
    CHECK(data.manifest->start_url.is_valid());
    current_app_id_str = data.manifest->id.spec();
    recommended_manifest_id_path_only =
        web_app::GenerateManifestIdFromStartUrlOnly(data.manifest->start_url)
            .value()
            .PathForRequest();
  } else {
    CHECK(!data.manifest->start_url.is_valid());
  }

  callback->sendSuccess(current_app_id_str, recommended_manifest_id_path_only,
                        bundle_id, parent_app_name);
}

void PageHandler::GetSubApps(std::unique_ptr<GetSubAppsCallback> callback) {
  auto sub_apps = std::make_unique<protocol::Array<protocol::Page::SubApp>>();
  if (!web_contents_) {
    callback->sendSuccess(std::move(sub_apps));
    return;
  }

  auto* provider =
      web_app::WebAppProvider::GetForWebContents(web_contents_.get());
  if (!provider) {
    callback->sendSuccess(std::move(sub_apps));
    return;
  }

  const web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();
  std::optional<webapps::AppId> app_id = registrar.FindBestAppWithUrlInScope(
      web_contents_->GetLastCommittedURL(),
      web_app::WebAppFilter::IsIsolatedApp() |
          web_app::WebAppFilter::IsIsolatedSubApp());
  if (!app_id.has_value()) {
    callback->sendSuccess(std::move(sub_apps));
    return;
  }

  std::vector<webapps::AppId> sub_app_ids = registrar.GetAllSubAppIds(*app_id);
  for (const auto& sub_id : sub_app_ids) {
    const web_app::WebApp* sub_app = registrar.GetAppById(sub_id);
    CHECK(sub_app);
    sub_apps->emplace_back(protocol::Page::SubApp::Create()
                               .SetName(registrar.GetAppShortName(sub_id))
                               .SetScope(sub_app->scope().spec())
                               .SetManifestId(sub_app->manifest_id().spec())
                               .SetStartUrl(sub_app->start_url().spec())
                               .Build());
  }

  callback->sendSuccess(std::move(sub_apps));
}

void PageHandler::GetSiblingSubApps(
    std::unique_ptr<GetSiblingSubAppsCallback> callback) {
  auto sibling_apps =
      std::make_unique<protocol::Array<protocol::Page::SubApp>>();
  if (!web_contents_) {
    callback->sendSuccess(std::move(sibling_apps));
    return;
  }

  auto* provider =
      web_app::WebAppProvider::GetForWebContents(web_contents_.get());
  if (!provider) {
    callback->sendSuccess(std::move(sibling_apps));
    return;
  }

  const web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();
  std::optional<webapps::AppId> app_id = registrar.FindBestAppWithUrlInScope(
      web_contents_->GetLastCommittedURL(),
      web_app::WebAppFilter::IsIsolatedApp() |
          web_app::WebAppFilter::IsIsolatedSubApp());
  if (!app_id.has_value()) {
    callback->sendSuccess(std::move(sibling_apps));
    return;
  }

  std::optional<webapps::AppId> parent_id = registrar.GetParentAppId(*app_id);
  if (!parent_id.has_value()) {
    callback->sendSuccess(std::move(sibling_apps));
    return;
  }

  std::vector<webapps::AppId> sibling_ids =
      registrar.GetAllSubAppIds(*parent_id);
  for (const auto& sibling_id : sibling_ids) {
    if (sibling_id == *app_id) {
      continue;
    }
    const web_app::WebApp* sibling_app = registrar.GetAppById(sibling_id);
    CHECK(sibling_app);
    sibling_apps->emplace_back(
        protocol::Page::SubApp::Create()
            .SetName(registrar.GetAppShortName(sibling_id))
            .SetScope(sibling_app->scope().spec())
            .SetManifestId(sibling_app->manifest_id().spec())
            .SetStartUrl(sibling_app->start_url().spec())
            .Build());
  }

  callback->sendSuccess(std::move(sibling_apps));
}

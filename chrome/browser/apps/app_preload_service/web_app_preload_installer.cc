// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/web_app_preload_installer.h"

#include <memory>

#include "base/barrier_callback.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/web_app_service_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/install_preloaded_verified_app_command.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/crosapi/mojom/web_app_types.mojom.h"
#include "components/webapps/browser/install_result_code.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

// Maximum size of the manifest file. 1MB.
constexpr int kMaxManifestSizeInBytes = 1024 * 1024;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("app_preload_service_web_installer",
                                        R"(
      semantics {
        sender: "App Preload Service"
        description:
          "Sends a request to a Google server to retrieve app installation"
          "information."
        trigger:
          "Requests are sent after the App Preload Service has performed an"
          "initial request to get a list of apps to install."
        data: "None"
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting: "This feature cannot be disabled by settings."
        policy_exception_justification:
          "This feature is required to deliver core user experiences and "
          "cannot be disabled by policy."
      }
    )");

constexpr char kCommandResultCodeHistogramName[] =
    "AppPreloadService.WebAppInstall.CommandResultCode";

int GetResponseCode(network::SimpleURLLoader* simple_loader) {
  if (simple_loader->ResponseInfo() && simple_loader->ResponseInfo()->headers) {
    return simple_loader->ResponseInfo()->headers->response_code();
  } else {
    return -1;
  }
}

void RecordInstallResultMetric(apps::WebAppPreloadResult result) {
  base::UmaHistogramEnumeration("AppPreloadService.WebAppInstall.InstallResult",
                                result);
}

}  // namespace

namespace apps {

WebAppPreloadInstaller::WebAppPreloadInstaller(Profile* profile)
    : profile_(profile) {
  // Check CrosapiManager::IsInitialized as it is not initialized in some unit
  // tests. This should never fail in production code.
  if (web_app::IsWebAppsCrosapiEnabled() &&
      crosapi::CrosapiManager::IsInitialized()) {
    if (crosapi::CrosapiManager::Get()
            ->crosapi_ash()
            ->web_app_service_ash()
            ->GetWebAppProviderBridge() != nullptr) {
      // Set to true if the lacros bridge is already connected.
      lacros_is_connected_ = true;
    } else {
      // Add an observer to observe when the lacros bridge connects.
      crosapi::WebAppServiceAsh* web_app_service_ash =
          crosapi::CrosapiManager::Get()->crosapi_ash()->web_app_service_ash();
      web_app_service_observer_.Observe(web_app_service_ash);
    }
  }
}

WebAppPreloadInstaller::~WebAppPreloadInstaller() = default;

void WebAppPreloadInstaller::InstallAllApps(
    std::vector<PreloadAppDefinition> apps,
    WebAppPreloadInstalledCallback callback) {
  CHECK(!installation_complete_callback_);
  installation_complete_callback_ = std::move(callback);
  apps_for_installation_ = apps;

  if (web_app::IsWebAppsCrosapiEnabled()) {
    if (lacros_is_connected_) {
      InstallAllAppsWhenReady();
    }
  } else {
    auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);
    CHECK(provider);
    provider->on_registry_ready().Post(
        FROM_HERE,
        base::BindOnce(&WebAppPreloadInstaller::InstallAllAppsWhenReady,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

std::string WebAppPreloadInstaller::GetAppId(
    const PreloadAppDefinition& app) const {
  // The app's "Web app manifest ID" is the equivalent of the unhashed app ID.
  return web_app::GenerateAppIdFromManifestId(app.GetWebAppManifestId());
}

void WebAppPreloadInstaller::OnWebAppProviderBridgeConnected() {
  lacros_is_connected_ = true;
  InstallAllAppsWhenReady();
}

void WebAppPreloadInstaller::InstallAllAppsWhenReady() {
  if (!apps_for_installation_.has_value()) {
    return;
  }

  // Request installation of any remaining apps. If there are no apps to
  // install, OnAllAppInstallationFinished will be called immediately.
  const auto install_barrier_callback = base::BarrierCallback<bool>(
      apps_for_installation_.value().size(),
      base::BindOnce(&WebAppPreloadInstaller::OnAllAppInstallationFinished,
                     weak_ptr_factory_.GetWeakPtr()));

  for (const PreloadAppDefinition& app : apps_for_installation_.value()) {
    CHECK_EQ(app.GetPlatform(), AppType::kWeb);
    InstallAppImpl(app, install_barrier_callback);
  }

  // Reset the values after installation has been performed.
  apps_for_installation_ = absl::nullopt;
}

void WebAppPreloadInstaller::InstallAppImpl(
    PreloadAppDefinition app,
    WebAppPreloadInstalledCallback callback) {
  // Retrieve web manifest
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(app.GetWebAppManifestUrl());

  if (!resource_request->url.is_valid()) {
    LOG(ERROR) << "Manifest URL for " << app.GetName()
               << "is invalid: " << resource_request->url;
    RecordInstallResultMetric(WebAppPreloadResult::kInvalidManifestUrl);
    std::move(callback).Run(/*success=*/false);
    return;
  }

  resource_request->method = "GET";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       kTrafficAnnotation);

  auto* loader_ptr = simple_loader.get();

  loader_ptr->DownloadToString(
      profile_->GetURLLoaderFactory().get(),
      base::BindOnce(&WebAppPreloadInstaller::OnManifestRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), app, std::move(callback),
                     std::move(simple_loader)),
      kMaxManifestSizeInBytes);
}

void WebAppPreloadInstaller::OnManifestRetrieved(
    PreloadAppDefinition app,
    WebAppPreloadInstalledCallback callback,
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    std::unique_ptr<std::string> response) {
  if (url_loader->NetError() != net::OK) {
    LOG(ERROR) << "Downloading manifest failed for " << app.GetName()
               << " with error code: " << GetResponseCode(url_loader.get());

    RecordInstallResultMetric(url_loader->NetError() ==
                                      net::ERR_HTTP_RESPONSE_CODE_FAILURE
                                  ? WebAppPreloadResult::kManifestResponseError
                                  : WebAppPreloadResult::kManifestNetworkError);
    std::move(callback).Run(/*success=*/false);
    return;
  }

  if (response->empty()) {
    RecordInstallResultMetric(WebAppPreloadResult::kManifestResponseEmpty);
    std::move(callback).Run(/*success=*/false);
    return;
  }

  auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);

  if (web_app::IsWebAppsCrosapiEnabled()) {
    crosapi::mojom::WebAppProviderBridge* web_app_provider_bridge =
        crosapi::CrosapiManager::Get()
            ->crosapi_ash()
            ->web_app_service_ash()
            ->GetWebAppProviderBridge();
    if (!web_app_provider_bridge) {
      std::move(callback).Run(/*success=*/false);
      return;
    }
    auto web_app_install_info = crosapi::mojom::PreloadWebAppInstallInfo::New();
    web_app_install_info->document_url =
        GURL(app.GetWebAppManifestId()).GetWithEmptyPath();
    web_app_install_info->manifest_url = app.GetWebAppOriginalManifestUrl();
    web_app_install_info->expected_app_id = GetAppId(app);
    web_app_install_info->manifest = std::move(*response);

    web_app_provider_bridge->InstallPreloadWebApp(
        std::move(web_app_install_info),
        base::BindOnce(&WebAppPreloadInstaller::OnAppInstalled,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  } else {
    provider->command_manager().ScheduleCommand(
        std::make_unique<web_app::InstallPreloadedVerifiedAppCommand>(
            webapps::WebappInstallSource::PRELOADED_OEM,
            /*document_url=*/GURL(app.GetWebAppManifestId()).GetWithEmptyPath(),
            /*manifest_url=*/app.GetWebAppOriginalManifestUrl(),
            std::move(*response), GetAppId(app),
            base::BindOnce(&WebAppPreloadInstaller::OnAppInstalled,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback))));
  }
}

void WebAppPreloadInstaller::OnAppInstalled(
    WebAppPreloadInstalledCallback callback,
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  bool success = webapps::IsSuccess(code);
  RecordInstallResultMetric(success ? WebAppPreloadResult::kSuccess
                                    : WebAppPreloadResult::kWebAppInstallError);
  base::UmaHistogramEnumeration(kCommandResultCodeHistogramName, code);

  std::move(callback).Run(success);
}

void WebAppPreloadInstaller::OnAllAppInstallationFinished(
    const std::vector<bool>& results) {
  CHECK(installation_complete_callback_);
  std::move(installation_complete_callback_)
      .Run(base::ranges::all_of(results, [](bool b) { return b; }));
}

}  // namespace apps

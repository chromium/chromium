// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/web_app_installer.h"

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
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

// Maximum size of the manifest file. 1MB.
constexpr int kMaxManifestSizeInBytes = 1024 * 1024;

// TODO(b/315077325): Rename annotation to be related to AppInstallService
// instead of AppPreloadService.
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

int GetResponseCode(network::SimpleURLLoader* simple_loader) {
  if (simple_loader->ResponseInfo() && simple_loader->ResponseInfo()->headers) {
    return simple_loader->ResponseInfo()->headers->response_code();
  } else {
    return -1;
  }
}

void RecordInstallResultMetric(apps::AppInstallSurface surface,
                               apps::WebAppInstallResult result) {
  base::UmaHistogramEnumeration(
      "Apps.AppInstallService.WebAppInstaller.InstallResult", result);
  base::UmaHistogramEnumeration(
      base::StrCat({"Apps.AppInstallService.WebAppInstaller.InstallResult.",
                    base::ToString(surface)}),
      result);
}

void RecordCommandResultMetric(apps::AppInstallSurface surface,
                               webapps::InstallResultCode code) {
  base::UmaHistogramEnumeration(
      "Apps.AppInstallService.WebAppInstaller.CommandResultCode", code);
  base::UmaHistogramEnumeration(
      base::StrCat({"Apps.AppInstallService.WebAppInstaller.CommandResultCode.",
                    base::ToString(surface)}),
      code);
}

}  // namespace

namespace apps {

WebAppInstaller::WebAppInstaller(Profile* profile)
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

WebAppInstaller::~WebAppInstaller() = default;

void WebAppInstaller::InstallApp(AppInstallSurface surface,
                                 AppInstallData data,
                                 WebAppInstalledCallback callback) {
  CHECK(absl::holds_alternative<WebAppInstallData>(data.app_type_data));
  pending_requests_.emplace_back(surface, std::move(data), std::move(callback));

  if (web_app::IsWebAppsCrosapiEnabled()) {
    if (lacros_is_connected_) {
      InstallPendingRequests();
    }
  } else {
    auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);
    CHECK(provider);
    provider->on_registry_ready().Post(
        FROM_HERE, base::BindOnce(&WebAppInstaller::InstallPendingRequests,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

WebAppInstaller::InstallRequest::InstallRequest(
    AppInstallSurface surface,
    AppInstallData data,
    WebAppInstalledCallback callback)
    : surface(surface), data(std::move(data)), callback(std::move(callback)) {}

WebAppInstaller::InstallRequest::InstallRequest(InstallRequest&&) = default;
WebAppInstaller::InstallRequest::~InstallRequest() = default;

void WebAppInstaller::OnWebAppProviderBridgeConnected() {
  lacros_is_connected_ = true;
  InstallPendingRequests();
}

void WebAppInstaller::OnWebAppServiceAshDestroyed() {
  web_app_service_observer_.Reset();
}

void WebAppInstaller::InstallPendingRequests() {
  for (InstallRequest& request : std::exchange(pending_requests_, {})) {
    CHECK_EQ(request.data.package_id.app_type(), AppType::kWeb);
    InstallAppImpl(std::move(request));
  }
  CHECK(pending_requests_.empty());
}

void WebAppInstaller::InstallAppImpl(InstallRequest request) {
  // Retrieve web manifest
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url =
      absl::get<WebAppInstallData>(request.data.app_type_data)
          .proxied_manifest_url;

  if (!resource_request->url.is_valid()) {
    LOG(ERROR) << "Manifest URL for " << request.data.name
               << "is invalid: " << resource_request->url;
    RecordInstallResultMetric(request.surface,
                              WebAppInstallResult::kInvalidManifestUrl);
    std::move(request.callback).Run(/*success=*/false);
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
      base::BindOnce(&WebAppInstaller::OnManifestRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request),
                     std::move(simple_loader)),
      kMaxManifestSizeInBytes);
}

void WebAppInstaller::OnManifestRetrieved(
    InstallRequest request,
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    std::unique_ptr<std::string> response) {
  if (url_loader->NetError() != net::OK) {
    LOG(ERROR) << "Downloading manifest failed for " << request.data.name
               << " with error code: " << GetResponseCode(url_loader.get());

    RecordInstallResultMetric(
        request.surface,
        url_loader->NetError() == net::ERR_HTTP_RESPONSE_CODE_FAILURE
            ? WebAppInstallResult::kManifestResponseError
            : WebAppInstallResult::kManifestNetworkError);
    std::move(request.callback).Run(/*success=*/false);
    return;
  }

  if (response->empty()) {
    RecordInstallResultMetric(request.surface,
                              WebAppInstallResult::kManifestResponseEmpty);
    std::move(request.callback).Run(/*success=*/false);
    return;
  }

  webapps::AppId expected_app_id = web_app::GenerateAppIdFromManifestId(
      GURL(request.data.package_id.identifier()));

  auto& web_app_data = absl::get<WebAppInstallData>(request.data.app_type_data);

  auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);

  if (web_app::IsWebAppsCrosapiEnabled()) {
    crosapi::mojom::WebAppProviderBridge* web_app_provider_bridge =
        crosapi::CrosapiManager::Get()
            ->crosapi_ash()
            ->web_app_service_ash()
            ->GetWebAppProviderBridge();
    if (!web_app_provider_bridge) {
      std::move(request.callback).Run(/*success=*/false);
      return;
    }

    auto web_app_install_info = crosapi::mojom::PreloadWebAppInstallInfo::New();
    web_app_install_info->document_url = web_app_data.document_url;
    web_app_install_info->manifest_url = web_app_data.original_manifest_url;
    web_app_install_info->expected_app_id = expected_app_id;
    web_app_install_info->manifest = std::move(*response);
    web_app_install_info->install_source = [&] {
      switch (request.surface) {
        case AppInstallSurface::kAppInstallNavigationThrottle:
          // TODO(b/315078159): Support non-preload installs over crosapi.
          NOTREACHED();
          [[fallthrough]];
        case AppInstallSurface::kAppPreloadServiceOem:
          return crosapi::mojom::PreloadWebAppInstallSource::kOemPreload;
        case AppInstallSurface::kAppPreloadServiceDefault:
          return crosapi::mojom::PreloadWebAppInstallSource::kDefaultPreload;
      }
    }();

    web_app_provider_bridge->InstallPreloadWebApp(
        std::move(web_app_install_info),
        base::BindOnce(&WebAppInstaller::OnAppInstalled,
                       weak_ptr_factory_.GetWeakPtr(), request.surface,
                       std::move(request.callback)));
    return;
  } else {
    webapps::WebappInstallSource install_source = [&] {
      switch (request.surface) {
        case AppInstallSurface::kAppInstallNavigationThrottle:
          // TODO(b/315078159): Add nav throttle as a new surface.
          NOTREACHED();
          [[fallthrough]];
        case AppInstallSurface::kAppPreloadServiceOem:
          return webapps::WebappInstallSource::PRELOADED_OEM;
        case AppInstallSurface::kAppPreloadServiceDefault:
          return webapps::WebappInstallSource::PRELOADED_DEFAULT;
      }
    }();

    provider->command_manager().ScheduleCommand(
        std::make_unique<web_app::InstallPreloadedVerifiedAppCommand>(
            install_source,
            /*document_url=*/web_app_data.document_url,
            /*manifest_url=*/web_app_data.original_manifest_url,
            std::move(*response), expected_app_id,
            base::BindOnce(&WebAppInstaller::OnAppInstalled,
                           weak_ptr_factory_.GetWeakPtr(), request.surface,
                           std::move(request.callback))));
  }
}

void WebAppInstaller::OnAppInstalled(AppInstallSurface surface,
                                     WebAppInstalledCallback callback,
                                     const webapps::AppId& app_id,
                                     webapps::InstallResultCode code) {
  bool success = webapps::IsSuccess(code);
  RecordInstallResultMetric(surface,
                            success ? WebAppInstallResult::kSuccess
                                    : WebAppInstallResult::kWebAppInstallError);
  RecordCommandResultMetric(surface, code);

  std::move(callback).Run(success);
}

}  // namespace apps

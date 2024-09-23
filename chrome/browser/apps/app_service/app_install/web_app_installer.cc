// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/web_app_installer.h"

#include <memory>

#include "base/barrier_callback.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/install_app_from_verified_manifest_command.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
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

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("app_install_service_web_app_installer",
                                        R"(
      semantics {
        sender: "App Install Service"
        description:
          "Sends a request to a Google server to retrieve web app installation"
          "data."
        trigger:
          "Requests are sent as part of App Install Service triggered installs "
          "for web apps."
        internal: {
          contacts {
            email: "cros-apps-foundation-system@google.com"
          }
        }
        user_data: {
          type: NONE
        }
        data: "None"
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2024-01-02"
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

WebAppInstaller::WebAppInstaller(Profile* profile) : profile_(profile) {
  // Check CrosapiManager::IsInitialized as it is not initialized in some unit
  // tests. This should never fail in production code.
  if (web_app::IsWebAppsCrosapiEnabled() &&
      crosapi::CrosapiManager::IsInitialized()) {
    // Add an observer to observe when the lacros bridge connects.
    crosapi::WebAppServiceAsh* web_app_service_ash =
        crosapi::CrosapiManager::Get()->crosapi_ash()->web_app_service_ash();
    web_app_service_observer_.Observe(web_app_service_ash);
  }
}

WebAppInstaller::~WebAppInstaller() = default;

void WebAppInstaller::InstallApp(AppInstallSurface surface,
                                 AppInstallData data,
                                 WebAppInstalledCallback callback) {
  CHECK(absl::holds_alternative<WebAppInstallData>(data.app_type_data));

  // Retrieve web manifest
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url =
      absl::get<WebAppInstallData>(data.app_type_data).proxied_manifest_url;

  if (!resource_request->url.is_valid()) {
    LOG(ERROR) << "Manifest URL for " << data.name
               << "is invalid: " << resource_request->url;
    RecordInstallResultMetric(surface,
                              WebAppInstallResult::kInvalidManifestUrl);
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
      base::BindOnce(&WebAppInstaller::OnManifestRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(surface),
                     std::move(data), std::move(callback),
                     std::move(simple_loader)),
      kMaxManifestSizeInBytes);
}

void WebAppInstaller::OnWebAppProviderBridgeConnected() {
  MaybeSendPendingCrosapiRequests();
}

void WebAppInstaller::OnWebAppServiceAshDestroyed() {
  web_app_service_observer_.Reset();
}

void WebAppInstaller::OnManifestRetrieved(
    AppInstallSurface surface,
    AppInstallData data,
    WebAppInstalledCallback callback,
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    std::unique_ptr<std::string> response) {
  if (url_loader->NetError() != net::OK) {
    LOG(ERROR) << "Downloading manifest failed for " << data.name
               << " with error code: " << GetResponseCode(url_loader.get());

    RecordInstallResultMetric(
        surface, url_loader->NetError() == net::ERR_HTTP_RESPONSE_CODE_FAILURE
                     ? WebAppInstallResult::kManifestResponseError
                     : WebAppInstallResult::kManifestNetworkError);
    std::move(callback).Run(/*success=*/false);
    return;
  }

  if (response->empty()) {
    RecordInstallResultMetric(surface,
                              WebAppInstallResult::kManifestResponseEmpty);
    std::move(callback).Run(/*success=*/false);
    return;
  }

  webapps::AppId expected_app_id =
      web_app::GenerateAppIdFromManifestId(GURL(data.package_id.identifier()));

  auto& web_app_data = absl::get<WebAppInstallData>(data.app_type_data);

  auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);

  if (web_app::IsWebAppsCrosapiEnabled()) {
    auto web_app_install_info =
        crosapi::mojom::WebAppVerifiedManifestInstallInfo::New();
    web_app_install_info->document_url = web_app_data.document_url;
    web_app_install_info->verified_manifest_url =
        web_app_data.original_manifest_url;
    web_app_install_info->expected_app_id = expected_app_id;
    web_app_install_info->verified_manifest_contents = std::move(*response);
    web_app_install_info->install_source = [&] {
      switch (surface) {
        case AppInstallSurface::kAppInstallUriUnknown:
        case AppInstallSurface::kAppInstallUriShowoff:
        case AppInstallSurface::kAppInstallUriMall:
        case AppInstallSurface::kAppInstallUriMallV2:
        case AppInstallSurface::kAppInstallUriGetit:
        case AppInstallSurface::kAppInstallUriLauncher:
        case AppInstallSurface::kAppInstallUriPeripherals:
          return crosapi::mojom::WebAppInstallSource::kAlmanacInstallAppUri;
        case AppInstallSurface::kAppPreloadServiceOem:
          return crosapi::mojom::WebAppInstallSource::kOemPreload;
        case AppInstallSurface::kAppPreloadServiceDefault:
          return crosapi::mojom::WebAppInstallSource::kDefaultPreload;
        case AppInstallSurface::kOobeAppRecommendations:
          return crosapi::mojom::WebAppInstallSource::kOobeAppRecommendations;
      }
    }();

    pending_crosapi_requests_.emplace_back(
        std::move(web_app_install_info),
        base::BindOnce(&WebAppInstaller::OnAppInstalled,
                       weak_ptr_factory_.GetWeakPtr(), surface,
                       std::move(callback)));
    MaybeSendPendingCrosapiRequests();
    return;
  } else {
    webapps::WebappInstallSource install_source = [&] {
      switch (surface) {
        case AppInstallSurface::kAppInstallUriUnknown:
        case AppInstallSurface::kAppInstallUriShowoff:
        case AppInstallSurface::kAppInstallUriMall:
        case AppInstallSurface::kAppInstallUriMallV2:
        case AppInstallSurface::kAppInstallUriGetit:
        case AppInstallSurface::kAppInstallUriLauncher:
        case AppInstallSurface::kAppInstallUriPeripherals:
          return webapps::WebappInstallSource::ALMANAC_INSTALL_APP_URI;
        case AppInstallSurface::kAppPreloadServiceOem:
          return webapps::WebappInstallSource::PRELOADED_OEM;
        case AppInstallSurface::kAppPreloadServiceDefault:
          return webapps::WebappInstallSource::PRELOADED_DEFAULT;
        case AppInstallSurface::kOobeAppRecommendations:
          return webapps::WebappInstallSource::OOBE_APP_RECOMMENDATIONS;
      }
    }();

    bool is_website = data.package_id.package_type() == PackageType::kWebsite;
    web_app::WebAppInstallParams install_params;
    if (is_website) {
      install_params.user_display_mode =
          web_app_data.open_as_window
              ? web_app::mojom::UserDisplayMode::kStandalone
              : web_app::mojom::UserDisplayMode::kBrowser;
    }

    provider->command_manager().ScheduleCommand(
        std::make_unique<web_app::InstallAppFromVerifiedManifestCommand>(
            install_source,
            /*document_url=*/web_app_data.document_url,
            /*verified_manifest_url=*/web_app_data.original_manifest_url,
            /*verified_manifest_contents=*/std::move(*response),
            expected_app_id, /*is_diy_app=*/is_website, install_params,
            base::BindOnce(&WebAppInstaller::OnAppInstalled,
                           weak_ptr_factory_.GetWeakPtr(), surface,
                           std::move(callback))));
  }
}

void WebAppInstaller::MaybeSendPendingCrosapiRequests() {
  CHECK(web_app::IsWebAppsCrosapiEnabled());

  crosapi::mojom::WebAppProviderBridge* web_app_provider_bridge =
      crosapi::CrosapiManager::Get()
          ->crosapi_ash()
          ->web_app_service_ash()
          ->GetWebAppProviderBridge();
  if (!web_app_provider_bridge) {
    return;
  }

  for (PendingCrosapiRequest& request :
       std::exchange(pending_crosapi_requests_, {})) {
    web_app_provider_bridge->InstallWebAppFromVerifiedManifest(
        std::move(request.info), std::move(request.callback));
  }
  CHECK(pending_crosapi_requests_.empty());
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

WebAppInstaller::PendingCrosapiRequest::PendingCrosapiRequest(
    crosapi::mojom::WebAppVerifiedManifestInstallInfoPtr info,
    crosapi::mojom::WebAppProviderBridge::
        InstallWebAppFromVerifiedManifestCallback callback)
    : info(std::move(info)), callback(std::move(callback)) {}

WebAppInstaller::PendingCrosapiRequest::PendingCrosapiRequest(
    PendingCrosapiRequest&&) = default;

WebAppInstaller::PendingCrosapiRequest::~PendingCrosapiRequest() = default;

}  // namespace apps

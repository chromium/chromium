// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/web_app_preload_installer.h"

#include <memory>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "url/origin.h"

namespace {

// Maximum size of the manifest file. 1MB.
constexpr int kMaxManifestSizeInBytes = 1024 * 1024;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("app_preload_service_web_installer", R"(
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

}  // namespace

namespace apps {

WebAppPreloadInstaller::WebAppPreloadInstaller(Profile* profile)
    : profile_(profile) {}

WebAppPreloadInstaller::~WebAppPreloadInstaller() = default;

std::unique_ptr<WebAppInstallInfo>
WebAppPreloadInstaller::ManifestToWebAppInstallInfo(
    GURL document_url,
    GURL original_manifest_url,
    base::Value::Dict& manifest) {
  auto install_info = std::make_unique<WebAppInstallInfo>();

  // Shared error message
  constexpr char kSharedError[] =
      "Unable to convert manifest into WebAppInstallInfo. Field that was not "
      "defined or was the wrong type: ";

  if (!document_url.is_valid()) {
    VLOG(1) << kSharedError << "Document URL";
    return nullptr;
  }

  if (!original_manifest_url.is_valid()) {
    VLOG(1) << kSharedError << "Original Manifest URL";
    return nullptr;
  }

  // Manifest keys
  constexpr char kName[] = "name";
  constexpr char kStartUrl[] = "start_url";
  constexpr char kManifestId[] = "id";
  constexpr char kScope[] = "scope";

  // Title
  auto* title = manifest.FindString(kName);
  if (!title) {
    VLOG(1) << kSharedError << kName;
    return nullptr;
  }
  install_info->title = base::UTF8ToUTF16(*title);

  // Start URL
  auto* start_url = manifest.FindString(kStartUrl);
  if (!start_url) {
    VLOG(1) << kSharedError << kStartUrl;
    return nullptr;
  }
  install_info->start_url = original_manifest_url.Resolve(*start_url);
  if (!install_info->start_url.is_valid()) {
    VLOG(1) << kSharedError << kStartUrl << ". Value: " << *start_url;
    return nullptr;
  }
  if (!url::IsSameOriginWith(document_url, install_info->start_url)) {
    VLOG(1) << kSharedError << kStartUrl << ". Value: " << *start_url
            << ". Mismatch in origin with document url: " << document_url;
    return nullptr;
  }

  // Manifest ID
  auto* manifest_id = manifest.FindString(kManifestId);
  if (manifest_id) {
    GURL start_url_origin = install_info->start_url.GetWithEmptyPath();
    GURL processed_id = start_url_origin.Resolve(*manifest_id);
    if (processed_id.is_valid() &&
        url::IsSameOriginWith(start_url_origin, processed_id)) {
      install_info->manifest_id = processed_id.spec().substr(
          processed_id.GetWithEmptyPath().spec().size());
    }
  }

  // Scope
  auto* scope = manifest.FindString(kScope);
  if (scope) {
    install_info->scope = original_manifest_url.Resolve(*scope);
  }

  if (install_info->scope.is_empty() || !install_info->scope.is_valid() ||
      !url::IsSameOriginWith(install_info->start_url, install_info->scope) ||
      !base::StartsWith(install_info->start_url.path(),
                        install_info->scope.path(),
                        base::CompareCase::SENSITIVE)) {
    install_info->scope = install_info->start_url;
  }

  // Display mode
  install_info->display_mode = blink::mojom::DisplayMode::kStandalone;
  install_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;

  return install_info;
}

void WebAppPreloadInstaller::InstallApp(
    const PreloadAppDefinition& app,
    WebAppPreloadInstalledCallback callback) {
  DCHECK_EQ(app.GetPlatform(), AppType::kWeb);

  auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);
  DCHECK(provider);

  provider->on_registry_ready().Post(
      FROM_HERE,
      base::BindOnce(&WebAppPreloadInstaller::InstallAppImpl,
                     weak_ptr_factory_.GetWeakPtr(), app, std::move(callback)));
}

std::string WebAppPreloadInstaller::GetAppId(
    const PreloadAppDefinition& app) const {
  // The app's "Web app manifest ID" is the equivalent of the unhashed app ID.
  return web_app::GenerateAppIdFromUnhashed(app.GetWebAppManifestId().spec());
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
  if (url_loader->NetError() != net::OK || response->empty()) {
    LOG(ERROR) << "Downloading manifest failed for " << app.GetName()
               << " with error code: " << GetResponseCode(url_loader.get());
    std::move(callback).Run(/*success=*/false);
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response,
      base::BindOnce(&WebAppPreloadInstaller::OnManifestParsed,
                     weak_ptr_factory_.GetWeakPtr(), app, std::move(callback)));
}

void WebAppPreloadInstaller::OnManifestParsed(
    PreloadAppDefinition app,
    WebAppPreloadInstalledCallback callback,
    data_decoder::DataDecoder::ValueOrError parsing_result) {
  if (!parsing_result.has_value() || !parsing_result->is_dict()) {
    LOG(ERROR) << "Parsing the manifest for " << app.GetName()
               << " has failed. Parsing error: " << parsing_result.error();
    std::move(callback).Run(/*success=*/false);
    return;
  }

  std::unique_ptr<WebAppInstallInfo> install_info = ManifestToWebAppInstallInfo(
      GURL(app.GetWebAppManifestId()), app.GetWebAppOriginalManifestUrl(),
      parsing_result->GetDict());

  if (!install_info) {
    LOG(ERROR)
        << "Failed to convert parsed manifest into WebAppInstallInfo for app "
        << app.GetName();
    std::move(callback).Run(/*success=*/false);
    return;
  }

  std::string local_manifest_id = web_app::GenerateAppIdUnhashed(
      install_info->manifest_id, install_info->start_url);
  if (app.GetWebAppManifestId() != local_manifest_id) {
    // The data parsing has some inconsistencies with the server definition, so
    // don't install the app.
    LOG(ERROR) << app.GetName()
               << " failed to install due to inconsistent manifest ID.";
    LOG(ERROR) << "Server generated manifest ID: " << app.GetWebAppManifestId();
    LOG(ERROR) << "Locally generated manifest ID: " << local_manifest_id;

    // TODO(b/264493427): Add logging to record when this happens.
    std::move(callback).Run(/*success=*/false);
    return;
  }

  SendInstallCommand(std::move(callback), std::move(install_info));
}

void WebAppPreloadInstaller::SendInstallCommand(
    WebAppPreloadInstalledCallback callback,
    std::unique_ptr<WebAppInstallInfo> install_info) {
  web_app::WebAppInstallParams params;
  params.add_to_quick_launch_bar = false;

  auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);

  if (!install_info) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  provider->scheduler().InstallFromInfoWithParams(
      std::move(install_info),
      /*overwrite_existing_manifest_fields=*/false,
      webapps::WebappInstallSource::PRELOADED_OEM,
      base::BindOnce(&WebAppPreloadInstaller::OnAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      params);
}

void WebAppPreloadInstaller::OnAppInstalled(
    WebAppPreloadInstalledCallback callback,
    const web_app::AppId& app_id,
    webapps::InstallResultCode code) {
  std::move(callback).Run(webapps::IsSuccess(code));
}

}  // namespace apps

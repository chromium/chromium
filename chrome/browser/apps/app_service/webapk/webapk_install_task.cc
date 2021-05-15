// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/webapk/webapk_install_task.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/apps/app_service/webapk/webapk_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/common/chrome_switches.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/mojom/webapk.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/version_info/version_info.h"
#include "components/webapk/webapk.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/smhasher/src/MurmurHash2.h"
#include "url/gurl.h"

namespace {

// The MIME type of the POST data sent to the server.
constexpr char kProtoMimeType[] = "application/x-protobuf";

constexpr char kRequesterPackageName[] = "org.chromium.arc.webapk";

constexpr char kAbiListPropertyName[] = "ro.product.cpu.abilist";

const char kMinimumIconSize = 64;

// The seed to use when taking the murmur2 hash of the icon.
const uint64_t kMurmur2HashSeed = 0;

constexpr char kWebApkServerUrl[] =
    "https://webapk.googleapis.com/v1/webApks?key=";

constexpr net::NetworkTrafficAnnotationTag kWebApksTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("webapk_minter_install_request",
                                        R"(
        semantics {
          sender: "WebAPKs"
          description:
            "Chrome OS generates small Android apps called 'WebAPKs' which "
            "represent the Progressive Web Apps installed in Chrome OS. These "
            "apps are installed in the ARC Android environment and used to "
            "improve integration between ARC and Chrome OS. This network "
            "request sends the details for a single web app to the WebAPK "
            "service, which returns details about the WebAPK to install."
          trigger: "Installing or updating a progressive web app."
          data:
            "The contents of the web app manifest for the web app, plus system "
            "information needed to generate the app."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          cookies_store: "N/A"
          setting: "No setting apart from disabling ARC"
          policy_exception_justification = "Not implemented"
        }
      )");

GURL GetServerUrl() {
  std::string server_url =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kWebApkServerUrl);

  if (server_url.empty()) {
    server_url = base::StrCat({kWebApkServerUrl, google_apis::GetAPIKey()});
  }

  return GURL(server_url);
}

// Attaches icon PNG data and hash to an existing icon entry, and then
// serializes and returns the entire proto. Should be called on a worker thread.
std::string AddIconDataAndSerializeProto(std::unique_ptr<webapk::WebApk> webapk,
                                         std::vector<uint8_t> icon_data) {
  base::AssertLongCPUWorkAllowed();
  DCHECK_EQ(webapk->manifest().icons_size(), 1);

  webapk::Image* icon = webapk->mutable_manifest()->mutable_icons(0);
  icon->set_image_data(icon_data.data(), icon_data.size());

  uint64_t icon_hash =
      MurmurHash64A(icon_data.data(), icon_data.size(), kMurmur2HashSeed);
  icon->set_hash(base::NumberToString(icon_hash));

  std::string serialized_proto;
  webapk->SerializeToString(&serialized_proto);

  return serialized_proto;
}

std::string GetArcAbi(const arc::ArcFeatures& arc_features) {
  const std::string& property =
      arc_features.build_props.at(kAbiListPropertyName);
  size_t separator_pos = property.find(',');
  if (separator_pos != std::string::npos) {
    return property.substr(0, separator_pos);
  }

  return property;
}

}  // namespace

namespace apps {

WebApkInstallTask::WebApkInstallTask(Profile* profile,
                                     const std::string& app_id)
    : profile_(profile),
      web_app_provider_(web_app::WebAppProviderBase::GetProviderBase(profile_)),
      app_id_(app_id) {
  DCHECK(web_app_provider_);
}

WebApkInstallTask::~WebApkInstallTask() = default;

void WebApkInstallTask::Start(ResultCallback callback) {
  VLOG(1) << "Generating WebAPK for app: " << app_id_;

  auto& registrar = web_app_provider_->registrar();

  // This is already checked in WebApkManager, check again in case anything
  // changed while the install request was queued.
  if (!registrar.IsInstalled(app_id_) ||
      !registrar.GetAppShareTarget(app_id_)) {
    std::move(callback).Run(false);
    return;
  }

  std::unique_ptr<webapk::WebApk> webapk = std::make_unique<webapk::WebApk>();
  webapk->set_manifest_url(registrar.GetAppManifestUrl(app_id_).spec());
  webapk->set_requester_application_package(kRequesterPackageName);
  webapk->set_requester_application_version(version_info::GetVersionNumber());
  webapk->add_update_reasons(webapk::WebApk::NONE);

  arc::ArcFeaturesParser::GetArcFeatures(base::BindOnce(
      &WebApkInstallTask::OnArcFeaturesLoaded, weak_ptr_factory_.GetWeakPtr(),
      std::move(webapk), std::move(callback)));
}

void WebApkInstallTask::OnArcFeaturesLoaded(
    std::unique_ptr<webapk::WebApk> webapk,
    ResultCallback callback,
    absl::optional<arc::ArcFeatures> arc_features) {
  if (!arc_features) {
    LOG(ERROR) << "Could not load ArcFeatures";
    std::move(callback).Run(false);
    return;
  }
  webapk->set_android_abi(GetArcAbi(arc_features.value()));

  auto& icon_manager = web_app_provider_->icon_manager();
  absl::optional<web_app::AppIconManager::IconSizeAndPurpose>
      icon_size_and_purpose = icon_manager.FindIconMatchBigger(
          app_id_, {IconPurpose::MASKABLE, IconPurpose::ANY}, kMinimumIconSize);

  if (!icon_size_and_purpose) {
    LOG(ERROR) << "Could not find suitable icon";
    std::move(callback).Run(false);
    return;
  }

  // We need to send a URL for the icon, but it's possible the local image we're
  // sending has been resized and so doesn't exactly match any of the images in
  // the manifest. Since we can't be perfect, it's okay to be roughly correct
  // and just send any URL of the correct purpose.
  auto& registrar = web_app_provider_->registrar();
  const auto& icon_infos = registrar.GetAppIconInfos(app_id_);
  auto it = std::find_if(
      icon_infos.begin(), icon_infos.end(),
      [&icon_size_and_purpose](const WebApplicationIconInfo& info) {
        return info.purpose == icon_size_and_purpose->purpose;
      });

  if (it == icon_infos.end()) {
    LOG(ERROR) << "Could not find URL for icon";
    std::move(callback).Run(false);
    return;
  }
  std::string icon_url = it->url.spec();

  webapk::WebAppManifest* web_app_manifest = webapk->mutable_manifest();
  web_app_manifest->set_short_name(registrar.GetAppShortName(app_id_));
  web_app_manifest->set_start_url(registrar.GetAppStartUrl(app_id_).spec());
  web_app_manifest->add_scopes(registrar.GetAppScope(app_id_).spec());

  auto* share_target = registrar.GetAppShareTarget(app_id_);
  webapk::ShareTarget* proto_share_target =
      web_app_manifest->add_share_targets();
  proto_share_target->set_action(share_target->action.spec());
  proto_share_target->set_method(
      apps::ShareTarget::MethodToString(share_target->method));
  proto_share_target->set_enctype(
      apps::ShareTarget::EnctypeToString(share_target->enctype));

  webapk::ShareTargetParams* proto_params =
      proto_share_target->mutable_params();
  if (!share_target->params.title.empty()) {
    proto_params->set_title(share_target->params.title);
  }
  if (!share_target->params.text.empty()) {
    proto_params->set_text(share_target->params.text);
  }
  if (!share_target->params.url.empty()) {
    proto_params->set_url(share_target->params.url);
  }

  for (const auto& file : share_target->params.files) {
    webapk::ShareTargetParamsFile* proto_file = proto_params->add_files();
    proto_file->set_name(file.name);
    for (const auto& accept_type : file.accept) {
      proto_file->add_accept(accept_type);
    }
  }

  webapk::Image* image = web_app_manifest->add_icons();
  image->set_src(std::move(icon_url));
  image->add_purposes(icon_size_and_purpose->purpose == IconPurpose::MASKABLE
                          ? webapk::Image::MASKABLE
                          : webapk::Image::ANY);
  image->add_usages(webapk::Image::PRIMARY_ICON);

  icon_manager.ReadSmallestCompressedIcon(
      app_id_, {icon_size_and_purpose->purpose}, icon_size_and_purpose->size_px,
      base::BindOnce(&WebApkInstallTask::OnLoadedIcon,
                     weak_ptr_factory_.GetWeakPtr(), std::move(webapk),
                     std::move(callback)));
}

void WebApkInstallTask::OnLoadedIcon(std::unique_ptr<webapk::WebApk> webapk,
                                     ResultCallback callback,
                                     IconPurpose purpose,
                                     std::vector<uint8_t> data) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(AddIconDataAndSerializeProto, std::move(webapk),
                     std::move(data)),
      base::BindOnce(&WebApkInstallTask::OnProtoSerialized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebApkInstallTask::OnProtoSerialized(ResultCallback callback,
                                          std::string serialized_proto) {
  GURL server_url = GetServerUrl();

  // TODO(crbug.com/1198433): Add timeout.
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = server_url;
  request->method = "POST";
  request->load_flags = net::LOAD_DISABLE_CACHE;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  auto* url_loader_factory = profile_->GetDefaultStoragePartition()
                                 ->GetURLLoaderFactoryForBrowserProcess()
                                 .get();

  url_loader_ = network::SimpleURLLoader::Create(std::move(request),
                                                 kWebApksTrafficAnnotation);
  url_loader_->AttachStringForUpload(std::move(serialized_proto),
                                     kProtoMimeType);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory,
      base::BindOnce(&WebApkInstallTask::OnUrlLoaderComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebApkInstallTask::OnUrlLoaderComplete(
    ResultCallback callback,
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();

  if (!response_body || response_code != net::HTTP_OK) {
    LOG(WARNING) << "WebAPK server returned response code " << response_code;
    std::move(callback).Run(false);
    return;
  }

  auto response = std::make_unique<webapk::WebApkResponse>();
  if (!response->ParseFromString(*response_body)) {
    LOG(WARNING) << "Failed to parse WebApkResponse proto";
    std::move(callback).Run(false);
    return;
  }

  VLOG(1) << "Installing WebAPK: " << response->package_name();

  auto* arc_service_manager = arc::ArcServiceManager::Get();
  DCHECK(arc_service_manager);
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->webapk(), InstallWebApk);

  if (!instance) {
    LOG(ERROR) << "WebApkInstance is not ready";
    std::move(callback).Run(false);
    return;
  }

  auto& registrar = web_app_provider_->registrar();

  int webapk_version;
  base::StringToInt(response->version(), &webapk_version);
  instance->InstallWebApk(
      response->package_name(), webapk_version,
      registrar.GetAppShortName(app_id_), response->token(),
      base::BindOnce(&WebApkInstallTask::OnInstallComplete,
                     weak_ptr_factory_.GetWeakPtr(), response->package_name(),
                     std::move(callback)));
}

void WebApkInstallTask::OnInstallComplete(
    const std::string& package_name,
    ResultCallback callback,
    arc::mojom::WebApkInstallResult result) {
  VLOG(1) << "WebAPK installation finished with result " << result;

  bool success = result == arc::mojom::WebApkInstallResult::kSuccess;
  if (success) {
    webapk_prefs::AddWebApk(profile_, app_id_, package_name);
  }

  std::move(callback).Run(success);
}

}  // namespace apps

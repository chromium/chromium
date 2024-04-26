// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/webapk/webapk_install_task.h"

#include <utility>

#include "ash/components/arc/mojom/webapk.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/apps/app_service/webapk/webapk_prefs.h"
#include "chrome/browser/apps/app_service/webapk/webapk_utils.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/web_app_service_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_switches.h"
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
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/smhasher/src/MurmurHash2.h"
#include "url/gurl.h"

namespace {

// The MIME type of the POST data sent to the server.
constexpr char kProtoMimeType[] = "application/x-protobuf";

constexpr char kRequesterPackageName[] = "org.chromium.arc.webapk";

const char kMinimumIconSize = 64;

// The seed to use when taking the murmur2 hash of the icon.
const uint64_t kMurmur2HashSeed = 0;

// Time to wait for a response from the Web APK minter.
constexpr base::TimeDelta kMinterResponseTimeout = base::Seconds(60);

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
          internal {
            contacts {
              email: "tsergeant@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2023-01-12"
        }
        policy {
          cookies_allowed: NO
          setting: "No setting apart from disabling ARC"
          chrome_policy: {
            ArcAppToWebAppSharingEnabled: {
              ArcAppToWebAppSharingEnabled: true
            }
          }
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

bool DoesShareTargetDiffer(webapk::WebAppManifest manifest,
                           arc::mojom::WebShareTargetInfoPtr share_info) {
  if (!share_info) {
    // There's no |share_info| in the current WebAPK, which means that a share
    // target was added.
    return true;
  }

  // There is only one share target added.
  auto share_target = manifest.share_targets(0);
  DCHECK_EQ(manifest.share_targets_size(), 1);

  if (share_target.action() != share_info->action.value_or("") ||
      share_target.method() != share_info->method.value_or("") ||
      share_target.enctype() != share_info->enctype.value_or("")) {
    return true;
  }

  auto share_param = share_target.params();
  if (share_param.title() != share_info->param_title.value_or("") ||
      share_param.text() != share_info->param_text.value_or("") ||
      share_param.url() != share_info->param_url.value_or("")) {
    return true;
  }

  // Compare share files.
  if (share_param.files_size() !=
      static_cast<int>(share_info->file_names.size())) {
    return true;
  }

  for (int i = 0; i < share_param.files_size(); i++) {
    if (share_param.files(i).name() != share_info->file_names[i]) {
      return true;
    }

    if (share_param.files(i).accept_size() !=
        static_cast<int>(share_info->file_accepts[i].size())) {
      return true;
    }

    for (int j = 0; j < share_param.files(i).accept_size(); j++) {
      if (share_param.files(i).accept(j) != share_info->file_accepts[i][j]) {
        return true;
      }
    }
  }
  return false;
}

void AddUpdateParams(webapk::WebApk* webapk,
                     arc::mojom::WebApkInfoPtr web_apk_info) {
  webapk->set_version(web_apk_info->apk_version);
  // The |manifest_url| is used as a key on Android, so the |manifest_url| sent
  // to the server to query a particular app should always be the same.
  webapk->set_manifest_url(web_apk_info->manifest_url);
  // Any changes to web app identity which make it through to App Service will
  // have gone through an update policy check, which makes it safe to update
  // the WebAPK too.
  webapk->set_app_identity_update_supported(true);

  auto manifest = webapk->manifest();
  if (manifest.short_name() != web_apk_info->name) {
    webapk->add_update_reasons(webapk::WebApk::SHORT_NAME_DIFFERS);
  }

  if (manifest.start_url() != web_apk_info->start_url) {
    webapk->add_update_reasons(webapk::WebApk::START_URL_DIFFERS);
  }

  // There is only one scope added to the scopes list.
  DCHECK_EQ(manifest.scopes_size(), 1);
  if (manifest.scopes(0) != web_apk_info->scope) {
    webapk->add_update_reasons(webapk::WebApk::SCOPE_DIFFERS);
  }

  // There is only one icon added to the icon list.
  DCHECK_EQ(manifest.icons_size(), 1);
  if (manifest.icons(0).hash() != web_apk_info->icon_hash) {
    webapk->add_update_reasons(webapk::WebApk::PRIMARY_ICON_HASH_DIFFERS);
  }

  // Check differences in share target
  if (DoesShareTargetDiffer(manifest, std::move(web_apk_info->share_info))) {
    webapk->add_update_reasons(webapk::WebApk::WEB_SHARE_TARGET_DIFFERS);
  }
}

// Attaches icon PNG data and hash to an existing icon entry, and then
// serializes and returns the entire proto. Should be called on a worker thread.
std::optional<std::string> AddIconDataAndSerializeProto(
    std::unique_ptr<webapk::WebApk> webapk,
    std::vector<uint8_t> icon_data,
    arc::mojom::WebApkInfoPtr web_apk_info) {
  base::AssertLongCPUWorkAllowed();
  DCHECK_EQ(webapk->manifest().icons_size(), 1);

  webapk::Image* icon = webapk->mutable_manifest()->mutable_icons(0);
  if (!icon->has_image_data()) {
    icon->set_image_data(icon_data.data(), icon_data.size());

    uint64_t icon_hash =
        MurmurHash64A(icon_data.data(), icon_data.size(), kMurmur2HashSeed);
    icon->set_hash(base::NumberToString(icon_hash));
  }

  if (web_apk_info) {
    AddUpdateParams(webapk.get(), std::move(web_apk_info));
    // If we don't have an update reason here, return before we query the server
    // as there is no reason to update.
    if (webapk->update_reasons_size() == 0) {
      return std::nullopt;
    }
  }

  std::string serialized_proto;
  webapk->SerializeToString(&serialized_proto);

  return serialized_proto;
}

std::string GetArcAbi(const arc::ArcFeatures& arc_features) {
  // The property value will be a comma separated list, e.g. "x86_64,x86". The
  // highest priority will be listed first.
  return base::SplitString(arc_features.build_props.abi_list, ",",
                           base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)[0];
}

}  // namespace

namespace apps {

// Installs or updates a WebAPK.
WebApkInstallTask::WebApkInstallTask(Profile* profile,
                                     const std::string& app_id)
    : profile_(profile),
      web_app_provider_(web_app::WebAppProvider::GetForWebApps(profile_)),
      app_id_(app_id),
      package_name_to_update_(
          webapk_prefs::GetWebApkPackageName(profile_, app_id_)),
      minter_timeout_(kMinterResponseTimeout) {
  DCHECK(web_app::IsWebAppsCrosapiEnabled() || web_app_provider_);
}

WebApkInstallTask::~WebApkInstallTask() = default;

void WebApkInstallTask::Start(ResultCallback callback) {
  VLOG(1) << "Generating WebAPK for app: " << app_id_;
  result_callback_ = std::move(callback);

  if (web_app::IsWebAppsCrosapiEnabled()) {
    FetchWebApkInfoFromCrosapi();
    return;
  }

  auto& registrar = web_app_provider_->registrar_unsafe();

  // Installation & share target are already checked in WebApkManager, check
  // again in case anything changed while the install request was queued.
  // Manifest URL is always set for apps installed or updated in recent
  // versions, but might be missing for older apps.
  if (!registrar.IsInstalled(app_id_) ||
      !registrar.GetAppShareTarget(app_id_) ||
      registrar.GetAppManifestUrl(app_id_).is_empty()) {
    DeliverResult(WebApkInstallStatus::kAppInvalid);
    return;
  }

  std::unique_ptr<webapk::WebApk> webapk = std::make_unique<webapk::WebApk>();
  webapk->set_manifest_url(registrar.GetAppManifestUrl(app_id_).spec());
  webapk->set_requester_application_package(kRequesterPackageName);
  webapk->set_requester_application_version(
      std::string(version_info::GetVersionNumber()));

  LoadWebApkInfo(std::move(webapk));
}

void WebApkInstallTask::LoadWebApkInfo(std::unique_ptr<webapk::WebApk> webapk) {
  if (!package_name_to_update_.has_value()) {
    // This is a new install, continue with the installation process.
    webapk->add_update_reasons(webapk::WebApk::NONE);
    arc::ArcFeaturesParser::GetArcFeatures(
        base::BindOnce(&WebApkInstallTask::OnArcFeaturesLoaded,
                       weak_ptr_factory_.GetWeakPtr(), std::move(webapk)));
    return;
  }

  // If a package_name exists in webapk_prefs, this WebAPK is already installed,
  // so we need to perform an update.
  webapk->set_package_name(package_name_to_update_.value());

  // Fetch details of the existing WebAPK from ARC++.
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  DCHECK(arc_service_manager);
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->webapk(), GetWebApkInfo);

  if (!instance) {
    LOG(ERROR) << "WebApkInstance is not ready";
    DeliverResult(WebApkInstallStatus::kArcUnavailable);
    return;
  }

  instance->GetWebApkInfo(
      package_name_to_update_.value(),
      base::BindOnce(&WebApkInstallTask::OnWebApkInfoLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(webapk)));
}

void WebApkInstallTask::OnWebApkInfoLoaded(
    std::unique_ptr<webapk::WebApk> webapk,
    arc::mojom::WebApkInfoPtr result) {
  if (!result) {
    LOG(ERROR) << "Could not load WebApkInfo";
    DeliverResult(WebApkInstallStatus::kUpdateGetWebApkInfoError);
    return;
  }

  web_apk_info_ = std::move(result);

  arc::ArcFeaturesParser::GetArcFeatures(
      base::BindOnce(&WebApkInstallTask::OnArcFeaturesLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(webapk)));
}

void WebApkInstallTask::OnArcFeaturesLoaded(
    std::unique_ptr<webapk::WebApk> webapk,
    std::optional<arc::ArcFeatures> arc_features) {
  if (!arc_features) {
    LOG(ERROR) << "Could not load ArcFeatures";
    DeliverResult(WebApkInstallStatus::kArcUnavailable);
    return;
  }
  webapk->set_android_abi(GetArcAbi(arc_features.value()));

  if (web_app::IsWebAppsCrosapiEnabled()) {
    WebApkInstallTask::OnLoadedIcon(std::move(webapk),
                                    web_app::IconPurpose::ANY,
                                    /*data=*/{});
    return;
  }

  auto& icon_manager = web_app_provider_->icon_manager();
  std::optional<web_app::WebAppIconManager::IconSizeAndPurpose>
      icon_size_and_purpose = icon_manager.FindIconMatchBigger(
          app_id_, {web_app::IconPurpose::MASKABLE, web_app::IconPurpose::ANY},
          kMinimumIconSize);

  if (!icon_size_and_purpose) {
    LOG(ERROR) << "Could not find suitable icon";
    DeliverResult(WebApkInstallStatus::kAppInvalid);
    return;
  }

  // We need to send a URL for the icon, but it's possible the local image we're
  // sending has been resized and so doesn't exactly match any of the images in
  // the manifest. Since we can't be perfect, it's okay to be roughly correct
  // and just send any URL of the correct purpose.
  auto& registrar = web_app_provider_->registrar_unsafe();
  const auto& manifest_icons = registrar.GetAppIconInfos(app_id_);
  auto it = base::ranges::find_if(
      manifest_icons, [&icon_size_and_purpose](const apps::IconInfo& info) {
        return info.purpose == web_app::ManifestPurposeToIconInfoPurpose(
                                   icon_size_and_purpose->purpose);
      });

  if (it == manifest_icons.end()) {
    LOG(ERROR) << "Could not find URL for icon";
    DeliverResult(WebApkInstallStatus::kAppInvalid);
    return;
  }
  std::string icon_url = it->url.spec();

  webapk::WebAppManifest* web_app_manifest = webapk->mutable_manifest();
  PopulateWebApkManifest(profile_, app_id_, web_app_manifest);

  webapk::Image* image = web_app_manifest->add_icons();
  image->set_src(std::move(icon_url));
  image->add_purposes(icon_size_and_purpose->purpose ==
                              web_app::IconPurpose::MASKABLE
                          ? webapk::Image::MASKABLE
                          : webapk::Image::ANY);
  image->add_usages(webapk::Image::PRIMARY_ICON);

  icon_manager.ReadSmallestCompressedIcon(
      app_id_, {icon_size_and_purpose->purpose}, icon_size_and_purpose->size_px,
      base::BindOnce(&WebApkInstallTask::OnLoadedIcon,
                     weak_ptr_factory_.GetWeakPtr(), std::move(webapk)));
}

void WebApkInstallTask::OnLoadedIcon(std::unique_ptr<webapk::WebApk> webapk,
                                     web_app::IconPurpose purpose,
                                     std::vector<uint8_t> data) {
  app_short_name_ = webapk->manifest().short_name();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(AddIconDataAndSerializeProto, std::move(webapk),
                     std::move(data), std::move(web_apk_info_)),
      base::BindOnce(&WebApkInstallTask::OnProtoSerialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebApkInstallTask::OnProtoSerialized(
    std::optional<std::string> serialized_proto) {
  if (!serialized_proto && !serialized_proto.has_value()) {
    // We don't need to continue the update, because the existing WebAPK is up
    // to date.
    webapk_prefs::SetUpdateNeededForApp(profile_, app_id_,
                                        /* update_needed= */ false);
    DeliverResult(WebApkInstallStatus::kUpdateCancelledWebApkUpToDate);
    return;
  }
  GURL server_url = GetServerUrl();

  timer_.Start(FROM_HERE, minter_timeout_,
               base::BindOnce(&WebApkInstallTask::DeliverResult,
                              weak_ptr_factory_.GetWeakPtr(),
                              WebApkInstallStatus::kNetworkTimeout));

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
  url_loader_->AttachStringForUpload(std::move(serialized_proto.value()),
                                     kProtoMimeType);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory,
      base::BindOnce(&WebApkInstallTask::OnUrlLoaderComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebApkInstallTask::OnUrlLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  timer_.Stop();

  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  if (!response_body || response_code != net::HTTP_OK) {
    LOG(WARNING) << "WebAPK server returned response code " << response_code;
    DeliverResult(WebApkInstallStatus::kNetworkError);
    return;
  }

  auto response = std::make_unique<webapk::WebApkResponse>();
  if (!response->ParseFromString(*response_body)) {
    LOG(WARNING) << "Failed to parse WebApkResponse proto";
    DeliverResult(WebApkInstallStatus::kNetworkError);
    return;
  }

  VLOG(1) << "Installing WebAPK: " << response->package_name();

  auto* arc_service_manager = arc::ArcServiceManager::Get();
  DCHECK(arc_service_manager);
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->webapk(), InstallWebApk);

  if (!instance) {
    LOG(ERROR) << "WebApkInstance is not ready";
    DeliverResult(WebApkInstallStatus::kArcUnavailable);
    return;
  }

  int webapk_version;
  base::StringToInt(response->version(), &webapk_version);
  instance->InstallWebApk(
      response->package_name(), webapk_version, app_short_name_,
      response->token(),
      base::BindOnce(&WebApkInstallTask::OnInstallComplete,
                     weak_ptr_factory_.GetWeakPtr(), response->package_name()));
}

void WebApkInstallTask::OnInstallComplete(
    const std::string& package_name,
    arc::mojom::WebApkInstallResult result) {
  VLOG(1) << "WebAPK installation finished with result " << result;

  const bool success = result == arc::mojom::WebApkInstallResult::kSuccess;
  const bool is_update = package_name_to_update_.has_value();
  if (success) {
    if (is_update) {
      webapk_prefs::SetUpdateNeededForApp(profile_, app_id_,
                                          /* update_needed= */ false);
    } else {
      webapk_prefs::AddWebApk(profile_, app_id_, package_name);
    }
  }

  DeliverResult(success ? WebApkInstallStatus::kSuccess
                        : WebApkInstallStatus::kGooglePlayError);
}

void WebApkInstallTask::FetchWebApkInfoFromCrosapi() {
  crosapi::mojom::WebAppProviderBridge* web_app_provider_bridge =
      crosapi::CrosapiManager::Get()
          ->crosapi_ash()
          ->web_app_service_ash()
          ->GetWebAppProviderBridge();
  if (!web_app_provider_bridge) {
    // TODO(crbug.com/40199484): Consider adding an enum entry for failures
    // relating to Lacros.
    DeliverResult(WebApkInstallStatus::kAppInvalid);
    return;
  }

  web_app_provider_bridge->GetWebApkCreationParams(
      app_id_,
      base::BindOnce(&WebApkInstallTask::OnWebApkInfoFetchedFromCrosapi,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebApkInstallTask::OnWebApkInfoFetchedFromCrosapi(
    crosapi::mojom::WebApkCreationParamsPtr webapk_creation_params) {
  // TODO(crbug.com/40199484): Consider deserializing on another thread.

  std::unique_ptr<webapk::WebApk> webapk;
  if (webapk_creation_params &&
      !webapk_creation_params->webapk_manifest_proto_bytes.empty()) {
    webapk = std::make_unique<webapk::WebApk>();
    webapk->set_manifest_url(webapk_creation_params->manifest_url);
    if (!webapk->mutable_manifest()->ParseFromArray(
            webapk_creation_params->webapk_manifest_proto_bytes.data(),
            webapk_creation_params->webapk_manifest_proto_bytes.size())) {
      webapk.reset();
    }
  }
  if (!webapk) {
    // TODO(crbug.com/40199484): Consider adding an enum entry for failures
    // relating to Lacros.
    DeliverResult(WebApkInstallStatus::kAppInvalid);
    return;
  }

  webapk->set_requester_application_package(kRequesterPackageName);
  webapk->set_requester_application_version(
      std::string(version_info::GetVersionNumber()));
  LoadWebApkInfo(std::move(webapk));
}

void WebApkInstallTask::DeliverResult(WebApkInstallStatus result) {
  // Invalidate weak pointers so that in-flight tasks cannot attempt to deliver
  // a second result.
  weak_ptr_factory_.InvalidateWeakPtrs();

  RecordWebApkInstallResult(package_name_to_update_.has_value(), result);

  DCHECK(result_callback_);
  std::move(result_callback_).Run(result == WebApkInstallStatus::kSuccess);
}

}  // namespace apps

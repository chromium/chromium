// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/cros_display_config.mojom.h"
#include "base/base64url.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/common/api/system_display.h"
#include "gpu/config/gpu_info.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/gfx/extension_set.h"
#include "ui/gl/gl_version_info.h"

namespace chromeos {

namespace {

constexpr const char kGetAppListUrl[] =
    "https://android.clients.google.com/fdfe/chrome/getfastreinstallappslist";

constexpr int kResponseErrorNotEnoughApps = 5;

constexpr int kResponseErrorNotFirstTimeChromebookUser = 6;

constexpr base::TimeDelta kDownloadTimeOut = base::TimeDelta::FromMinutes(1);

constexpr const int64_t kMaxDownloadBytes = 1024 * 1024;  // 1Mb

constexpr const int kMaxAppCount = 21;

enum RecommendAppsResponseParseResult {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  RECOMMEND_APPS_RESPONSE_PARSE_RESULT_NO_ERROR = 0,
  RECOMMEND_APPS_RESPONSE_PARSE_RESULT_INVALID_JSON = 1,
  RECOMMEND_APPS_RESPONSE_PARSE_RESULT_NO_APP = 2,
  RECOMMEND_APPS_RESPONSE_PARSE_RESULT_OWNS_CHROMEBOOK_ALREADY = 3,
  RECOMMEND_APPS_RESPONSE_PARSE_RESULT_UNKNOWN_ERROR_CODE = 4,
  RECOMMEND_APPS_RESPONSE_PARSE_RESULT_INVALID_ERROR_CODE = 5,

  kMaxValue = RECOMMEND_APPS_RESPONSE_PARSE_RESULT_INVALID_ERROR_CODE
};

bool HasTouchScreen() {
  return !ui::InputDeviceManager::GetInstance()
              ->GetTouchscreenDevices()
              .empty();
}

bool HasStylusInput() {
  // Check to see if the hardware reports it is stylus capable.
  for (const ui::TouchscreenDevice& device :
       ui::InputDeviceManager::GetInstance()->GetTouchscreenDevices()) {
    if (device.has_stylus &&
        device.type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL) {
      return true;
    }
  }

  return false;
}

bool HasKeyboard() {
  return !ui::InputDeviceManager::GetInstance()->GetKeyboardDevices().empty();
}

bool HasHardKeyboard() {
  for (const ui::InputDevice& device :
       ui::InputDeviceManager::GetInstance()->GetKeyboardDevices()) {
    if (!device.phys.empty())
      return true;
  }

  return false;
}

gfx::Size GetScreenSize() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel();
}

// TODO(rsgingerrs): This function is copied from Play. We need to find a way to
// keep this synced with the Play side if there are any changes there. Another
// approach is to let the server do the calculation since we have provided the
// screen width, height and dpi.
int CalculateStableScreenLayout(const int screen_width,
                                const int screen_height,
                                const float dpi) {
  const int density_default = 160;
  const float px_to_dp = density_default / static_cast<float>(dpi);
  const int short_size_dp = static_cast<int>(screen_width * px_to_dp);
  const int long_size_dp = static_cast<int>(screen_height * px_to_dp);

  int screen_layout_size;
  bool screen_layout_long;

  const int screenlayout_size_small = 0x01;
  const int screenlayout_size_normal = 0x02;
  const int screenlayout_size_large = 0x03;
  const int screenlayout_size_xlarge = 0x04;
  const int screenlayout_long_no = 0x10;

  // These semi-magic numbers define our compatibility modes for
  // applications with different screens.  These are guarantees to
  // app developers about the space they can expect for a particular
  // configuration.  DO NOT CHANGE!
  if (long_size_dp < 470) {
    // This is shorter than an HVGA normal density screen (which
    // is 480 pixels on its long side).
    screen_layout_size = screenlayout_size_small;
    screen_layout_long = false;
  } else {
    // What size is this screen?
    if (long_size_dp >= 960 && short_size_dp >= 720) {
      // 1.5xVGA or larger screens at medium density are the point
      // at which we consider it to be an extra large screen.
      screen_layout_size = screenlayout_size_xlarge;
    } else if (long_size_dp >= 640 && short_size_dp >= 480) {
      // VGA or larger screens at medium density are the point
      // at which we consider it to be a large screen.
      screen_layout_size = screenlayout_size_large;
    } else {
      screen_layout_size = screenlayout_size_normal;
    }

    // Is this a long screen? Anything wider than WVGA (5:3) is considering to
    // be long.
    screen_layout_long = ((long_size_dp * 3) / 5) >= (short_size_dp - 1);
  }

  int screen_layout = screen_layout_size;
  if (!screen_layout_long) {
    screen_layout |= screenlayout_long_no;
  }

  return screen_layout;
}

device_configuration::DeviceConfigurationProto_ScreenLayout
GetScreenLayoutSizeId(const int screen_layout_size_value) {
  const int screenlayout_size_small = 0x01;
  const int screenlayout_size_normal = 0x02;
  const int screenlayout_size_large = 0x03;
  const int screenlayout_size_xlarge = 0x04;
  const int screenlayout_size_mask = 0x0f;
  int size_bits = screen_layout_size_value & screenlayout_size_mask;

  switch (size_bits) {
    case screenlayout_size_small:
      return device_configuration::DeviceConfigurationProto_ScreenLayout::
          DeviceConfigurationProto_ScreenLayout_SMALL;
    case screenlayout_size_normal:
      return device_configuration::DeviceConfigurationProto_ScreenLayout::
          DeviceConfigurationProto_ScreenLayout_NORMAL;
    case screenlayout_size_large:
      return device_configuration::DeviceConfigurationProto_ScreenLayout::
          DeviceConfigurationProto_ScreenLayout_LARGE;
    case screenlayout_size_xlarge:
      return device_configuration::DeviceConfigurationProto_ScreenLayout::
          DeviceConfigurationProto_ScreenLayout_EXTRA_LARGE;
    default:
      return device_configuration::DeviceConfigurationProto_ScreenLayout::
          DeviceConfigurationProto_ScreenLayout_UNDEFINED_SCREEN_LAYOUT;
  }
}

const gpu::GPUInfo GetGPUInfo() {
  return content::GpuDataManager::GetInstance()->GetGPUInfo();
}

// This function converts the major and minor versions to the proto accepted
// value. For example, if the version is 3.2, the return value is 0x00030002.
unsigned GetGLVersionInfo() {
  const gpu::GPUInfo gpu_info = GetGPUInfo();
  gfx::ExtensionSet extensionSet(gfx::MakeExtensionSet(gpu_info.gl_extensions));
  gl::GLVersionInfo glVersionInfo(gpu_info.gl_version.c_str(),
                                  gpu_info.gl_renderer.c_str(), extensionSet);

  unsigned major_version = glVersionInfo.major_version;
  unsigned minor_version = glVersionInfo.minor_version;
  unsigned version = 0x0000ffff;
  version &= minor_version;
  version |= (major_version << 16) & 0xffff0000;

  return version;
}

gfx::ExtensionSet GetGLExtensions() {
  const gpu::GPUInfo gpu_info = GetGPUInfo();
  gfx::ExtensionSet extensionSet(gfx::MakeExtensionSet(gpu_info.gl_extensions));

  return extensionSet;
}

const std::string& GetAndroidSdkVersion(const arc::ArcFeatures& arc_features) {
  return arc_features.build_props.at("ro.build.version.sdk");
}

std::vector<std::string> GetCpuAbiList(const arc::ArcFeatures& arc_features) {
  const std::string& abi_list_str =
      arc_features.build_props.at("ro.product.cpu.abilist");
  return base::SplitString(abi_list_str, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_ALL);
}

std::string CompressAndEncodeProtoMessageOnBlockingThread(
    device_configuration::DeviceConfigurationProto device_config) {
  std::string encoded_device_configuration_proto;

  std::string serialized_proto;
  device_config.SerializeToString(&serialized_proto);
  std::string compressed_proto;
  compression::GzipCompress(serialized_proto, &compressed_proto);
  base::Base64UrlEncode(compressed_proto,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_device_configuration_proto);

  return encoded_device_configuration_proto;
}

void RecordUmaResponseAppCount(int app_count) {
  UMA_HISTOGRAM_CUSTOM_COUNTS("OOBE.RecommendApps.Fetcher.AppCount", app_count,
                              0, kMaxAppCount, kMaxAppCount + 1);
}

void RecordUmaDownloadTime(base::TimeDelta download_time) {
  UMA_HISTOGRAM_TIMES("OOBE.RecommendApps.Fetcher.DownloadTime", download_time);
}

void RecordUmaResponseCode(int code) {
  base::UmaHistogramSparse("OOBE.RecommendApps.Fetcher.ResponseCode", code);
}

void RecordUmaResponseParseResult(RecommendAppsResponseParseResult result) {
  UMA_HISTOGRAM_ENUMERATION("OOBE.RecommendApps.Fetcher.ResponseParseResult",
                            result);
}

void RecordUmaResponseSize(unsigned long responseSize) {
  UMA_HISTOGRAM_COUNTS_1M(
      "OOBE.RecommendApps.Fetcher.ResponseSize",
      static_cast<base::HistogramBase::Sample>(responseSize));
}

}  // namespace

RecommendAppsFetcher::RecommendAppsFetcher(RecommendAppsScreenView* view)
    : view_(view), weak_ptr_factory_(this) {
  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  DCHECK(connector);
  connector->BindInterface(ash::mojom::kServiceName, &cros_display_config_);

  PopulateDeviceConfig();
  StartAshRequest();
  arc::ArcFeaturesParser::GetArcFeatures(
      base::BindOnce(&RecommendAppsFetcher::OnArcFeaturesRead,
                     weak_ptr_factory_.GetWeakPtr()));
}

RecommendAppsFetcher::~RecommendAppsFetcher() = default;

void RecommendAppsFetcher::PopulateDeviceConfig() {
  if (!HasTouchScreen()) {
    device_config_.set_touch_screen(
        device_configuration::DeviceConfigurationProto_TouchScreen::
            DeviceConfigurationProto_TouchScreen_NOTOUCH);
  } else if (!HasStylusInput()) {
    device_config_.set_touch_screen(
        device_configuration::DeviceConfigurationProto_TouchScreen::
            DeviceConfigurationProto_TouchScreen_FINGER);
  } else {
    device_config_.set_touch_screen(
        device_configuration::DeviceConfigurationProto_TouchScreen::
            DeviceConfigurationProto_TouchScreen_STYLUS);
  }

  if (!HasKeyboard()) {
    device_config_.set_keyboard(
        device_configuration::DeviceConfigurationProto_Keyboard::
            DeviceConfigurationProto_Keyboard_NOKEYS);
  } else {
    // TODO(rsgingerrs): Currently there is no straightforward way to determine
    // whether it is a full keyboard or not. We assume it is safe to set it as
    // QWERTY keyboard for this feature.
    device_config_.set_keyboard(
        device_configuration::DeviceConfigurationProto_Keyboard::
            DeviceConfigurationProto_Keyboard_QWERTY);
  }
  device_config_.set_has_hard_keyboard(HasHardKeyboard());

  // TODO(rsgingerrs): There is no straightforward way to get this info. We
  // assume it is safe to set it as no navigation.
  device_config_.set_navigation(
      device_configuration::DeviceConfigurationProto_Navigation::
          DeviceConfigurationProto_Navigation_NONAV);
  device_config_.set_has_five_way_navigation(false);

  device_config_.set_gl_es_version(GetGLVersionInfo());

  for (const base::StringPiece& gl_extension : GetGLExtensions()) {
    if (!gl_extension.empty())
      device_config_.add_gl_extension(gl_extension.as_string());
  }
}

void RecommendAppsFetcher::StartAshRequest() {
  cros_display_config_->GetDisplayUnitInfoList(
      false /* single_unified */,
      base::BindOnce(&RecommendAppsFetcher::OnAshResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RecommendAppsFetcher::MaybeStartCompressAndEncodeProtoMessage() {
  if (!ash_ready_ || !arc_features_ready_ || has_started_proto_processing_)
    return;

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&CompressAndEncodeProtoMessageOnBlockingThread,
                     std::move(device_config_)),
      base::BindOnce(&RecommendAppsFetcher::OnProtoMessageCompressedAndEncoded,
                     weak_ptr_factory_.GetWeakPtr()));
  has_started_proto_processing_ = true;
}

void RecommendAppsFetcher::OnProtoMessageCompressedAndEncoded(
    std::string encoded_device_configuration_proto) {
  proto_compressed_and_encoded_ = true;
  encoded_device_configuration_proto_ = encoded_device_configuration_proto;
  StartDownload();
}

void RecommendAppsFetcher::OnAshResponse(
    std::vector<ash::mojom::DisplayUnitInfoPtr> all_displays_info) {
  ash_ready_ = true;

  int screen_density = 0;
  for (const ash::mojom::DisplayUnitInfoPtr& display_info : all_displays_info) {
    if (base::NumberToString(display::Display::InternalDisplayId()) ==
        display_info->id) {
      screen_density = display_info->dpi_x + display_info->dpi_y;
      break;
    }
  }
  device_config_.set_screen_density(screen_density);

  const int screen_width = GetScreenSize().width();
  const int screen_height = GetScreenSize().height();
  device_config_.set_screen_width(screen_width);
  device_config_.set_screen_height(screen_height);

  const int screen_layout =
      CalculateStableScreenLayout(screen_width, screen_height, screen_density);
  device_config_.set_screen_layout(GetScreenLayoutSizeId(screen_layout));

  MaybeStartCompressAndEncodeProtoMessage();
}

void RecommendAppsFetcher::OnArcFeaturesRead(
    base::Optional<arc::ArcFeatures> read_result) {
  arc_features_ready_ = true;

  if (read_result != base::nullopt) {
    for (const auto& feature : read_result.value().feature_map) {
      device_config_.add_system_available_feature(feature.first);
    }

    for (const auto& abi : GetCpuAbiList(read_result.value())) {
      device_config_.add_native_platform(abi);
    }

    play_store_version_ = read_result.value().play_store_version;

    android_sdk_version_ = GetAndroidSdkVersion(read_result.value());
  }

  MaybeStartCompressAndEncodeProtoMessage();
}

void RecommendAppsFetcher::StartDownload() {
  if (!proto_compressed_and_encoded_)
    return;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("play_recommended_apps_download", R"(
        semantics {
          sender: "ChromeOS Recommended Apps Screen"
          description:
            "Chrome OS downloads the recommended app list from Google Play API."
          trigger:
            "When user has accepted the ARC Terms of Service."
          data:
            "URL of the Google Play API."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookie_store: "user"
          setting:
            "NA"
          policy_exception_justification:
            "Not implemented, considered not necessary."
        })");

  Profile* profile = ProfileManager::GetActiveUserProfile();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kGetAppListUrl);
  resource_request->method = "GET";
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;

  resource_request->headers.SetHeader("X-DFE-Device-Config",
                                      encoded_device_configuration_proto_);
  resource_request->headers.SetHeader("X-DFE-Sdk-Version",
                                      android_sdk_version_);
  resource_request->headers.SetHeader("X-DFE-Chromesky-Client-Version",
                                      play_store_version_);

  network::mojom::URLLoaderFactory* loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess()
          .get();

  start_time_ = base::TimeTicks::Now();
  app_list_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  // Retry up to three times if network changes are detected during the
  // download.
  app_list_loader_->SetRetryOptions(
      3, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  app_list_loader_->DownloadToString(
      loader_factory,
      base::BindOnce(&RecommendAppsFetcher::OnDownloaded,
                     base::Unretained(this)),
      kMaxDownloadBytes);

  // Abort the download attempt if it takes longer than one minute.
  download_timer_.Start(FROM_HERE, kDownloadTimeOut, this,
                        &RecommendAppsFetcher::OnDownloadTimeout);
}

void RecommendAppsFetcher::OnDownloadTimeout() {
  // Destroy the fetcher, which will abort the download attempt.
  app_list_loader_.reset();

  RecordUmaDownloadTime(base::TimeTicks::Now() - start_time_);

  // Show an error message to the user.
  if (view_)
    view_->OnLoadError();
}

void RecommendAppsFetcher::OnDownloaded(
    std::unique_ptr<std::string> response_body) {
  download_timer_.Stop();

  RecordUmaDownloadTime(base::TimeTicks::Now() - start_time_);

  std::unique_ptr<network::SimpleURLLoader> loader(std::move(app_list_loader_));
  if (!view_)
    return;

  int response_code = 0;
  if (!loader->ResponseInfo() || !loader->ResponseInfo()->headers) {
    view_->OnLoadError();
    return;
  }
  response_code = loader->ResponseInfo()->headers->response_code();
  RecordUmaResponseCode(response_code);

  // If the recommended app list could not be downloaded, show an error message
  // to the user.
  if (!response_body || response_body->empty()) {
    view_->OnLoadError();
    return;
  }

  // If the recommended app list were downloaded successfully, show them to
  // the user.
  //
  // The response starts with a prefix ")]}'". This needs to be removed before
  // further parsing.
  RecordUmaResponseSize(response_body->size());
  constexpr base::StringPiece json_xss_prevention_prefix(")]}'");
  base::StringPiece response_body_json(*response_body);
  if (response_body_json.starts_with(json_xss_prevention_prefix))
    response_body_json.remove_prefix(json_xss_prevention_prefix.length());
  base::Optional<base::Value> output = ParseResponse(response_body_json);
  if (!output.has_value()) {
    RecordUmaResponseAppCount(0);
    view_->OnParseResponseError();
    return;
  }

  view_->OnLoadSuccess(std::move(output.value()));
}

void RecommendAppsFetcher::Retry() {
  StartDownload();
}

base::Optional<base::Value> RecommendAppsFetcher::ParseResponse(
    base::StringPiece response) {
  base::Value output(base::Value::Type::LIST);

  int error_code;
  std::string error_msg;
  std::unique_ptr<base::Value> json_value =
      base::JSONReader::ReadAndReturnError(response, base::JSON_PARSE_RFC,
                                           &error_code, &error_msg);

  if (!json_value || (!json_value->is_list() && !json_value->is_dict())) {
    LOG(ERROR) << "Error parsing response JSON: " << error_msg;
    RecordUmaResponseParseResult(
        RECOMMEND_APPS_RESPONSE_PARSE_RESULT_INVALID_JSON);
    return base::nullopt;
  }

  // If the response is a dictionary, it is an error message in the
  // following format:
  //   {"Error code":"error code","Error message":"Error message"}
  if (json_value->is_dict()) {
    const base::Value* response_error_code_value =
        json_value->FindKeyOfType("Error code", base::Value::Type::STRING);

    if (!response_error_code_value) {
      LOG(ERROR) << "Unable to find error code: response="
                 << response.substr(0, 128);
      RecordUmaResponseParseResult(
          RECOMMEND_APPS_RESPONSE_PARSE_RESULT_INVALID_JSON);
      return base::nullopt;
    }

    base::StringPiece response_error_code_str =
        response_error_code_value->GetString();
    int response_error_code = 0;
    if (!base::StringToInt(response_error_code_str, &response_error_code)) {
      LOG(WARNING) << "Unable to parse error code: " << response_error_code_str;
      RecordUmaResponseParseResult(
          RECOMMEND_APPS_RESPONSE_PARSE_RESULT_INVALID_ERROR_CODE);
      return base::nullopt;
    }

    if (response_error_code == kResponseErrorNotFirstTimeChromebookUser) {
      RecordUmaResponseParseResult(
          RECOMMEND_APPS_RESPONSE_PARSE_RESULT_OWNS_CHROMEBOOK_ALREADY);
    } else if (response_error_code == kResponseErrorNotEnoughApps) {
      RecordUmaResponseParseResult(RECOMMEND_APPS_RESPONSE_PARSE_RESULT_NO_APP);
    } else {
      LOG(WARNING) << "Unknown error code: " << response_error_code_str;
      RecordUmaResponseParseResult(
          RECOMMEND_APPS_RESPONSE_PARSE_RESULT_UNKNOWN_ERROR_CODE);
    }

    return base::nullopt;
  }

  // Otherwise, the response should return a list of apps.
  const base::Value::ListStorage& app_list = json_value->GetList();
  if (app_list.empty()) {
    DVLOG(1) << "No app in the response.";
    RecordUmaResponseParseResult(RECOMMEND_APPS_RESPONSE_PARSE_RESULT_NO_APP);
    return base::nullopt;
  }

  for (auto& item : app_list) {
    base::Value output_map(base::Value::Type::DICTIONARY);

    if (!item.is_dict()) {
      DVLOG(1) << "Cannot parse item.";
      continue;
    }

    // Retrieve the app title.
    const base::Value* title =
        item.FindPathOfType({"title_", "name_"}, base::Value::Type::STRING);
    if (title)
      output_map.SetKey("name", base::Value(title->GetString()));

    // Retrieve the package name.
    const base::Value* package_name =
        item.FindPathOfType({"id_", "id_"}, base::Value::Type::STRING);
    if (package_name)
      output_map.SetKey("package_name", base::Value(package_name->GetString()));

    // Retrieve the icon URL for the app.
    //
    // The name "privateDoNotAccessOrElseSafeUrlWrappedValue_" here is because
    // it is a direct serialization from the proto message. The value has been
    // sanitized so it is regarded as a safe URL. In general, if the response is
    // a protobuf, we should not directly access this field but use the wrapper
    // method getSafeUrlString() to read it. In our case, we don't have the
    // option other than access it directly.
    const base::Value* icon_url = item.FindPathOfType(
        {"icon_", "url_", "privateDoNotAccessOrElseSafeUrlWrappedValue_"},
        base::Value::Type::STRING);
    if (icon_url)
      output_map.SetKey("icon", base::Value(icon_url->GetString()));

    output.GetList().push_back(std::move(output_map));
  }

  RecordUmaResponseParseResult(RECOMMEND_APPS_RESPONSE_PARSE_RESULT_NO_ERROR);
  RecordUmaResponseAppCount(static_cast<int>(output.GetList().size()));

  return output;
}

}  // namespace chromeos

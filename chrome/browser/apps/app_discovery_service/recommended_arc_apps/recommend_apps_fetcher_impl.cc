// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/recommended_arc_apps/recommend_apps_fetcher_impl.h"

#include <string_view>

#include "base/base64url.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/apps/app_discovery_service/recommended_arc_apps/recommend_apps_fetcher_delegate.h"
#include "content/public/browser/gpu_data_manager.h"
#include "extensions/common/api/system_display.h"
#include "gpu/config/gpu_info.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/gfx/extension_set.h"
#include "ui/gl/gl_version_info.h"

namespace apps {
namespace {

constexpr const char kGetRevisedAppListUrl[] =
    "https://android.clients.google.com/fdfe/chrome/getSetupAppRecommendations";

constexpr base::TimeDelta kDownloadTimeOut = base::Minutes(1);

constexpr const int64_t kMaxDownloadBytes = 1024 * 1024;  // 1Mb

// Fake gpu info for test.
const gpu::GPUInfo* g_gpu_info_for_test = nullptr;

bool HasTouchScreen() {
  return !ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices().empty();
}

bool HasStylusInput() {
  // Check to see if the hardware reports it is stylus capable.
  for (const ui::TouchscreenDevice& device :
       ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices()) {
    if (device.has_stylus &&
        device.type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL) {
      return true;
    }
  }

  return false;
}

bool HasKeyboard() {
  return !ui::DeviceDataManager::GetInstance()->GetKeyboardDevices().empty();
}

bool HasHardKeyboard() {
  for (const ui::InputDevice& device :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (!device.phys.empty()) {
      return true;
    }
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
  const int screen_width_dp = static_cast<int>(screen_width * px_to_dp);
  const int screen_height_dp = static_cast<int>(screen_height * px_to_dp);
  const int short_size_dp = std::min(screen_width_dp, screen_height_dp);
  const int long_size_dp = std::max(screen_width_dp, screen_height_dp);

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
  if (g_gpu_info_for_test) {
    return *g_gpu_info_for_test;
  }

  return content::GpuDataManager::GetInstance()->GetGPUInfo();
}

// This function converts the major and minor versions to the proto accepted
// value. For example, if the version is 3.2, the return value is 0x00030002.
unsigned GetGLVersionInfo(const gpu::GPUInfo& gpu_info) {
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

gfx::ExtensionSet GetGLExtensions(const gpu::GPUInfo& gpu_info) {
  gfx::ExtensionSet extensionSet(gfx::MakeExtensionSet(gpu_info.gl_extensions));

  return extensionSet;
}

const std::string& GetDeviceFingerprint(const arc::ArcFeatures& arc_features) {
  return arc_features.build_props.fingerprint;
}

const std::string& GetAndroidSdkVersion(const arc::ArcFeatures& arc_features) {
  return arc_features.build_props.sdk_version;
}

std::vector<std::string> GetCpuAbiList(const arc::ArcFeatures& arc_features) {
  // The property value will be a comma separated list, e.g. "x86_64,x86".
  return base::SplitString(arc_features.build_props.abi_list, ",",
                           base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
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

void RecordUmaDownloadTime(base::TimeDelta download_time) {
  UMA_HISTOGRAM_TIMES("OOBE.RecommendApps.Fetcher.DownloadTime", download_time);
}

void RecordUmaResponseCode(int code) {
  base::UmaHistogramSparse("OOBE.RecommendApps.Fetcher.ResponseCode", code);
}

}  // namespace

RecommendAppsFetcherImpl::ScopedGpuInfoForTest::ScopedGpuInfoForTest(
    const gpu::GPUInfo* gpu_info) {
  DCHECK(!g_gpu_info_for_test);
  g_gpu_info_for_test = gpu_info;
}

RecommendAppsFetcherImpl::ScopedGpuInfoForTest::~ScopedGpuInfoForTest() {
  g_gpu_info_for_test = nullptr;
}

RecommendAppsFetcherImpl::RecommendAppsFetcherImpl(
    RecommendAppsFetcherDelegate* delegate,
    mojo::PendingRemote<crosapi::mojom::CrosDisplayConfigController>
        display_config,
    network::mojom::URLLoaderFactory* url_loader_factory)
    : delegate_(delegate),
      url_loader_factory_(url_loader_factory),
      arc_features_getter_(
          base::BindRepeating(&arc::ArcFeaturesParser::GetArcFeatures)),
      cros_display_config_(std::move(display_config)) {}

RecommendAppsFetcherImpl::~RecommendAppsFetcherImpl() = default;

void RecommendAppsFetcherImpl::PopulateDeviceConfig() {
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

  const gpu::GPUInfo gpu_info = GetGPUInfo();
  device_config_.set_gl_es_version(GetGLVersionInfo(gpu_info));

  for (std::string_view gl_extension : GetGLExtensions(gpu_info)) {
    if (!gl_extension.empty()) {
      device_config_.add_gl_extension(std::string(gl_extension));
    }
  }
}

void RecommendAppsFetcherImpl::StartAshRequest() {
  cros_display_config_->GetDisplayUnitInfoList(
      false /* single_unified */,
      base::BindOnce(&RecommendAppsFetcherImpl::OnAshResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RecommendAppsFetcherImpl::MaybeStartCompressAndEncodeProtoMessage() {
  if (!ash_ready_ || !arc_features_ready_ || has_started_proto_processing_) {
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&CompressAndEncodeProtoMessageOnBlockingThread,
                     std::move(device_config_)),
      base::BindOnce(
          &RecommendAppsFetcherImpl::OnProtoMessageCompressedAndEncoded,
          weak_ptr_factory_.GetWeakPtr()));
  has_started_proto_processing_ = true;
}

void RecommendAppsFetcherImpl::OnProtoMessageCompressedAndEncoded(
    std::string encoded_device_configuration_proto) {
  proto_compressed_and_encoded_ = true;
  encoded_device_configuration_proto_ = encoded_device_configuration_proto;
  StartDownload();
}

void RecommendAppsFetcherImpl::OnAshResponse(
    std::vector<crosapi::mojom::DisplayUnitInfoPtr> all_displays_info) {
  ash_ready_ = true;

  int screen_density = 0;
  for (const crosapi::mojom::DisplayUnitInfoPtr& display_info :
       all_displays_info) {
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

void RecommendAppsFetcherImpl::OnArcFeaturesRead(
    std::optional<arc::ArcFeatures> read_result) {
  arc_features_ready_ = true;

  if (read_result != std::nullopt) {
    for (const auto& feature : read_result.value().feature_map) {
      device_config_.add_system_available_feature(feature.first);
    }

    for (const auto& abi : GetCpuAbiList(read_result.value())) {
      device_config_.add_native_platform(abi);
    }

    play_store_version_ = read_result.value().play_store_version;

    android_sdk_version_ = GetAndroidSdkVersion(read_result.value());

    device_fingerprint_ = GetDeviceFingerprint(read_result.value());
  }

  MaybeStartCompressAndEncodeProtoMessage();
}

void RecommendAppsFetcherImpl::StartDownload() {
  if (!proto_compressed_and_encoded_) {
    return;
  }

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
          setting:
            "NA"
          policy_exception_justification:
            "Not implemented, considered not necessary."
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kGetRevisedAppListUrl);
  resource_request->headers.SetHeader("X-DFE-Device-Fingerprint",
                                      device_fingerprint_);
  resource_request->method = "GET";
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;

  resource_request->headers.SetHeader("X-DFE-Device-Config",
                                      encoded_device_configuration_proto_);
  resource_request->headers.SetHeader("X-DFE-Sdk-Version",
                                      android_sdk_version_);
  resource_request->headers.SetHeader("X-DFE-Chromesky-Client-Version",
                                      play_store_version_);

  start_time_ = base::TimeTicks::Now();
  app_list_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  // Retry up to three times if network changes are detected during the
  // download.
  app_list_loader_->SetRetryOptions(
      3, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  app_list_loader_->DownloadToString(
      url_loader_factory_,
      base::BindOnce(&RecommendAppsFetcherImpl::OnDownloaded,
                     base::Unretained(this)),
      kMaxDownloadBytes);

  // Abort the download attempt if it takes longer than one minute.
  download_timer_.Start(FROM_HERE, kDownloadTimeOut, this,
                        &RecommendAppsFetcherImpl::OnDownloadTimeout);
}

void RecommendAppsFetcherImpl::OnDownloadTimeout() {
  // Destroy the fetcher, which will abort the download attempt.
  app_list_loader_.reset();

  RecordUmaDownloadTime(base::TimeTicks::Now() - start_time_);

  delegate_->OnLoadError();
}

void RecommendAppsFetcherImpl::OnDownloaded(
    std::unique_ptr<std::string> response_body) {
  download_timer_.Stop();

  RecordUmaDownloadTime(base::TimeTicks::Now() - start_time_);

  std::unique_ptr<network::SimpleURLLoader> loader(std::move(app_list_loader_));
  int response_code = 0;
  if (!loader->ResponseInfo() || !loader->ResponseInfo()->headers) {
    delegate_->OnLoadError();
    return;
  }
  response_code = loader->ResponseInfo()->headers->response_code();
  RecordUmaResponseCode(response_code);

  // If the recommended app list could not be downloaded, show an error message
  // to the user.
  if (!response_body || response_body->empty()) {
    delegate_->OnLoadError();
    return;
  }

  // If the recommended app list were downloaded successfully, show them to
  // the user.
  //
  // The response starts with a prefix ")]}'". This needs to be removed before
  // further parsing.
  const std::string json_xss_prevention_prefix = ")]}'";
  std::string response_body_json = *response_body;
  if (base::StartsWith(response_body_json, json_xss_prevention_prefix)) {
    response_body_json =
        response_body_json.substr(json_xss_prevention_prefix.length());
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      response_body_json,
      base::BindOnce(&RecommendAppsFetcherImpl::OnJsonParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RecommendAppsFetcherImpl::Start() {
  PopulateDeviceConfig();
  StartAshRequest();
  arc_features_getter_.Run(
      base::BindOnce(&RecommendAppsFetcherImpl::OnArcFeaturesRead,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RecommendAppsFetcherImpl::Retry() {
  StartDownload();
}

void RecommendAppsFetcherImpl::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    delegate_->OnParseResponseError();
    return;
  }
  delegate_->OnLoadSuccess(std::move(*result));
}

}  // namespace apps

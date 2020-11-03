// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/backdrop/ambient_backend_controller_impl.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/ambient_metrics.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/guid.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chromeos/assistant/internal/ambient/backdrop_client_config.h"
#include "chromeos/assistant/internal/proto/google3/backdrop/backdrop.pb.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

namespace ash {

namespace {

using BackdropClientConfig = chromeos::ambient::BackdropClientConfig;

constexpr char kProtoMimeType[] = "application/protobuf";

// Max body size in bytes to download.
constexpr int kMaxBodySizeBytes = 1 * 1024 * 1024;  // 1 MiB

std::string GetClientId() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  DCHECK(prefs);

  std::string client_id =
      prefs->GetString(ambient::prefs::kAmbientBackdropClientId);
  if (client_id.empty()) {
    client_id = base::GenerateGUID();
    prefs->SetString(ambient::prefs::kAmbientBackdropClientId, client_id);
  }

  return client_id;
}

std::unique_ptr<network::ResourceRequest> CreateResourceRequest(
    const BackdropClientConfig::Request& request) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(request.url);
  resource_request->method = request.method;
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  for (const auto& header : request.headers) {
    std::string encoded_value;
    if (header.needs_base_64_encoded)
      base::Base64Encode(header.value, &encoded_value);
    else
      encoded_value = header.value;

    resource_request->headers.SetHeader(header.name, encoded_value);
  }

  return resource_request;
}

std::string BuildBackdropTopicDetails(
    const backdrop::ScreenUpdate::Topic& backdrop_topic) {
  std::string result;
  if (backdrop_topic.has_metadata_line_1())
    result += backdrop_topic.metadata_line_1();

  if (backdrop_topic.has_metadata_line_2()) {
    if (!result.empty())
      result += " ";
    result += backdrop_topic.metadata_line_2();
  }
  // Do not include metadata_line_3.
  return result;
}

AmbientModeTopicType ToAmbientModeTopicType(
    const backdrop::ScreenUpdate_Topic& topic) {
  if (!topic.has_topic_type())
    return AmbientModeTopicType::kOther;

  switch (topic.topic_type()) {
    case backdrop::CURATED:
      return AmbientModeTopicType::kCurated;
    case backdrop::PERSONAL_PHOTO:
      return AmbientModeTopicType::kPersonal;
    case backdrop::FEATURED_PHOTO:
      return AmbientModeTopicType::kFeatured;
    case backdrop::GEO_PHOTO:
      return AmbientModeTopicType::kGeo;
    case backdrop::CULTURAL_INSTITUTE:
      return AmbientModeTopicType::kCulturalInstitute;
    case backdrop::RSS_TOPIC:
      return AmbientModeTopicType::kRss;
    case backdrop::CAPTURED_ON_PIXEL:
      return AmbientModeTopicType::kCapturedOnPixel;
    default:
      return AmbientModeTopicType::kOther;
  }
}

base::Optional<std::string> GetStringValue(base::Value::ConstListView values,
                                           size_t field_number) {
  if (values.empty() || values.size() < field_number)
    return base::nullopt;

  const base::Value& v = values[field_number - 1];
  if (!v.is_string())
    return base::nullopt;

  return v.GetString();
}

base::Optional<double> GetDoubleValue(base::Value::ConstListView values,
                                      size_t field_number) {
  if (values.empty() || values.size() < field_number)
    return base::nullopt;

  const base::Value& v = values[field_number - 1];
  if (!v.is_double() && !v.is_int())
    return base::nullopt;

  return v.GetDouble();
}

base::Optional<bool> GetBoolValue(base::Value::ConstListView values,
                                  size_t field_number) {
  if (values.empty() || values.size() < field_number)
    return base::nullopt;

  const base::Value& v = values[field_number - 1];
  if (v.is_bool())
    return v.GetBool();

  if (v.is_int())
    return v.GetInt() > 0;

  return base::nullopt;
}

base::Optional<WeatherInfo> ToWeatherInfo(const base::Value& result) {
  DCHECK(result.is_list());
  if (!result.is_list())
    return base::nullopt;

  WeatherInfo weather_info;
  const auto& list_result = result.GetList();

  weather_info.condition_icon_url = GetStringValue(
      list_result, backdrop::WeatherInfo::kConditionIconUrlFieldNumber);
  weather_info.temp_f =
      GetDoubleValue(list_result, backdrop::WeatherInfo::kTempFFieldNumber);
  weather_info.show_celsius =
      GetBoolValue(list_result, backdrop::WeatherInfo::kShowCelsiusFieldNumber)
          .value_or(false);

  return weather_info;
}

// Helper function to save the information we got from the backdrop server to a
// public struct so that they can be accessed by public codes.
ScreenUpdate ToScreenUpdate(
    const backdrop::ScreenUpdate& backdrop_screen_update) {
  ScreenUpdate screen_update;
  // Parse |AmbientModeTopic|.
  int topics_size = backdrop_screen_update.next_topics_size();
  if (topics_size > 0) {
    for (auto& backdrop_topic : backdrop_screen_update.next_topics()) {
      DCHECK(backdrop_topic.has_url());

      auto topic_type = ToAmbientModeTopicType(backdrop_topic);
      if (!ambient::util::IsAmbientModeTopicTypeAllowed(topic_type))
        continue;

      AmbientModeTopic ambient_topic;
      ambient_topic.topic_type = topic_type;
      if (backdrop_topic.has_portrait_image_url())
        ambient_topic.url = backdrop_topic.portrait_image_url();
      else
        ambient_topic.url = backdrop_topic.url();

      if (backdrop_topic.has_related_topic()) {
        if (backdrop_topic.related_topic().has_portrait_image_url()) {
          ambient_topic.related_image_url =
              backdrop_topic.related_topic().portrait_image_url();
        } else {
          ambient_topic.related_image_url =
              backdrop_topic.related_topic().url();
        }
      }
      ambient_topic.details = BuildBackdropTopicDetails(backdrop_topic);
      screen_update.next_topics.emplace_back(ambient_topic);
    }
  }

  // Parse |WeatherInfo|.
  if (backdrop_screen_update.has_weather_info()) {
    backdrop::WeatherInfo backdrop_weather_info =
        backdrop_screen_update.weather_info();
    WeatherInfo weather_info;
    if (backdrop_weather_info.has_condition_icon_url()) {
      weather_info.condition_icon_url =
          backdrop_weather_info.condition_icon_url();
    }

    if (backdrop_weather_info.has_temp_f())
      weather_info.temp_f = backdrop_weather_info.temp_f();

    if (backdrop_weather_info.has_show_celsius())
      weather_info.show_celsius = backdrop_weather_info.show_celsius();

    screen_update.weather_info = weather_info;
  }
  return screen_update;
}

bool IsArtSettingVisible(const ArtSetting& art_setting) {
  const auto& album_id = art_setting.album_id;

  if (album_id == kAmbientModeStreetArtAlbumId)
    return chromeos::features::kAmbientModeStreetArtAlbumEnabled.Get();

  if (album_id == kAmbientModeCapturedOnPixelAlbumId)
    return chromeos::features::kAmbientModeCapturedOnPixelAlbumEnabled.Get();

  if (album_id == kAmbientModeEarthAndSpaceAlbumId)
    return chromeos::features::kAmbientModeEarthAndSpaceAlbumEnabled.Get();

  if (album_id == kAmbientModeFeaturedPhotoAlbumId)
    return chromeos::features::kAmbientModeFeaturedPhotoAlbumEnabled.Get();

  if (album_id == kAmbientModeFineArtAlbumId)
    return chromeos::features::kAmbientModeFineArtAlbumEnabled.Get();

  return false;
}

gfx::Size GetDisplaySizeInPixel() {
  auto* ambient_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AmbientModeContainer);
  return display::Screen::GetScreen()
      ->GetDisplayNearestView(ambient_container)
      .GetSizeInPixel();
}

}  // namespace

// Helper class for handling Backdrop service requests.
class BackdropURLLoader {
 public:
  BackdropURLLoader() = default;
  BackdropURLLoader(const BackdropURLLoader&) = delete;
  BackdropURLLoader& operator=(const BackdropURLLoader&) = delete;
  ~BackdropURLLoader() = default;

  // Starts downloading the proto. |request_body| is a serialized proto and
  // will be used as the upload body if it is a POST request.
  void Start(std::unique_ptr<network::ResourceRequest> resource_request,
             const base::Optional<std::string>& request_body,
             const net::NetworkTrafficAnnotationTag& traffic_annotation,
             network::SimpleURLLoader::BodyAsStringCallback callback) {
    // No ongoing downloading task.
    DCHECK(!simple_loader_);

    loader_factory_ = AmbientClient::Get()->GetURLLoaderFactory();
    simple_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation);
    if (request_body)
      simple_loader_->AttachStringForUpload(*request_body, kProtoMimeType);

    // |base::Unretained| is safe because this instance outlives
    // |simple_loader_|.
    simple_loader_->DownloadToString(
        loader_factory_.get(),
        base::BindOnce(&BackdropURLLoader::OnUrlDownloaded,
                       base::Unretained(this), std::move(callback)),
        kMaxBodySizeBytes);
  }

 private:
  // Called when the download completes.
  void OnUrlDownloaded(network::SimpleURLLoader::BodyAsStringCallback callback,
                       std::unique_ptr<std::string> response_body) {
    loader_factory_.reset();

    if (simple_loader_->NetError() == net::OK && response_body) {
      simple_loader_.reset();
      std::move(callback).Run(std::move(response_body));
      return;
    }

    int response_code = -1;
    if (simple_loader_->ResponseInfo() &&
        simple_loader_->ResponseInfo()->headers) {
      response_code = simple_loader_->ResponseInfo()->headers->response_code();
    }

    LOG(ERROR) << "Downloading Backdrop proto failed with error code: "
               << response_code << " with network error"
               << simple_loader_->NetError();
    simple_loader_.reset();
    std::move(callback).Run(std::make_unique<std::string>());
    return;
  }

  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;
};

AmbientBackendControllerImpl::AmbientBackendControllerImpl()
    : backdrop_client_config_(ash::AmbientClient::Get()->ShouldUseProdServer()
                                  ? BackdropClientConfig::ServerType::kProd
                                  : BackdropClientConfig::ServerType::kDev) {}

AmbientBackendControllerImpl::~AmbientBackendControllerImpl() = default;

void AmbientBackendControllerImpl::FetchScreenUpdateInfo(
    int num_topics,
    OnScreenUpdateInfoFetchedCallback callback) {
  Shell::Get()->ambient_controller()->RequestAccessToken(base::BindOnce(
      &AmbientBackendControllerImpl::FetchScreenUpdateInfoInternal,
      weak_factory_.GetWeakPtr(), num_topics, std::move(callback)));
}

void AmbientBackendControllerImpl::GetSettings(GetSettingsCallback callback) {
  Shell::Get()->ambient_controller()->RequestAccessToken(
      base::BindOnce(&AmbientBackendControllerImpl::StartToGetSettings,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AmbientBackendControllerImpl::UpdateSettings(
    const AmbientSettings& settings,
    UpdateSettingsCallback callback) {
  Shell::Get()->ambient_controller()->RequestAccessToken(base::BindOnce(
      &AmbientBackendControllerImpl::StartToUpdateSettings,
      weak_factory_.GetWeakPtr(), settings, std::move(callback)));
}

void AmbientBackendControllerImpl::FetchSettingPreview(
    int preview_width,
    int preview_height,
    OnSettingPreviewFetchedCallback callback) {
  Shell::Get()->ambient_controller()->RequestAccessToken(
      base::BindOnce(&AmbientBackendControllerImpl::FetchSettingPreviewInternal,
                     weak_factory_.GetWeakPtr(), preview_width, preview_height,
                     std::move(callback)));
}

void AmbientBackendControllerImpl::FetchPersonalAlbums(
    int banner_width,
    int banner_height,
    int num_albums,
    const std::string& resume_token,
    OnPersonalAlbumsFetchedCallback callback) {
  Shell::Get()->ambient_controller()->RequestAccessToken(
      base::BindOnce(&AmbientBackendControllerImpl::FetchPersonalAlbumsInternal,
                     weak_factory_.GetWeakPtr(), banner_width, banner_height,
                     num_albums, resume_token, std::move(callback)));
}

void AmbientBackendControllerImpl::SetPhotoRefreshInterval(
    base::TimeDelta interval) {
  Shell::Get()
      ->ambient_controller()
      ->GetAmbientBackendModel()
      ->SetPhotoRefreshInterval(interval);
}

void AmbientBackendControllerImpl::FetchWeather(FetchWeatherCallback callback) {
  auto response_handler =
      [](FetchWeatherCallback callback,
         std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
         std::unique_ptr<std::string> response) {
        constexpr char kJsonPrefix[] = ")]}'\n";

        if (response && response->length() > strlen(kJsonPrefix)) {
          auto json_handler =
              [](FetchWeatherCallback callback,
                 data_decoder::DataDecoder::ValueOrError result) {
                if (result.value) {
                  std::move(callback).Run(ToWeatherInfo(result.value.value()));
                } else {
                  DVLOG(1) << "Failed to parse weather json.";
                  std::move(callback).Run(base::nullopt);
                }
              };

          data_decoder::DataDecoder::ParseJsonIsolated(
              response->substr(strlen(kJsonPrefix)),
              base::BindOnce(json_handler, std::move(callback)));
        } else {
          std::move(callback).Run(base::nullopt);
        }
      };

  const auto* user = user_manager::UserManager::Get()->GetActiveUser();
  DCHECK(user->HasGaiaAccount());
  BackdropClientConfig::Request request =
      backdrop_client_config_.CreateFetchWeatherInfoRequest(
          user->GetAccountId().GetGaiaId(), GetClientId());
  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateResourceRequest(request);
  auto backdrop_url_loader = std::make_unique<BackdropURLLoader>();
  auto* loader_ptr = backdrop_url_loader.get();
  loader_ptr->Start(std::move(resource_request), /*request_body=*/base::nullopt,
                    NO_TRAFFIC_ANNOTATION_YET,
                    base::BindOnce(response_handler, std::move(callback),
                                   std::move(backdrop_url_loader)));
}

const std::array<const char*, 2>&
AmbientBackendControllerImpl::GetBackupPhotoUrls() const {
  return chromeos::ambient::kBackupPhotoUrls;
}

void AmbientBackendControllerImpl::FetchScreenUpdateInfoInternal(
    int num_topics,
    OnScreenUpdateInfoFetchedCallback callback,
    const std::string& gaia_id,
    const std::string& access_token) {
  if (gaia_id.empty() || access_token.empty()) {
    LOG(ERROR) << "Failed to fetch access token";
    // Returns an empty instance to indicate the failure.
    std::move(callback).Run(ash::ScreenUpdate());
    return;
  }

  std::string client_id = GetClientId();
  BackdropClientConfig::Request request =
      backdrop_client_config_.CreateFetchScreenUpdateRequest(
          num_topics, gaia_id, access_token, client_id);
  auto resource_request = CreateResourceRequest(request);

  // For portrait photos, the server returns image of half requested width.
  // When the device is in portrait mode, where only shows one portrait photo,
  // it will cause unnecessary scaling. To reduce this effect, always requesting
  // the landscape display size.
  // TODO(b/172075868): Support tiling in portrait mode.
  gfx::Size display_size_px = GetDisplaySizeInPixel();
  const int width = std::max(display_size_px.width(), display_size_px.height());
  const int height =
      std::min(display_size_px.width(), display_size_px.height());
  resource_request->url =
      net::AppendQueryParameter(resource_request->url, "device-screen-width",
                                base::NumberToString(width));
  resource_request->url =
      net::AppendQueryParameter(resource_request->url, "device-screen-height",
                                base::NumberToString(height));

  auto backdrop_url_loader = std::make_unique<BackdropURLLoader>();
  auto* loader_ptr = backdrop_url_loader.get();
  loader_ptr->Start(
      std::move(resource_request), request.body, NO_TRAFFIC_ANNOTATION_YET,
      base::BindOnce(&AmbientBackendControllerImpl::OnScreenUpdateInfoFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(backdrop_url_loader)));
}

void AmbientBackendControllerImpl::OnScreenUpdateInfoFetched(
    OnScreenUpdateInfoFetchedCallback callback,
    std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
    std::unique_ptr<std::string> response) {
  DCHECK(backdrop_url_loader);

  // Parse the |ScreenUpdate| out from the response string.
  // Note that the |backdrop_screen_update| can be an empty instance if the
  // parsing has failed.
  backdrop::ScreenUpdate backdrop_screen_update =
      BackdropClientConfig::ParseScreenUpdateFromResponse(*response);

  // Store the information to a public struct and notify the caller.
  auto screen_update = ToScreenUpdate(backdrop_screen_update);
  std::move(callback).Run(screen_update);
}

void AmbientBackendControllerImpl::StartToGetSettings(
    GetSettingsCallback callback,
    const std::string& gaia_id,
    const std::string& access_token) {
  if (gaia_id.empty() || access_token.empty()) {
    std::move(callback).Run(/*topic_source=*/base::nullopt);
    return;
  }

  std::string client_id = GetClientId();
  BackdropClientConfig::Request request =
      backdrop_client_config_.CreateGetSettingsRequest(gaia_id, access_token,
                                                       client_id);
  auto resource_request = CreateResourceRequest(request);

  auto backdrop_url_loader = std::make_unique<BackdropURLLoader>();
  auto* loader_ptr = backdrop_url_loader.get();
  loader_ptr->Start(
      std::move(resource_request), request.body, NO_TRAFFIC_ANNOTATION_YET,
      base::BindOnce(&AmbientBackendControllerImpl::OnGetSettings,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(backdrop_url_loader)));
}

void AmbientBackendControllerImpl::OnGetSettings(
    GetSettingsCallback callback,
    std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
    std::unique_ptr<std::string> response) {
  DCHECK(backdrop_url_loader);

  auto settings = BackdropClientConfig::ParseGetSettingsResponse(*response);
  // |art_settings| should not be empty if parsed successfully.
  if (settings.art_settings.empty()) {
    std::move(callback).Run(base::nullopt);
  } else {
    for (auto& art_setting : settings.art_settings) {
      art_setting.visible = IsArtSettingVisible(art_setting);
      art_setting.enabled = art_setting.enabled && art_setting.visible;
    }
    std::move(callback).Run(settings);
  }
}

void AmbientBackendControllerImpl::StartToUpdateSettings(
    const AmbientSettings& settings,
    UpdateSettingsCallback callback,
    const std::string& gaia_id,
    const std::string& access_token) {
  if (gaia_id.empty() || access_token.empty()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  std::string client_id = GetClientId();
  BackdropClientConfig::Request request =
      backdrop_client_config_.CreateUpdateSettingsRequest(gaia_id, access_token,
                                                          client_id, settings);
  auto resource_request = CreateResourceRequest(request);

  auto backdrop_url_loader = std::make_unique<BackdropURLLoader>();
  auto* loader_ptr = backdrop_url_loader.get();
  loader_ptr->Start(
      std::move(resource_request), request.body, NO_TRAFFIC_ANNOTATION_YET,
      base::BindOnce(&AmbientBackendControllerImpl::OnUpdateSettings,
                     weak_factory_.GetWeakPtr(), std::move(callback), settings,
                     std::move(backdrop_url_loader)));
}

void AmbientBackendControllerImpl::OnUpdateSettings(
    UpdateSettingsCallback callback,
    const AmbientSettings& settings,
    std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
    std::unique_ptr<std::string> response) {
  DCHECK(backdrop_url_loader);

  const bool success =
      BackdropClientConfig::ParseUpdateSettingsResponse(*response);

  if (success) {
    // Store information about the ambient mode settings in a user pref so that
    // it can be uploaded as a histogram.
    Shell::Get()->session_controller()->GetPrimaryUserPrefService()->SetInteger(
        ambient::prefs::kAmbientModePhotoSourcePref,
        static_cast<int>(ambient::AmbientSettingsToPhotoSource(settings)));
  }

  std::move(callback).Run(success);

  // Clear disk cache when Settings changes.
  // TODO(wutao): Use observer pattern. Need to future narrow down
  // the clear up only on albums changes, not on temperature unit
  // changes.
  if (success) {
    Shell::Get()
        ->ambient_controller()
        ->ambient_photo_controller()
        ->ClearCache();
  }
}

void AmbientBackendControllerImpl::FetchSettingPreviewInternal(
    int preview_width,
    int preview_height,
    OnSettingPreviewFetchedCallback callback,
    const std::string& gaia_id,
    const std::string& access_token) {
  if (gaia_id.empty() || access_token.empty()) {
    LOG(ERROR) << "Failed to fetch access token";
    // Returns an empty instance to indicate the failure.
    std::move(callback).Run(/*preview_urls=*/{});
    return;
  }

  BackdropClientConfig::Request request =
      backdrop_client_config_.CreateFetchSettingPreviewRequest(
          preview_width, preview_height, gaia_id, access_token);
  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateResourceRequest(request);
  auto backdrop_url_loader = std::make_unique<BackdropURLLoader>();
  auto* loader_ptr = backdrop_url_loader.get();
  loader_ptr->Start(
      std::move(resource_request), /*request_body=*/base::nullopt,
      NO_TRAFFIC_ANNOTATION_YET,
      base::BindOnce(&AmbientBackendControllerImpl::OnSettingPreviewFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(backdrop_url_loader)));
}

void AmbientBackendControllerImpl::OnSettingPreviewFetched(
    OnSettingPreviewFetchedCallback callback,
    std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
    std::unique_ptr<std::string> response) {
  DCHECK(backdrop_url_loader);

  // Parse the |SettingPreviewResponse| out from the response string.
  // Note that the |preview_urls| can be empty if the parsing has failed.
  std::vector<std::string> preview_urls =
      BackdropClientConfig::ParseSettingPreviewResponse(*response);
  std::move(callback).Run(std::move(preview_urls));
}

void AmbientBackendControllerImpl::FetchPersonalAlbumsInternal(
    int banner_width,
    int banner_height,
    int num_albums,
    const std::string& resume_token,
    OnPersonalAlbumsFetchedCallback callback,
    const std::string& gaia_id,
    const std::string& access_token) {
  if (gaia_id.empty() || access_token.empty()) {
    LOG(ERROR) << "Failed to fetch access token";
    // Returns an empty instance to indicate the failure.
    std::move(callback).Run(ash::PersonalAlbums());
    return;
  }

  BackdropClientConfig::Request request =
      backdrop_client_config_.CreateFetchPersonalAlbumsRequest(
          banner_width, banner_height, num_albums, resume_token, gaia_id,
          access_token);
  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateResourceRequest(request);
  auto backdrop_url_loader = std::make_unique<BackdropURLLoader>();
  auto* loader_ptr = backdrop_url_loader.get();
  loader_ptr->Start(
      std::move(resource_request), /*request_body=*/base::nullopt,
      NO_TRAFFIC_ANNOTATION_YET,
      base::BindOnce(&AmbientBackendControllerImpl::OnPersonalAlbumsFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(backdrop_url_loader)));
}

void AmbientBackendControllerImpl::OnPersonalAlbumsFetched(
    OnPersonalAlbumsFetchedCallback callback,
    std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
    std::unique_ptr<std::string> response) {
  DCHECK(backdrop_url_loader);

  // Parse the |PersonalAlbumsResponse| out from the response string.
  // Note that the |personal_albums| can be an empty instance if the parsing has
  // failed.
  ash::PersonalAlbums personal_albums =
      BackdropClientConfig::ParsePersonalAlbumsResponse(*response);
  std::move(callback).Run(std::move(personal_albums));
}

void AmbientBackendControllerImpl::FetchSettingsAndAlbums(
    int banner_width,
    int banner_height,
    int num_albums,
    OnSettingsAndAlbumsFetchedCallback callback) {
  auto on_done = base::BarrierClosure(
      /*num_callbacks=*/2,
      base::BindOnce(&AmbientBackendControllerImpl::OnSettingsAndAlbumsFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback)));

  GetSettings(base::BindOnce(&AmbientBackendControllerImpl::OnSettingsFetched,
                             weak_factory_.GetWeakPtr(), on_done));

  FetchPersonalAlbums(
      banner_width, banner_height, num_albums, /*resume_token=*/"",
      base::BindOnce(&AmbientBackendControllerImpl::OnAlbumsFetched,
                     weak_factory_.GetWeakPtr(), on_done));
}

void AmbientBackendControllerImpl::OnSettingsFetched(
    base::RepeatingClosure on_done,
    const base::Optional<ash::AmbientSettings>& settings) {
  settings_ = settings;
  std::move(on_done).Run();
}

void AmbientBackendControllerImpl::OnAlbumsFetched(
    base::RepeatingClosure on_done,
    ash::PersonalAlbums personal_albums) {
  personal_albums_ = std::move(personal_albums);
  std::move(on_done).Run();
}

void AmbientBackendControllerImpl::OnSettingsAndAlbumsFetched(
    OnSettingsAndAlbumsFetchedCallback callback) {
  std::move(callback).Run(settings_, std::move(personal_albums_));
}

}  // namespace ash

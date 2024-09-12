// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/focus_mode/focus_mode_ui.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/url_constants.h"
#include "ash/style/switch.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_controller.h"
#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_types.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/focus_mode/mojom/focus_mode.mojom-shared.h"
#include "ash/webui/grit/ash_focus_mode_resources.h"
#include "ash/webui/grit/ash_focus_mode_resources_map.h"
#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/gfx/codec/webp_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/webui/webui_allowlist.h"
#include "url/url_constants.h"

namespace ash {

namespace {

// The artwork needs to be at least this big to be shown. If the source is
// smaller, we'll scale it up to this size. This constant is based on
// global_media_controls::kMediaItemArtworkMinSize.
constexpr gfx::Size kArtworkMinSize(114, 114);

// Minimum time interval of the rate limiter between two playback reports for
// the same track.
constexpr base::TimeDelta kRateLimitingInterval = base::Seconds(6);

// Resizes an image so that it is at least `kArtworkMinSize`.
gfx::ImageSkia EnsureMinSize(const gfx::ImageSkia& image) {
  // We are assuming that the input artwork is roughly square in aspect ratio.
  if (image.width() < kArtworkMinSize.width() ||
      image.height() < kArtworkMinSize.height()) {
    return gfx::ImageSkiaOperations::CreateResizedImage(
        image, skia::ImageOperations::RESIZE_GOOD, kArtworkMinSize);
  }

  return image;
}

// Takes the given image, encodes it as webp and returns it in the form of a
// data URL. Returns an empty URL on error.
GURL MakeImageDataURL(const gfx::ImageSkia& image) {
  if (image.isNull()) {
    return {};
  }
  gfx::ImageSkia resized_image = EnsureMinSize(image);

  std::vector<unsigned char> webp_data;
  if (!gfx::WebpCodec::Encode(*resized_image.bitmap(), 50, &webp_data)) {
    return {};
  }

  GURL url("data:image/webp;base64," + base::Base64Encode(webp_data));
  if (url.spec().size() > url::kMaxURLChars) {
    return {};
  }

  return url;
}

youtube_music::PlaybackState GetPlaybackState(
    const focus_mode::mojom::PlaybackState playback_state) {
  switch (playback_state) {
    case focus_mode::mojom::PlaybackState::kPlaying:
      return youtube_music::PlaybackState::kPlaying;
    case focus_mode::mojom::PlaybackState::kPaused:
      return youtube_music::PlaybackState::kPaused;
    case focus_mode::mojom::PlaybackState::kSwitchedToNext:
      return youtube_music::PlaybackState::kSwitchedToNext;
    case focus_mode::mojom::PlaybackState::kEnded:
      return youtube_music::PlaybackState::kEnded;
    case focus_mode::mojom::PlaybackState::kNone:
      return youtube_music::PlaybackState::kNone;
  }
}

bool ValidatePlaybackData(const focus_mode::mojom::PlaybackDataPtr& data) {
  if (data.is_null()) {
    DLOG(ERROR) << "Failed to validate the playback data: empty data";
    return false;
  }

  if (data->state == focus_mode::mojom::PlaybackState::kNone) {
    DLOG(ERROR) << "Failed to validate the playback data: uninitialized state";
    return false;
  }

  if (data->playback_start_offset < 0 || data->playback_start_offset > 18000) {
    DLOG(ERROR) << "Failed to validate the playback data: bad playback data, "
                   "playback_start_offset="
                << data->playback_start_offset;
    return false;
  }

  if (data->media_time_current < 0 || data->media_time_current > 18000) {
    DLOG(ERROR) << "Failed to validate the playback data: bad playback data, "
                   "media_time_current="
                << data->media_time_current;
    return false;
  }

  if (data->media_start < 0 || data->media_start > 18000 ||
      data->media_end < 0 || data->media_end > 18000 ||
      data->media_start >= data->media_end) {
    DLOG(ERROR) << "Failed to validate the playback data: bad playback data, "
                   "media_start="
                << data->media_start << ", media_end=" << data->media_end;
    return false;
  }

  return true;
}

}  // namespace

class FocusModeTrackProvider : public focus_mode::mojom::TrackProvider {
 public:
  void GetTrack(GetTrackCallback callback) override {
    auto* sounds_controller =
        FocusModeController::Get()->focus_mode_sounds_controller();
    sounds_controller->GetNextTrack(
        base::BindOnce(&FocusModeTrackProvider::HandleTrack,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetMediaClient(
      mojo::PendingRemote<focus_mode::mojom::MediaClient> client) override {
    client_remote_.reset();
    client_remote_.Bind(std::move(client));
  }

  void ReportPlayback(focus_mode::mojom::PlaybackDataPtr data) override {
    if (!ValidatePlaybackData(data)) {
      return;
    }

    youtube_music::MediaSegments media_segments;
    media_segments.insert(youtube_music::MediaSegment{
        data->media_start, data->media_end, data->client_start_time});
    const youtube_music::PlaybackData playback_data(
        GetPlaybackState(data->state), data->title, data->url,
        data->client_current_time, data->playback_start_offset,
        data->media_time_current, media_segments, data->initial_playback);
    rate_limiter_.OnPlaybackEvent(playback_data, base::Time::Now());
  }

  void ReportPlayerError() override {
    if (auto* controller = FocusModeController::Get()) {
      controller->focus_mode_sounds_controller()->ReportPlayerError();
    }
  }

  void BindInterface(
      mojo::PendingReceiver<focus_mode::mojom::TrackProvider> receiver) {
    receiver_.reset();
    receiver_.Bind(std::move(receiver));
  }

 private:
  // A simple rate limiter for YouTube Music APIs.
  class YTMRateLimiter {
   public:
    bool ShouldLimit(const youtube_music::PlaybackData& playback_data,
                     const base::Time timestamp) {
      // Do not limit if it's a different track that can not be aggregated.
      const bool same_track =
          last_playback_.has_value() &&
          last_playback_.value().CanAggregateWithNewData(playback_data) &&
          (!pending_playback_.has_value() ||
           pending_playback_.value().CanAggregateWithNewData(playback_data));
      if (!same_track) {
        return false;
      }

      // Do not limit if it's not within the interval.
      const bool within_interval =
          last_timestamp_.has_value() &&
          timestamp < last_timestamp_.value() + kRateLimitingInterval;
      if (!within_interval) {
        return false;
      }

      // Do not limit if it's the last event for the track.
      const bool last_event =
          playback_data.state ==
              youtube_music::PlaybackState::kSwitchedToNext ||
          playback_data.state == youtube_music::PlaybackState::kEnded;
      return !last_event;
    }

    void OnPlaybackEvent(youtube_music::PlaybackData playback_data,
                         const base::Time timestamp) {
      FocusModeSoundsController* sounds_controller =
          FocusModeController::Get()->focus_mode_sounds_controller();
      if (!sounds_controller) {
        return;
      }

      if (ShouldLimit(playback_data, timestamp)) {
        // If it should limit, aggregate the new data into the pending data and
        // wait for the next event.
        if (pending_playback_.has_value()) {
          pending_playback_->AggregateWithNewData(playback_data);
        } else {
          pending_playback_ = playback_data;
        }
      } else {
        // If it should *not* limit, either:
        //   - If the pending data and the new data are from the *same* track,
        //   aggregate the pending data into the new data and report it.
        //   - If the pending data and the new data are from *different* tracks,
        //   report the pending data and the new data separately.
        if (pending_playback_.has_value()) {
          if (pending_playback_.value().CanAggregateWithNewData(
                  playback_data)) {
            playback_data.AggregateWithNewData(pending_playback_.value());
          } else {
            sounds_controller->ReportYouTubeMusicPlayback(
                pending_playback_.value());
          }
          pending_playback_.reset();
        }
        sounds_controller->ReportYouTubeMusicPlayback(playback_data);
        last_timestamp_ = timestamp;
        last_playback_ = playback_data;
      }
    }

   private:
    // Timestamp for last reported playback.
    std::optional<base::Time> last_timestamp_ = std::nullopt;

    // Last reported playback data.
    std::optional<youtube_music::PlaybackData> last_playback_ = std::nullopt;

    // Pending playback data to report to the backend.
    std::optional<youtube_music::PlaybackData> pending_playback_ = std::nullopt;
  };

  void HandleTrack(focus_mode::mojom::TrackProvider::GetTrackCallback callback,
                   const std::optional<FocusModeSoundsDelegate::Track>& track) {
    if (!track) {
      std::move(callback).Run(focus_mode::mojom::TrackDefinition::New());
      return;
    }

    // If there is no thumbnail, then we can reply immediately.
    if (!track->thumbnail_url.is_valid()) {
      auto mojo_track = focus_mode::mojom::TrackDefinition::New(
          track->title, track->artist, /*thumbnail_url=*/GURL{},
          track->source_url, track->enable_playback_reporting);
      std::move(callback).Run(std::move(mojo_track));
      return;
    }

    // Otherwise we need to download and convert the thumbnail first.
    FocusModeSoundsController::DownloadTrackThumbnail(
        track->thumbnail_url,
        base::BindOnce(&FocusModeTrackProvider::OnThumbnailDownloaded,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       *track));
  }

  void OnThumbnailDownloaded(GetTrackCallback callback,
                             const FocusModeSoundsDelegate::Track& track,
                             const gfx::ImageSkia& image) {
    auto mojo_track = focus_mode::mojom::TrackDefinition::New(
        track.title, track.artist, MakeImageDataURL(image), track.source_url,
        track.enable_playback_reporting);
    std::move(callback).Run(std::move(mojo_track));
  }

  YTMRateLimiter rate_limiter_;
  mojo::Remote<focus_mode::mojom::MediaClient> client_remote_;
  mojo::Receiver<focus_mode::mojom::TrackProvider> receiver_{this};
  base::WeakPtrFactory<FocusModeTrackProvider> weak_factory_{this};
};

FocusModeUI::FocusModeUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui),
      track_provider_(std::make_unique<FocusModeTrackProvider>()) {
  // Set up the chrome://focus-mode-media source. Note that for the trusted
  // page, we need to pass the *host* as second parameter.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIFocusModeMediaHost);

  // This is needed so that the page can load the iframe from chrome-untrusted.
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);

  // Setup chrome://focus-mode-media main page.
  source->AddResourcePath("", IDR_ASH_FOCUS_MODE_FOCUS_MODE_HTML);
  // Add chrome://focus-mode-media content.
  source->AddResourcePaths(
      base::make_span(kAshFocusModeResources, kAshFocusModeResourcesSize));

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc, "default-src 'self';");
  // Enables the page to load the untrusted page in an iframe.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc,
      base::StringPrintf("frame-src %s;", chrome::kChromeUIFocusModePlayerURL));
  ash::EnableTrustedTypesCSP(source);

  // This sets the untrusted page to be in a web app scope. This in turn enables
  // autoplay of audio on the page. Without this, the page would require user
  // interaction in order to play audio, which isn't possible since the web UI
  // is hidden. See AutoPlayPolicy::GetAutoplayPolicyForDocument for more info.
  auto* web_contents = web_ui->GetWebContents();
  auto prefs = web_contents->GetOrCreateWebPreferences();
  prefs.web_app_scope = GURL(chrome::kChromeUIFocusModePlayerURL);
  web_contents->SetWebPreferences(prefs);
}

FocusModeUI::~FocusModeUI() = default;

void FocusModeUI::BindInterface(
    mojo::PendingReceiver<focus_mode::mojom::TrackProvider> receiver) {
  track_provider_->BindInterface(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(FocusModeUI)

FocusModeUIConfig::FocusModeUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIFocusModeMediaHost) {}

bool FocusModeUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return ash::features::IsFocusModeEnabled();
}

}  // namespace ash

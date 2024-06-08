// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/focus_mode/focus_mode_ui.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/url_constants.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_controller.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/grit/ash_focus_mode_resources.h"
#include "ash/webui/grit/ash_focus_mode_resources_map.h"
#include "base/base64.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
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

  void BindInterface(
      mojo::PendingReceiver<focus_mode::mojom::TrackProvider> receiver) {
    receiver_.reset();
    receiver_.Bind(std::move(receiver));
  }

 private:
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
          track->source_url);
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
        track.title, track.artist, MakeImageDataURL(image), track.source_url);
    std::move(callback).Run(std::move(mojo_track));
  }

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
    : WebUIConfig(content::kChromeUIScheme,
                  chrome::kChromeUIFocusModeMediaHost) {}

std::unique_ptr<content::WebUIController>
FocusModeUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                         const GURL& url) {
  return std::make_unique<FocusModeUI>(web_ui);
}

bool FocusModeUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return ash::features::IsFocusModeEnabled();
}

}  // namespace ash

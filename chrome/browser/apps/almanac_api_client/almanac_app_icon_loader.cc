// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/almanac_api_client/almanac_app_icon_loader.h"

#include "base/functional/bind.h"
#include "base/one_shot_event.h"
#include "base/strings/string_util.h"
#include "chrome/browser/apps/almanac_api_client/almanac_icon_cache.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace apps {

namespace {

constexpr int kSvgRasterSize = 192;

bool IsSvgExtension(const GURL& icon_url, std::string_view icon_mime_type) {
  std::string url_string = icon_url.spec();
  return icon_mime_type.empty()
             ? base::EndsWith(url_string, ".svg", base::CompareCase::SENSITIVE)
             : icon_mime_type == "image/svg+xml";
}

}  // namespace

// Creates a background WebContents for downloading and rendering an SVG image.
class AlmanacAppIconLoader::SvgLoader : public content::WebContentsObserver {
 public:
  explicit SvgLoader(Profile& profile) {
    // SVG icons require a full renderer to rasterize to a bitmap.
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(&profile));

    // Load about:blank to initialise the WebContents before using it.
    Observe(web_contents_.get());
    content::NavigationController::LoadURLParams load_params{
        GURL(url::kAboutBlankURL)};
    load_params.transition_type = ui::PAGE_TRANSITION_GENERATED;
    web_contents_->GetController().LoadURLWithParams(load_params);
    // Logic continues in DidStopLoading().
  }

  ~SvgLoader() override {}

  // content::WebContentsObserver:
  void DidStopLoading() override {
    if (!web_contents_ready_.is_signaled()) {
      web_contents_ready_.Signal();
    }
  }

  void GetSvg(GURL svg_url,
              base::OnceCallback<void(std::optional<SkBitmap>)> callback) {
    web_contents_ready_.Post(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&content::WebContents::DownloadImage),
                       web_contents_->GetWeakPtr(), svg_url,
                       /*is_favicon=*/false,
                       gfx::Size(kSvgRasterSize, kSvgRasterSize),
                       /*max_bitmap_size=*/0,  // No max size.
                       /*bypass_cache=*/false,
                       base::BindOnce(&SvgLoader::OnImageDownloaded,
                                      std::move(callback))));
  }

  void WebContentsDestroyed() override { Observe(nullptr); }

 private:
  static void OnImageDownloaded(
      base::OnceCallback<void(std::optional<SkBitmap>)> callback,
      int id,
      int http_status_code,
      const GURL& image_url,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& sizes) {
    if (!bitmaps.empty()) {
      std::move(callback).Run(bitmaps.front());
      return;
    }
    std::move(callback).Run(std::nullopt);
  }

  std::unique_ptr<content::WebContents> web_contents_;
  base::OneShotEvent web_contents_ready_;
};

AlmanacAppIconLoader::AlmanacAppIconLoader(Profile& profile)
    : profile_(profile.GetWeakPtr()) {}

AlmanacAppIconLoader::~AlmanacAppIconLoader() = default;

void AlmanacAppIconLoader::GetAppIcon(
    const GURL& icon_url,
    std::string_view icon_mime_type,
    bool icon_masking_allowed,
    base::OnceCallback<void(apps::IconValuePtr)> callback) {
  if (!profile_) {
    std::move(callback).Run(nullptr);
  }

  if (IsSvgExtension(icon_url, icon_mime_type)) {
    if (!svg_loader_) {
      svg_loader_ = std::make_unique<SvgLoader>(*profile_);
    }
    svg_loader_->GetSvg(
        icon_url, base::BindOnce(&AlmanacAppIconLoader::OnSvgLoaded,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 icon_masking_allowed, std::move(callback)));
    return;
  }

  if (!icon_cache_) {
    icon_cache_ = std::make_unique<AlmanacIconCache>(profile_->GetProfileKey());
  }
  icon_cache_->GetIcon(
      icon_url, base::BindOnce(&AlmanacAppIconLoader::OnBitmapLoaded,
                               weak_ptr_factory_.GetWeakPtr(),
                               icon_masking_allowed, std::move(callback)));
}

void AlmanacAppIconLoader::OnSvgLoaded(
    bool icon_masking_allowed,
    base::OnceCallback<void(apps::IconValuePtr)> callback,
    std::optional<SkBitmap> bitmap) {
  if (!bitmap.has_value()) {
    std::move(callback).Run(nullptr);
    return;
  }

  ApplyIconEffects(icon_masking_allowed,
                   gfx::Image::CreateFrom1xBitmap(bitmap.value()),
                   std::move(callback));
}

void AlmanacAppIconLoader::OnBitmapLoaded(
    bool icon_masking_allowed,
    base::OnceCallback<void(apps::IconValuePtr)> callback,
    const gfx::Image& icon_bitmap) {
  ApplyIconEffects(icon_masking_allowed, icon_bitmap, std::move(callback));
}

void AlmanacAppIconLoader::ApplyIconEffects(
    bool icon_masking_allowed,
    gfx::Image icon_bitmap,
    base::OnceCallback<void(apps::IconValuePtr)> callback) {
  if (!profile_ || icon_bitmap.IsEmpty()) {
    std::move(callback).Run(nullptr);
    return;
  }

  apps::IconValuePtr icon_value = std::make_unique<apps::IconValue>();
  icon_value->icon_type = apps::IconType::kStandard;
  icon_value->is_placeholder_icon = false;
  icon_value->is_maskable_icon = icon_masking_allowed;
  icon_value->uncompressed = icon_bitmap.AsImageSkia();

  apps::ApplyIconEffects(
      profile_.get(), /*app_id=*/std::nullopt,
      icon_masking_allowed ? apps::IconEffects::kCrOsStandardMask
                           : apps::IconEffects::kCrOsStandardIcon,
      icon_bitmap.Width(), std::move(icon_value), std::move(callback));
}

}  // namespace apps

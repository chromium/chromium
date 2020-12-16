// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/ntp_icon_source.h"

#include <stddef.h>
#include <algorithm>
#include <cmath>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/hash/sha1.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/platform_locale_settings.h"
#include "components/favicon/core/fallback_url_util.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/favicon_util.h"
#include "components/history/core/browser/top_sites.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/suggestions/suggestions_service.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/common/image_util.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/native_theme/native_theme.h"
#include "url/gurl.h"

namespace {

const char kIconSourceUmaClientName[] = "NtpIconSource";

const char kShowFallbackMonogramParam[] = "show_fallback_monogram";

// The requested color of the icon in 8-digit Hex format (e.g., #757575FF).
const char kColorParam[] = "color";

// The requested size of the icon.
const char kSizeParam[] = "size";

// The URL for which to create an icon.
const char kUrlParam[] = "url";

// Size of the icon background (gray circle), in dp.
const int kIconSizeDip = 48;

// Size of the favicon fallback (letter + colored circle), in dp.
const int kFallbackSizeDip = 32;

// Maximum size of the icon, in dp.
const int kMaxIconSizeDip = 192;

// URL to the server favicon service. "alt=404" means the service will return a
// 404 if an icon can't be found.
const char kServerFaviconURL[] =
    "https://s2.googleusercontent.com/s2/favicons?domain_url=%s&alt=404&sz=32";

// Used to parse the specification from the path.
struct ParsedNtpIconPath {
  // The URL for which the icon is being requested.
  GURL url;

  // The requested color of the icon in 8-digit Hex format (e.g., #757575FF).
  std::string color_rgba;

  // The size of the requested icon in dip.
  int size_in_dip = 0;

  // The device scale factor of the requested icon.
  float device_scale_factor = 1.0;

  // Whether to show a circle + letter monogram if an icon is unable.
  bool show_fallback_monogram = true;
};

float GetMaxDeviceScaleFactor() {
  std::vector<float> favicon_scales = favicon_base::GetFaviconScales();
  DCHECK(!favicon_scales.empty());
  return favicon_scales.back();
}

// Parses the path after chrome-search://ntpicon/. Example path is
// "?size=24@2x&url=https%3A%2F%2Fcnn.com"
const ParsedNtpIconPath ParseNtpIconPath(const std::string& path) {
  ParsedNtpIconPath parsed;
  parsed.show_fallback_monogram = true;
  parsed.size_in_dip = gfx::kFaviconSize;
  parsed.url = GURL();

  if (path.empty())
    return parsed;

  // NOTE(dbeam): can't start with an empty GURL() and use ReplaceComponents()
  // because it's not allowed for invalid URLs.
  GURL request = GURL(base::StrCat({chrome::kChromeSearchScheme, "://",
                                    chrome::kChromeUINewTabIconHost}))
                     .Resolve(path);

  for (net::QueryIterator it(request); !it.IsAtEnd(); it.Advance()) {
    std::string key = it.GetKey();
    if (key == kShowFallbackMonogramParam) {
      parsed.show_fallback_monogram = it.GetUnescapedValue() != "false";
    } else if (key == kColorParam) {
      parsed.color_rgba = it.GetUnescapedValue();
    } else if (key == kSizeParam) {
      std::vector<std::string> pieces =
          base::SplitString(it.GetUnescapedValue(), "@", base::TRIM_WHITESPACE,
                            base::SPLIT_WANT_NONEMPTY);
      if (pieces.empty() || pieces.size() > 2)
        continue;
      int size_in_dip = 0;
      if (!base::StringToInt(pieces[0], &size_in_dip))
        continue;
      parsed.size_in_dip = std::min(size_in_dip, kMaxIconSizeDip);
      if (pieces.size() > 1) {
        float scale_factor = 0.0;
        webui::ParseScaleFactor(pieces[1], &scale_factor);
        // Do not exceed the maximum scale factor for the device.
        parsed.device_scale_factor =
            std::min(scale_factor, GetMaxDeviceScaleFactor());
      }
    } else if (key == kUrlParam) {
      parsed.url = GURL(it.GetUnescapedValue());
    }
  }

  return parsed;
}

// Will draw |bitmap| in the center of the |canvas| of a given |size|.
// |bitmap| keeps its size.
void DrawFavicon(const SkBitmap& bitmap, gfx::Canvas* canvas, int size) {
  int x_origin = (size - bitmap.width()) / 2;
  int y_origin = (size - bitmap.height()) / 2;
  canvas->DrawImageInt(gfx::ImageSkia(gfx::ImageSkiaRep(bitmap, /*scale=*/1.0)),
                       x_origin, y_origin);
}

}  // namespace

struct NtpIconSource::NtpIconRequest {
  NtpIconRequest(content::URLDataSource::GotDataCallback cb,
                 const GURL& path,
                 int icon_size_in_pixels,
                 std::string color_rgba,
                 float scale,
                 bool show_fallback_monogram)
      : callback(std::move(cb)),
        path(path),
        icon_size_in_pixels(icon_size_in_pixels),
        color_rgba(color_rgba),
        device_scale_factor(scale),
        show_fallback_monogram(show_fallback_monogram) {}

  NtpIconRequest(NtpIconRequest&& other) = default;
  NtpIconRequest& operator=(NtpIconRequest&& other) = default;

  ~NtpIconRequest() {}

  content::URLDataSource::GotDataCallback callback;
  GURL path;
  int icon_size_in_pixels;
  std::string color_rgba;
  float device_scale_factor;
  bool show_fallback_monogram;
};

NtpIconSource::NtpIconSource(Profile* profile)
    : profile_(profile),
      image_fetcher_(std::make_unique<image_fetcher::ImageFetcherImpl>(
          std::make_unique<ImageDecoderImpl>(),
          content::BrowserContext::GetDefaultStoragePartition(profile)
              ->GetURLLoaderFactoryForBrowserProcess())) {}

NtpIconSource::~NtpIconSource() = default;

std::string NtpIconSource::GetSource() {
  return chrome::kChromeUINewTabIconHost;
}

void NtpIconSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);

  const ParsedNtpIconPath parsed =
      ParseNtpIconPath(content::URLDataSource::URLToRequestPath(url));

  if (parsed.url.is_valid()) {
    int icon_size_in_pixels =
        std::ceil(parsed.size_in_dip * parsed.device_scale_factor);
    NtpIconRequest request(std::move(callback), parsed.url, icon_size_in_pixels,
                           parsed.color_rgba, parsed.device_scale_factor,
                           parsed.show_fallback_monogram);

    // Check if the requested URL is part of the prepopulated pages (currently,
    // only the Web Store).
    scoped_refptr<history::TopSites> top_sites =
        TopSitesFactory::GetForProfile(profile_);
    if (top_sites) {
      for (const auto& prepopulated_page : top_sites->GetPrepopulatedPages()) {
        if (parsed.url == prepopulated_page.most_visited.url) {
          gfx::Image& image =
              ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                  prepopulated_page.favicon_id);

          // Resize as necessary.
          gfx::Size target_size(icon_size_in_pixels, icon_size_in_pixels);
          if (!image.IsEmpty() && image.Size() != target_size) {
            gfx::ImageSkia resized_image =
                gfx::ImageSkiaOperations::CreateResizedImage(
                    image.AsImageSkia(), skia::ImageOperations::RESIZE_BEST,
                    target_size);
            ReturnRenderedIconForRequest(std::move(request),
                                         gfx::Image(resized_image).AsBitmap());
          } else {
            ReturnRenderedIconForRequest(std::move(request), image.AsBitmap());
          }
          return;
        }
      }
    }

    // This will query for a local favicon. If not found, will take alternative
    // action in OnLocalFaviconAvailable.
    const bool fallback_to_host = true;
    favicon_service->GetRawFaviconForPageURL(
        parsed.url, {favicon_base::IconType::kFavicon}, icon_size_in_pixels,
        fallback_to_host,
        base::BindOnce(&NtpIconSource::OnLocalFaviconAvailable,
                       weak_ptr_factory_.GetWeakPtr(), std::move(request)),
        &cancelable_task_tracker_);
  } else {
    std::move(callback).Run(nullptr);
  }
}

std::string NtpIconSource::GetMimeType(const std::string&) {
  // NOTE: this may not always be correct for all possible types that this
  // source will serve. Seems to work fine, however.
  return "image/png";
}

bool NtpIconSource::ShouldServiceRequest(
    const GURL& url,
    content::BrowserContext* browser_context,
    int render_process_id) {
  if (url.SchemeIs(chrome::kChromeSearchScheme)) {
    return InstantService::ShouldServiceRequest(url, browser_context,
                                                render_process_id);
  }
  return URLDataSource::ShouldServiceRequest(url, browser_context,
                                             render_process_id);
}

void NtpIconSource::OnLocalFaviconAvailable(
    NtpIconRequest request,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  if (bitmap_result.is_valid()) {
    // A local favicon was found. Decode it to an SkBitmap so it can eventually
    // be passed as valid image data to ReturnRenderedIconForRequest.
    SkBitmap bitmap;
    bool result =
        gfx::PNGCodec::Decode(bitmap_result.bitmap_data.get()->front(),
                              bitmap_result.bitmap_data.get()->size(), &bitmap);
    DCHECK(result);
    ReturnRenderedIconForRequest(std::move(request), bitmap);
  } else {
    // Since a local favicon was not found, attempt to fetch a server icon if
    // the url is known to the server (this last check is important to avoid
    // leaking private history to the server).
    RequestServerFavicon(std::move(request));
  }
}

bool NtpIconSource::IsRequestedUrlInServerSuggestions(const GURL& url) {
  suggestions::SuggestionsService* suggestions_service =
      suggestions::SuggestionsServiceFactory::GetForProfile(profile_);
  if (!suggestions_service)
    return false;

  suggestions::SuggestionsProfile profile =
      suggestions_service->GetSuggestionsDataFromCache().value_or(
          suggestions::SuggestionsProfile());
  auto position =
      std::find_if(profile.suggestions().begin(), profile.suggestions().end(),
                   [url](const suggestions::ChromeSuggestion& suggestion) {
                     return suggestion.url() == url.spec();
                   });
  return position != profile.suggestions().end();
}

void NtpIconSource::RequestServerFavicon(NtpIconRequest request) {
  // Only fetch a server icon if the page url is known to the server. This check
  // is important to avoid leaking private history to the server.
  const GURL server_favicon_url =
      GURL(base::StringPrintf(kServerFaviconURL, request.path.spec().c_str()));
  if (!server_favicon_url.is_valid() ||
      !IsRequestedUrlInServerSuggestions(request.path)) {
    ReturnRenderedIconForRequest(std::move(request), SkBitmap());
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("ntp_icon_source", R"(
      semantics {
        sender: "NTP Icon Source"
        description:
          "Retrieves icons for site suggestions based on the user's browsing "
          "history, for use e.g. on the New Tab page."
        trigger:
          "Triggered when an icon for a suggestion is required (e.g. on "
          "the New Tab page), no local icon is available and the URL is known "
          "to the server (hence no private information is revealed)."
        data: "The URL for which to retrieve an icon."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users cannot disable this feature. The feature is enabled by "
          "default."
        policy_exception_justification: "Not implemented."
      })");
  image_fetcher::ImageFetcherParams params(traffic_annotation,
                                           kIconSourceUmaClientName);
  params.set_frame_size(
      gfx::Size(request.icon_size_in_pixels, request.icon_size_in_pixels));
  image_fetcher_->FetchImage(
      server_favicon_url,
      base::BindOnce(&NtpIconSource::OnServerFaviconAvailable,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request)),
      std::move(params));
}

void NtpIconSource::OnServerFaviconAvailable(
    NtpIconRequest request,
    const gfx::Image& fetched_image,
    const image_fetcher::RequestMetadata& metadata) {
  // If a server icon was not found, |fetched_bitmap| will be empty and a
  // fallback icon will be eventually drawn.
  SkBitmap fetched_bitmap = fetched_image.AsBitmap();
  if (!fetched_bitmap.empty()) {
    // The received server icon bitmap may still be bigger than our desired
    // size, so resize it.
    fetched_bitmap = skia::ImageOperations::Resize(
        fetched_bitmap, skia::ImageOperations::RESIZE_BEST,
        request.icon_size_in_pixels, request.icon_size_in_pixels);
  }

  ReturnRenderedIconForRequest(std::move(request), fetched_bitmap);
}

void NtpIconSource::ReturnRenderedIconForRequest(NtpIconRequest request,
                                                 const SkBitmap& favicon) {
  // Only use even pixel sizes to avoid issues when centering the fallback
  // monogram.
  const int icon_size =
      std::round(kIconSizeDip * request.device_scale_factor * 0.5) * 2.0;
  const int fallback_size =
      std::round(kFallbackSizeDip * request.device_scale_factor * 0.5) * 2.0;

  SkBitmap bitmap;

  // If necessary, draw the colored fallback monogram.
  if (favicon.empty() && request.show_fallback_monogram) {
    bitmap = favicon::GenerateMonogramFavicon(request.path, icon_size,
                                              fallback_size);
  } else {
    bitmap.allocN32Pixels(icon_size, icon_size, false);
    cc::SkiaPaintCanvas paint_canvas(bitmap);
    gfx::Canvas canvas(&paint_canvas, 1.f);
    canvas.DrawColor(SK_ColorTRANSPARENT, SkBlendMode::kSrc);
    if (favicon.empty()) {
      const auto* default_favicon = favicon::GetDefaultFavicon().ToImageSkia();
      const auto& rep =
          default_favicon->GetRepresentation(request.device_scale_factor);
      gfx::ImageSkia scaled_image(rep);
      const auto resized = gfx::ImageSkiaOperations::CreateResizedImage(
          scaled_image, skia::ImageOperations::RESIZE_BEST,
          gfx::Size(fallback_size, fallback_size));
      auto bitmap = *resized.bitmap();

      SkColor color = 0;
      if (extensions::image_util::ParseHexColorString(request.color_rgba,
                                                      &color)) {
        bitmap = SkBitmapOperations::CreateColorMask(bitmap, color);
      }
      DrawFavicon(bitmap, &canvas, icon_size);
    } else {
      DrawFavicon(favicon, &canvas, icon_size);
    }
  }

  std::vector<unsigned char> bitmap_data;
  bool result = gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &bitmap_data);
  DCHECK(result);
  std::move(request.callback)
      .Run(base::RefCountedBytes::TakeVector(&bitmap_data));
}

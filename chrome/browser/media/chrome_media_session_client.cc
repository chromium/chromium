// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_media_session_client.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "media/base/media_switches.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/paint_vector_icon.h"

ChromeMediaSessionClient* ChromeMediaSessionClient::GetInstance() {
  static base::NoDestructor<ChromeMediaSessionClient> instance;
  return instance.get();
}

bool ChromeMediaSessionClient::ShouldHideMetadata(
    content::BrowserContext* browser_context) const {
  return base::FeatureList::IsEnabled(media::kHideIncognitoMediaMetadata) &&
         Profile::FromBrowserContext(browser_context)->IsIncognitoProfile();
}

std::u16string ChromeMediaSessionClient::GetTitlePlaceholder() const {
  return l10n_util::GetStringUTF16(
      IDS_MEDIA_CONTROLS_TITLE_PLACEHOLDER_INCOGNITO);
}

std::u16string ChromeMediaSessionClient::GetSourceTitlePlaceholder() const {
  return std::u16string();
}

std::u16string ChromeMediaSessionClient::GetArtistPlaceholder() const {
  return std::u16string();
}

std::u16string ChromeMediaSessionClient::GetAlbumPlaceholder() const {
  return std::u16string();
}

SkBitmap ChromeMediaSessionClient::GetThumbnailPlaceholder() const {
  // Dip size of one side of the thumbnail placeholder including padding.
  const int kThumbnailDipSize = 100;

  const gfx::ImageSkia incognito_icon =
      gfx::CreateVectorIcon(kIncognitoRefreshMenuIcon,
                            /*dip_size=*/kThumbnailDipSize / 2, SK_ColorBLACK);
  const gfx::ImageSkia incognito_icon_with_circle =
      gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
          /*radius=*/kThumbnailDipSize / 3, SK_ColorWHITE, incognito_icon);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(kThumbnailDipSize, kThumbnailDipSize);
  bitmap.eraseColor(SK_ColorTRANSPARENT);
  const gfx::ImageSkia background = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);

  const gfx::ImageSkia thumbnail_with_padding =
      gfx::ImageSkiaOperations::CreateSuperimposedImage(
          background, incognito_icon_with_circle);

  const float dsf =
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();

  return thumbnail_with_padding.GetRepresentation(dsf).GetBitmap();
}

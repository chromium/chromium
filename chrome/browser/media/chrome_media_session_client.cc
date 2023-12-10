// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_media_session_client.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "media/base/media_switches.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

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
  return *ui::ResourceBundle::GetSharedInstance()
              .GetImageSkiaNamed(IDR_INCOGNITO_WHITE_CIRCLE)
              ->bitmap();
}

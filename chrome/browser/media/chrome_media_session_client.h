// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CHROME_MEDIA_SESSION_CLIENT_H_
#define CHROME_BROWSER_MEDIA_CHROME_MEDIA_SESSION_CLIENT_H_

#include "base/no_destructor.h"
#include "content/public/browser/media_session_client.h"

class ChromeMediaSessionClient : public content::MediaSessionClient {
 public:
  ChromeMediaSessionClient(const ChromeMediaSessionClient&) = delete;
  ChromeMediaSessionClient& operator=(const ChromeMediaSessionClient&) = delete;

  static ChromeMediaSessionClient* GetInstance();

  bool ShouldHideMetadata(
      content::BrowserContext* browser_context) const override;

  std::u16string GetTitlePlaceholder() const override;
  std::u16string GetSourceTitlePlaceholder() const override;
  std::u16string GetArtistPlaceholder() const override;
  std::u16string GetAlbumPlaceholder() const override;
  SkBitmap GetThumbnailPlaceholder() const override;

 private:
  friend base::NoDestructor<ChromeMediaSessionClient>;

  ChromeMediaSessionClient() = default;
};

#endif  // CHROME_BROWSER_MEDIA_CHROME_MEDIA_SESSION_CLIENT_H_

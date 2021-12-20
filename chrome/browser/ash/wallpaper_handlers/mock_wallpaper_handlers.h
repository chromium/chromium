// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_MOCK_WALLPAPER_HANDLERS_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_MOCK_WALLPAPER_HANDLERS_H_

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace wallpaper_handlers {

// Fetcher that returns a dummy value for the number of photos in a user's
// Google Photos library. Used to avoid network requests in unit tests.
class MockGooglePhotosCountFetcher : public GooglePhotosCountFetcher {
 public:
  explicit MockGooglePhotosCountFetcher(Profile* profile);

  MockGooglePhotosCountFetcher(const MockGooglePhotosCountFetcher&) = delete;
  MockGooglePhotosCountFetcher& operator=(const MockGooglePhotosCountFetcher&) =
      delete;

  ~MockGooglePhotosCountFetcher() override;

  // GooglePhotosCountFetcher:
  using OnGooglePhotosCountFetched = ash::personalization_app::mojom::
      WallpaperProvider::FetchGooglePhotosCountCallback;
  MOCK_METHOD(void,
              AddCallbackAndStartIfNecessary,
              (OnGooglePhotosCountFetched callback),
              (override));
};

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_MOCK_WALLPAPER_HANDLERS_H_

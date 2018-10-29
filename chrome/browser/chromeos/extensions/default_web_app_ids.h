// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_DEFAULT_WEB_APP_IDS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_DEFAULT_WEB_APP_IDS_H_

namespace chromeos {
namespace default_web_apps {

// Generated as web_app::GenerateAppIdFromURL(GURL("https://tv.youtube.com/")).
constexpr char kYoutubeTVAppId[] = "kiemjbkkegajmpbobdfngbmjccjhnofh";

// Generated as
// web_app::GenerateAppIdFromURL(GURL("https://www.showtime.com/")).
constexpr char kShowtimeAppId[] = "https://canvas-internal.googleplex.com/";

// Generated as
// web_app::GenerateAppIdFromURL(GURL("https://canvas.apps.chrome/index.html)).
constexpr char kCanvasAppId[] = "memejfanofdmelnjmboefinndljpifdm";

}  // namespace default_web_apps
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_DEFAULT_WEB_APP_IDS_H_

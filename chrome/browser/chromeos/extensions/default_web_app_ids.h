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
constexpr char kShowtimeAppId[] = "eoccpgmpiempcflglfokeengliildkag";

// Generated as
// web_app::GenerateAppIdFromURL(GURL("https://canvas.apps.chrome/")).
constexpr char kCanvasAppId[] = "ieailfmhaghpphfffooibmlghaeopach";

// Generated as
// web_app::GenerateAppIdFromURL(GURL(
// "https://google.com/chromebook/whatsnew/embedded/")).
constexpr char kReleaseNotesAppId[] = "lddhblppcjmenljhdleiahjighahdcje";

// Generated as web_app::GenerateAppIdFromURL(GURL("chrome://settings/")).
constexpr char kSettingsAppId[] = "inogagmajamaleonmanpkpkkigmklfad";

// Generated as web_app::GenerateAppIdFromURL(GURL("chrome://os-settings/")).
constexpr char kOsSettingsAppId[] = "odknhmnlageboeamepcngndbggdpaobj";

// Generated as
// web_app::GenerateAppIdFromURL(GURL("https://news.google.com/?lfhs=2")).
constexpr char kGoogleNewsAppId[] = "kfgapjallbhpciobgmlhlhokknljkgho";

// Generated as
// web_app::GenerateAppIdFromURL(
//     GURL("https://www.google.com/maps/_/sw/tt-install.html")).
constexpr char kGoogleMapsAppId[] = "mnhkaebcjjhencmpkapnbdaogjamfbcj";

}  // namespace default_web_apps
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_DEFAULT_WEB_APP_IDS_H_

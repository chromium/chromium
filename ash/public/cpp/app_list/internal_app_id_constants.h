// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_INTERNAL_APP_ID_CONSTANTS_H_
#define ASH_PUBLIC_CPP_APP_LIST_INTERNAL_APP_ID_CONSTANTS_H_

namespace ash {

// App ids for internal apps, also used to identify the shelf item.
// Generated as
// crx_file::id_util::GenerateId("org.chromium.keyboardshortcuthelper").
constexpr char kInternalAppIdKeyboardShortcutViewer[] =
    "bhbpmkoclkgbgaefijcdgkfjghcmiijm";

// Generated as crx_file::id_util::GenerateId("org.chromium.settings_ui").
constexpr char kInternalAppIdSettings[] = "dhnmfjegnohoakobpikffnelcemaplkm";

// Generated as
// crx_file::id_util::GenerateId("org.chromium.continuous_reading"). This is an
// app placehoder for continuous reading in Chrome.
constexpr char kInternalAppIdContinueReading[] =
    "fbokpncipdhffndmljhhidahghagaonp";

// Generated as crx_file::id_util::GenerateId("org.chromium.camera").
constexpr char kInternalAppIdCamera[] = "iniodglblcgmngkgdipeiclkdjjpnlbn";

// Generated as crx_file::id_util::GenerateId("org.chromium.discover").
constexpr char kInternalAppIdDiscover[] = "pjdncmlmjhcebmcacdddfacepcjmfaoo";

// Generated as
// web_app::GenerateAppIdFromURL(GURL(
// "https://google.com/chromebook/whatsnew/embedded/")).
constexpr char kReleaseNotesAppId[] = "kddjchdmnnpakappplfnloipgcbioilo";

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_INTERNAL_APP_ID_CONSTANTS_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_MALL_URL_CONSTANTS_H_
#define ASH_WEBUI_MALL_URL_CONSTANTS_H_

class GURL;

namespace ash {
inline constexpr char kChromeUIMallHost[] = "mall";
inline constexpr char kChromeUIMallUrl[] = "chrome://mall";

// Returns the base URL for the Mall, to be embedded inside the SWA.
//
// This can be overridden for debugging purposes using a switch:
// --mall-url=https://www.example.com/
GURL GetMallBaseUrl();

}  // namespace ash

#endif  // ASH_WEBUI_MALL_URL_CONSTANTS_H_

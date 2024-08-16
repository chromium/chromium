// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_UI_URL_CONSTANTS_H_
#define ASH_WEBUI_BOCA_UI_URL_CONSTANTS_H_

namespace ash::boca {
// Boca App Host.
inline constexpr char kChromeBocaAppHost[] = "boca-app";
// Boca App untrusted host.
inline constexpr char kChromeBocaAppUntrustedURL[] =
    "chrome-untrusted://boca-app/";
// Boca App untrusted URL.
inline constexpr char kChromeBocaAppUntrustedIndexURL[] =
    "chrome-untrusted://boca-app/index.html";
}  // namespace ash::boca

#endif  // ASH_WEBUI_BOCA_UI_URL_CONSTANTS_H_

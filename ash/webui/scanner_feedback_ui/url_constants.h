// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SCANNER_FEEDBACK_UI_URL_CONSTANTS_H_
#define ASH_WEBUI_SCANNER_FEEDBACK_UI_URL_CONSTANTS_H_

#include <string_view>

namespace ash {

inline constexpr std::string_view kScannerFeedbackUntrustedUrl =
    "chrome-untrusted://scanner-feedback/";
inline constexpr std::string_view kScannerFeedbackUntrustedHost =
    "scanner-feedback";
inline constexpr std::string_view kScannerFeedbackScreenshotPrefix =
    "screenshots/";
inline constexpr std::string_view kScannerFeedbackScreenshotSuffix = ".jpg";

}  // namespace ash

#endif  // ASH_WEBUI_SCANNER_FEEDBACK_UI_URL_CONSTANTS_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/recorder_app_ui/model_constants.h"

namespace ash {

constexpr speech::LanguageCode kDefaultLanguageCode =
    speech::LanguageCode::kEnUs;

constexpr char kSummaryXsModelUuid[] = "3ecdce05-c36d-46fa-9f21-bad92521e872";
constexpr char kSummaryXxsModelUuid[] = "73caa678-45cb-4007-abb9-f04e431376da";
constexpr char kTitleSuggestionXsModelUuid[] =
    "2ba72641-25bc-4822-853e-49cf8710cffb";
constexpr char kTitleSuggestionXxsModelUuid[] =
    "1bdd5282-2d14-413c-bf43-9ea6d55c38a6";

const uint32_t kInputTokenLimit = 3072;

}  // namespace ash

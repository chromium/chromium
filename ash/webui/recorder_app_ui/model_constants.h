// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_RECORDER_APP_UI_MODEL_CONSTANTS_H_
#define ASH_WEBUI_RECORDER_APP_UI_MODEL_CONSTANTS_H_

#include <cstdint>

#include "components/soda/constants.h"

namespace ash {

// Default supported language of transcription, speaker label, and GenAI
// features.
extern const speech::LanguageCode kDefaultLanguageCode;

// On-device model DLC id.
extern const char kSummaryXsModelUuid[];
extern const char kSummaryXxsModelUuid[];
extern const char kTitleSuggestionXsModelUuid[];
extern const char kTitleSuggestionXxsModelUuid[];

// Maximum input token to GenAI models.
// TODO: b/358233121 - Separate the limit of XS and XXS model after XS models
// start support larger token size.
extern const uint32_t kInputTokenLimit;

}  // namespace ash

#endif  // ASH_WEBUI_RECORDER_APP_UI_MODEL_CONSTANTS_H_

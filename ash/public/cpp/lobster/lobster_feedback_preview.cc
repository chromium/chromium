// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/lobster/lobster_feedback_preview.h"

namespace ash {

LobsterFeedbackPreview::LobsterFeedbackPreview() {}

LobsterFeedbackPreview::LobsterFeedbackPreview(
    const LobsterFeedbackPreview& other) = default;

LobsterFeedbackPreview::~LobsterFeedbackPreview() = default;

LobsterFeedbackPreview::LobsterFeedbackPreview(
    const std::map<std::string, std::string>& fields,
    const std::string& image_bytes)
    : fields(fields), preview_image_bytes(image_bytes) {}

}  // namespace ash

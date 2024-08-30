// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOBSTER_LOBSTER_FEEDBACK_PREVIEW_H_
#define ASH_PUBLIC_CPP_LOBSTER_LOBSTER_FEEDBACK_PREVIEW_H_

#include <map>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "url/gurl.h"

namespace ash {

struct ASH_PUBLIC_EXPORT LobsterFeedbackPreview {
  std::map<std::string, std::string> fields;
  std::string preview_image_bytes;

  LobsterFeedbackPreview();
  LobsterFeedbackPreview(const LobsterFeedbackPreview& other);

  LobsterFeedbackPreview(const std::map<std::string, std::string>& fields,
                         const std::string& preview_image_bytes);

  ~LobsterFeedbackPreview();
};

using LobsterFeedbackPreviewResponse =
    base::expected<LobsterFeedbackPreview, std::string>;

using LobsterPreviewFeedbackCallback =
    base::OnceCallback<void(const LobsterFeedbackPreviewResponse&)>;

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOBSTER_LOBSTER_FEEDBACK_PREVIEW_H_

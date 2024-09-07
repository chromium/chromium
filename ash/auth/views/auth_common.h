// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTH_VIEWS_AUTH_COMMON_H_
#define ASH_AUTH_VIEWS_AUTH_COMMON_H_

#include "ash/style/typography.h"
#include "base/containers/enum_set.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"

namespace ash {

// The enums below are used in histograms, do not remove/renumber entries. If
// you're adding to any of these enums, update the corresponding enum listing in
// tools/metrics/histograms/metadata/ash/enums.xml:
enum class AuthInputType {
  kPassword = 0,
  kPin = 1,
  kFingerprint = 2,
  kMaxValue = kFingerprint
};

using AuthFactorSet = base::
    EnumSet<AuthInputType, AuthInputType::kPassword, AuthInputType::kMaxValue>;

// The text width is the kActiveSessionAuthViewWidthDp -
// 2 X 32 dp margin.
inline constexpr int kTextLineWidthDp = 322 - 2 * 32;
inline constexpr ui::ColorId kTextColorId = cros_tokens::kCrosSysOnSurface;
inline constexpr TypographyToken kTextFont = TypographyToken::kCrosAnnotation1;

}  // namespace ash
#endif  // ASH_AUTH_VIEWS_AUTH_COMMON_H_

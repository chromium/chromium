// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTH_VIEWS_AUTH_COMMON_H_
#define ASH_AUTH_VIEWS_AUTH_COMMON_H_

#include "base/containers/enum_set.h"

namespace ash {

// The enums below are used in histograms, do not remove/renumber entries. If
// you're adding to any of these enums, update the corresponding enum listing in
// tools/metrics/histograms/metadata/ash/enums.xml:
enum class AuthInputType { kPassword = 0, kPin = 1, kMaxValue = kPin };

using AuthFactorSet =
    base::EnumSet<AuthInputType, AuthInputType::kPassword, AuthInputType::kPin>;

}  // namespace ash
#endif  // ASH_AUTH_VIEWS_AUTH_COMMON_H_

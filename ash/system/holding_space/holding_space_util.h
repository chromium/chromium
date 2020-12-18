// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_

#include "base/strings/string16.h"

namespace views {
class Label;
}  // namespace views

namespace ash {
namespace holding_space_util {

// Enumeration of supported label styles.
enum class LabelStyle {
  kBadge,
  kBody,
  kChip,
  kHeader,
};

// Creates a label with optional `text` matching the specified `style`.
std::unique_ptr<views::Label> CreateLabel(
    LabelStyle style,
    const base::string16& text = base::string16());

}  // namespace holding_space_util
}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_

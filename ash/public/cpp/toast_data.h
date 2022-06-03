// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TOAST_DATA_H_
#define ASH_PUBLIC_CPP_TOAST_DATA_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

struct ASH_PUBLIC_EXPORT ToastData {
  // "|duration_ms| == -1" means the toast view should be displayed until the
  // dismiss button is clicked.
  static const int32_t kInfiniteDuration = -1;

  ToastData(std::string id,
            const std::u16string& text,
            int32_t duration_ms,
            const absl::optional<std::u16string>& dismiss_text,
            bool visible_on_lock_screen = false);
  ToastData(const ToastData& other);
  ~ToastData();

  std::string id;
  std::u16string text;
  int32_t duration_ms;
  absl::optional<std::u16string> dismiss_text;
  bool visible_on_lock_screen;
  bool is_managed = false;
  base::RepeatingClosure dismiss_callback;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TOAST_DATA_H_

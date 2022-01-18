// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_TOAST_DATA_H_
#define ASH_PUBLIC_CPP_SYSTEM_TOAST_DATA_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/system/toast_catalog.h"
#include "base/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

struct ASH_PUBLIC_EXPORT ToastData {
  // "|duration_ms| == -1" means the toast view should be displayed until the
  // dismiss button is clicked.
  static const int32_t kInfiniteDuration = -1;
  static const int32_t kDefaultToastDurationMs = 6 * 1000;

  ToastData(std::string id,
            ToastCatalogName catalog_name,
            const std::u16string& text,
            int32_t duration_ms = kDefaultToastDurationMs,
            bool visible_on_lock_screen = false,
            const absl::optional<std::u16string>& dismiss_text = absl::nullopt);
  ToastData(const ToastData& other);
  ~ToastData();

  std::string id;
  ToastCatalogName catalog_name;
  std::u16string text;
  int32_t duration_ms;
  bool visible_on_lock_screen;
  absl::optional<std::u16string> dismiss_text;
  bool is_managed = false;
  base::RepeatingClosure dismiss_callback;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_TOAST_DATA_H_

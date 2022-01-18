// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/system/toast_data.h"

#include <utility>

namespace ash {

ToastData::ToastData(std::string id,
                     ToastCatalogName catalog_name,
                     const std::u16string& text,
                     int32_t duration_ms,
                     bool visible_on_lock_screen,
                     const absl::optional<std::u16string>& dismiss_text)
    : id(std::move(id)),
      catalog_name(catalog_name),
      text(text),
      duration_ms(duration_ms),
      visible_on_lock_screen(visible_on_lock_screen),
      dismiss_text(dismiss_text) {}

ToastData::ToastData(const ToastData& other) = default;

ToastData::~ToastData() = default;

}  // namespace ash

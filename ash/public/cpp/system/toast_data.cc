// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/system/toast_data.h"

#include <utility>

#include "base/time/time.h"

namespace ash {

ToastData::ToastData(std::string id,
                     ToastCatalogName catalog_name,
                     const std::u16string& text,
                     base::TimeDelta duration,
                     bool visible_on_lock_screen,
                     const absl::optional<std::u16string>& dismiss_text)
    : id(std::move(id)),
      catalog_name(catalog_name),
      text(text),
      duration(std::max(duration, kMinimumDuration)),
      visible_on_lock_screen(visible_on_lock_screen),
      dismiss_text(dismiss_text) {}

ToastData::ToastData(const ToastData& other) = default;

ToastData::~ToastData() = default;

}  // namespace ash

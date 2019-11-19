// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/toast_data.h"

#include <utility>

namespace ash {

ToastData::ToastData(std::string id,
                     const base::string16& text,
                     int32_t duration_ms,
                     const base::Optional<base::string16>& dismiss_text,
                     bool visible_on_lock_screen)
    : id(std::move(id)),
      text(text),
      duration_ms(duration_ms),
      dismiss_text(dismiss_text),
      visible_on_lock_screen(visible_on_lock_screen) {}

ToastData::ToastData(const ToastData& other) = default;

ToastData::~ToastData() = default;

}  // namespace ash

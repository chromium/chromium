// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/system/toast_data.h"

#include <utility>

#include "ash/strings/grit/ash_strings.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

ToastData::ToastData(std::string id,
                     ToastCatalogName catalog_name,
                     const std::u16string& text,
                     base::TimeDelta duration,
                     bool visible_on_lock_screen,
                     bool has_dismiss_button)
    : id(std::move(id)),
      catalog_name(catalog_name),
      text(text),
      duration(std::max(duration, kMinimumDuration)),
      visible_on_lock_screen(visible_on_lock_screen),
      time_created(base::TimeTicks::Now()) {
  if (has_dismiss_button) {
    button_type = ButtonType::kTextButton;
    button_text = l10n_util::GetStringUTF16(IDS_ASH_TOAST_DISMISS_BUTTON);
  }
}

ToastData::ToastData(ToastData&& other) = default;
ToastData& ToastData::operator=(ToastData&& other) = default;

ToastData::~ToastData() {
  // The toast can get cancelled before it shows, so we only want to run
  // `expired_callback` if the toast actually did show.
  if (!time_start_showing.is_null() && expired_callback)
    std::move(expired_callback).Run();
}

}  // namespace ash

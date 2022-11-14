// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_TOAST_DATA_H_
#define ASH_PUBLIC_CPP_SYSTEM_TOAST_DATA_H_

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/callback.h"
#include "base/time/time.h"

namespace ash {

struct ASH_PUBLIC_EXPORT ToastData {
  // A `ToastData` with a `kInfiniteDuration` duration will be displayed until
  // the dismiss button on the toast is clicked.
  static constexpr base::TimeDelta kInfiniteDuration = base::TimeDelta::Max();

  // The default duration that a toast will be shown before it is automatically
  // dismissed.
  static constexpr base::TimeDelta kDefaultToastDuration = base::Seconds(6);

  // Minimum duration for a toast to be visible before it is automatically
  // dismissed.
  static constexpr base::TimeDelta kMinimumDuration = base::Milliseconds(200);

  // Creates a `ToastData` which is used to configure how a toast behaves when
  // shown. The toast `duration` is how long the toast will be shown before it
  // is automatically dismissed. The `duration` will be set to
  // `kMinimumDuration` for any value provided that is smaller than
  // `kMinimumDuration`. To disable automatically dismissing the toast, set the
  // `duration` to `kInfiniteDuration`. If `has_dismiss_button` is true, it will
  // use the default dismiss text unless a non-empty `custom_dismiss_text` is
  // given.
  ToastData(std::string id,
            ToastCatalogName catalog_name,
            const std::u16string& text,
            base::TimeDelta duration = kDefaultToastDuration,
            bool visible_on_lock_screen = false,
            bool has_dismiss_button = false,
            const std::u16string& custom_dismiss_text = std::u16string());
  ToastData(ToastData&& other);
  ToastData& operator=(ToastData&& other);
  ~ToastData();

  std::string id;
  ToastCatalogName catalog_name;
  std::u16string text;
  base::TimeDelta duration;
  bool visible_on_lock_screen;
  std::u16string dismiss_text;
  bool is_managed = false;
  bool persist_on_hover = false;
  bool show_on_all_root_windows = false;
  // TODO(b/259100049): We should turn this into a `OnceClosure`.
  base::RepeatingClosure dismiss_callback;
  base::OnceClosure expired_callback;
  base::TimeTicks time_created;
  base::TimeTicks time_start_showing;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_TOAST_DATA_H_

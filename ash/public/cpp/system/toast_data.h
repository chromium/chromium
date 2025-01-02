// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_TOAST_DATA_H_
#define ASH_PUBLIC_CPP_SYSTEM_TOAST_DATA_H_

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/time/time.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

struct ASH_PUBLIC_EXPORT ToastData {
  // A `ToastData` with a `kInfiniteDuration` duration will be displayed for 30
  // minutes or until the dismiss button on the toast is clicked. An actual
  // infinite duration is not used to prevent cases where the toast won't be
  // dismissable e.g. on kiosk mode that limits user input.
  static constexpr base::TimeDelta kInfiniteDuration = base::Minutes(30);

  // The default duration that a toast will be shown before it is automatically
  // dismissed.
  static constexpr base::TimeDelta kDefaultToastDuration = base::Seconds(6);

  // Minimum duration for a toast to be visible before it is automatically
  // dismissed.
  static constexpr base::TimeDelta kMinimumDuration = base::Milliseconds(200);

  // Type of button to show next to the toast's body text.
  enum class ButtonType {
    // No button.
    kNone,
    // Text button which will run `button_callback` and dismiss the toast when
    // pressed.
    kTextButton,
    // Icon button which will run `button_callback` and dismiss the toast when
    // pressed.
    kIconButton,
  };

  // Creates a `ToastData` which is used to configure how a toast behaves when
  // shown. The toast `duration` is how long the toast will be shown before it
  // is automatically dismissed. The `duration` will be set to
  // `kMinimumDuration` for any value provided that is smaller than
  // `kMinimumDuration`. To disable automatically dismissing the toast, set the
  // `duration` to `kInfiniteDuration`. If `has_dismiss_button` is true, the
  // toast will have a dismiss button with the default dismiss text.
  ToastData(std::string id,
            ToastCatalogName catalog_name,
            const std::u16string& text,
            base::TimeDelta duration = kDefaultToastDuration,
            bool visible_on_lock_screen = false,
            bool has_dismiss_button = false);
  ToastData(ToastData&& other);
  ToastData& operator=(ToastData&& other);
  ~ToastData();

  std::string id;
  ToastCatalogName catalog_name;
  std::u16string text;
  base::TimeDelta duration;
  bool visible_on_lock_screen;
  bool persist_on_hover = false;
  bool show_on_all_root_windows = false;
  bool activatable = false;
  ButtonType button_type = ButtonType::kNone;
  // TODO(b/259100049): We should turn this into a `OnceClosure`.
  base::RepeatingClosure button_callback = base::DoNothing();
  std::u16string button_text;
  // RAW_PTR_EXCLUSION: Never allocated by PartitionAlloc (always points to a
  // global), so there is no benefit to using a raw_ptr, only cost.
  RAW_PTR_EXCLUSION const gfx::VectorIcon* button_icon =
      &gfx::VectorIcon::EmptyIcon();
  RAW_PTR_EXCLUSION const gfx::VectorIcon* leading_icon =
      &gfx::VectorIcon::EmptyIcon();
  base::OnceClosure expired_callback;
  base::TimeTicks time_created;
  base::TimeTicks time_start_showing;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_TOAST_DATA_H_

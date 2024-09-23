// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_MOJOM_IME_MOJOM_TRAITS_H_
#define ASH_COMPONENTS_ARC_MOJOM_IME_MOJOM_TRAITS_H_

#include "ash/components/arc/mojom/ime.mojom-shared.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/events/event.h"

namespace mojo {

template <>
struct EnumTraits<arc::mojom::TextInputType, ui::TextInputType> {
  using MojoType = arc::mojom::TextInputType;

  // The two enum types are similar, but intentionally made not identical.
  // We cannot force them to be in sync. If we do, updates in ui::TextInputType
  // must always be propagated to the mojom::TextInputType mojo definition in
  // ARC container side, which is in a different repository than Chromium.
  // We don't want such dependency.
  //
  // That's why we need a lengthy switch statement instead of static_cast
  // guarded by a static assert on the two enums to be in sync.

  static MojoType ToMojom(ui::TextInputType input) {
    switch (input) {
      case ui::TEXT_INPUT_TYPE_NONE:
        return MojoType::NONE;
      case ui::TEXT_INPUT_TYPE_TEXT:
        return MojoType::TEXT;
      case ui::TEXT_INPUT_TYPE_PASSWORD:
        return MojoType::PASSWORD;
      case ui::TEXT_INPUT_TYPE_SEARCH:
        return MojoType::SEARCH;
      case ui::TEXT_INPUT_TYPE_EMAIL:
        return MojoType::EMAIL;
      case ui::TEXT_INPUT_TYPE_NUMBER:
        return MojoType::NUMBER;
      case ui::TEXT_INPUT_TYPE_TELEPHONE:
        return MojoType::TELEPHONE;
      case ui::TEXT_INPUT_TYPE_URL:
        return MojoType::URL;
      case ui::TEXT_INPUT_TYPE_DATE:
        return MojoType::DATE;
      case ui::TEXT_INPUT_TYPE_DATE_TIME:
        return MojoType::DATETIME;
      case ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL:
        return MojoType::DATETIME;
      case ui::TEXT_INPUT_TYPE_MONTH:
        return MojoType::DATE;
      case ui::TEXT_INPUT_TYPE_TIME:
        return MojoType::TIME;
      case ui::TEXT_INPUT_TYPE_WEEK:
        return MojoType::DATE;
      case ui::TEXT_INPUT_TYPE_TEXT_AREA:
        return MojoType::TEXT;
      case ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE:
        return MojoType::TEXT;
      case ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD:
        return MojoType::DATETIME;
      case ui::TEXT_INPUT_TYPE_NULL:
        return MojoType::NONE;
    }
    NOTREACHED();
  }

  static bool FromMojom(MojoType input, ui::TextInputType* out) {
    switch (input) {
      case MojoType::NONE:
        *out = ui::TEXT_INPUT_TYPE_NONE;
        return true;
      case MojoType::TEXT:
        *out = ui::TEXT_INPUT_TYPE_TEXT;
        return true;
      case MojoType::PASSWORD:
        *out = ui::TEXT_INPUT_TYPE_PASSWORD;
        return true;
      case MojoType::SEARCH:
        *out = ui::TEXT_INPUT_TYPE_SEARCH;
        return true;
      case MojoType::EMAIL:
        *out = ui::TEXT_INPUT_TYPE_EMAIL;
        return true;
      case MojoType::NUMBER:
        *out = ui::TEXT_INPUT_TYPE_NUMBER;
        return true;
      case MojoType::TELEPHONE:
        *out = ui::TEXT_INPUT_TYPE_TELEPHONE;
        return true;
      case MojoType::URL:
        *out = ui::TEXT_INPUT_TYPE_URL;
        return true;
      case MojoType::DATE:
        *out = ui::TEXT_INPUT_TYPE_DATE;
        return true;
      case MojoType::TIME:
        *out = ui::TEXT_INPUT_TYPE_TIME;
        return true;
      case MojoType::DATETIME:
        *out = ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL;
        return true;
      case MojoType::ANDROID_NULL:
        *out = ui::TEXT_INPUT_TYPE_NULL;
        return true;
    }
    NOTREACHED();
  }
};

using KeyEventUniquePtr = std::unique_ptr<ui::KeyEvent>;
template <>
struct StructTraits<arc::mojom::KeyEventDataDataView, KeyEventUniquePtr> {
  static bool pressed(const KeyEventUniquePtr& key_event) {
    return key_event->type() == ui::EventType::kKeyPressed;
  }
  static int32_t key_code(const KeyEventUniquePtr& key_event) {
    return key_event->key_code();
  }
  static bool is_shift_down(const KeyEventUniquePtr& key_event) {
    return key_event->IsShiftDown();
  }
  static bool is_control_down(const KeyEventUniquePtr& key_event) {
    return key_event->IsControlDown();
  }
  static bool is_alt_down(const KeyEventUniquePtr& key_event) {
    return key_event->IsAltDown();
  }
  static bool is_capslock_on(const KeyEventUniquePtr& key_event) {
    return key_event->IsCapsLockOn();
  }
  static int32_t scan_code(const KeyEventUniquePtr& key_event) {
    return key_event->scan_code();
  }
  static bool is_alt_gr_down(const KeyEventUniquePtr& key_event) {
    return key_event->IsAltGrDown();
  }
  static bool is_repeat(const KeyEventUniquePtr& key_event) {
    return key_event->is_repeat();
  }

  static bool Read(arc::mojom::KeyEventDataDataView data,
                   KeyEventUniquePtr* out);
};

}  // namespace mojo

#endif  // ASH_COMPONENTS_ARC_MOJOM_IME_MOJOM_TRAITS_H_

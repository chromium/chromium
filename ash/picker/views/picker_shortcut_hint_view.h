// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SHORTCUT_HINT_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SHORTCUT_HINT_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/picker/picker_search_result.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class ASH_EXPORT PickerShortcutHintView : public views::View {
  METADATA_HEADER(PickerShortcutHintView, views::View)

 public:
  explicit PickerShortcutHintView(PickerCapsLockResult::Shortcut);
  PickerShortcutHintView(const PickerShortcutHintView&) = delete;
  PickerShortcutHintView& operator=(const PickerShortcutHintView&) = delete;
  ~PickerShortcutHintView() override;

  const std::u16string& GetShortcutText() const;

 private:
  std::u16string shortcut_text_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SHORTCUT_HINT_VIEW_H_

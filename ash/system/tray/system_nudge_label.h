// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_NUDGE_LABEL_H_
#define ASH_SYSTEM_TRAY_SYSTEM_NUDGE_LABEL_H_

#include "base/containers/flat_map.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/view.h"

namespace ash {

// A label for system nudges which automatically updates text color on theme
// changes and supports inline embedding of custom views.
class SystemNudgeLabel : public views::View {
 public:
  SystemNudgeLabel(std::u16string text, int fixed_width);

  SystemNudgeLabel(const SystemNudgeLabel&) = delete;
  SystemNudgeLabel& operator=(const SystemNudgeLabel&) = delete;

  ~SystemNudgeLabel() override;

  // Passes ownership of a custom view, so that the system nudge can have a
  // custom view within its text located at the `offset`.
  void AddCustomView(std::unique_ptr<View> custom_view, size_t offset);

  const std::u16string& GetText() const;

  void set_font_size_delta(int font_size_delta) {
    font_size_delta_ = font_size_delta;
  }

  // views::View:
  void OnThemeChanged() override;

 private:
  views::StyledLabel* const styled_label_;
  base::flat_map<size_t, views::StyledLabel::RangeStyleInfo>
      custom_view_styles_by_offset_;
  int font_size_delta_ = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_NUDGE_LABEL_H_

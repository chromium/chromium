// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_KNOB_SWITCH_H_
#define ASH_STYLE_KNOB_SWITCH_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace ash {

class ASH_EXPORT KnobSwitch : public views::Button {
 public:
  METADATA_HEADER(KnobSwitch);

  using Callback = base::RepeatingCallback<void(bool selected)>;

  explicit KnobSwitch(Callback switch_callback = Callback());
  KnobSwitch(const KnobSwitch&) = delete;
  KnobSwitch& operator=(const KnobSwitch&) = delete;
  ~KnobSwitch() override;

  void SetSelected(bool selected);
  bool selected() const { return selected_; }

  // views::View:
  void Layout() override;

 private:
  // views::Button:
  void StateChanged(ButtonState old_state) override;
  void NotifyClick(const ui::Event& event) override;

  Callback switch_callback_;

  // Owned by switch view.
  views::View* track_ = nullptr;
  // Owned by tracker view.
  views::View* knob_ = nullptr;

  bool selected_ = false;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, KnobSwitch, views::Button)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::KnobSwitch)

#endif  // ASH_STYLE_KNOB_SWITCH_H_

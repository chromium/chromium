// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_PANEL_WIDGET_H_
#define ASH_SYSTEM_MAHI_MAHI_PANEL_WIDGET_H_

#include "ash/ash_export.h"
#include "ui/views/widget/widget.h"

namespace views {
class UniqueWidgetPtr;
}  // namespace views

namespace ash {

// The widget that contains the Mahi panel.
// TODO(b/319329379): Use this class in `CreatePanelWidget()` when resizing and
// closing capability is added.
class ASH_EXPORT MahiPanelWidget : public views::Widget {
 public:
  // Creates the Mahi panel widget within the display with `display_id`.
  static views::UniqueWidgetPtr CreatePanelWidget(int64_t display_id);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_MAHI_PANEL_WIDGET_H_

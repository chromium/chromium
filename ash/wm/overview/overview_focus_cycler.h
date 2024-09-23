// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_FOCUS_CYCLER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_FOCUS_CYCLER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"

namespace views {
class Widget;
class View;
}  // namespace views

namespace ash {
class OverviewSession;

class ASH_EXPORT OverviewFocusCycler {
 public:
  explicit OverviewFocusCycler(OverviewSession* overview_session);
  OverviewFocusCycler(const OverviewFocusCycler&) = delete;
  OverviewFocusCycler& operator=(const OverviewFocusCycler&) = delete;
  ~OverviewFocusCycler();

  // Moves the focus ring to the next traversable view. Rotates focus to the
  // next traversable widget if necessary.
  void MoveFocus(bool reverse);

  bool AcceptSelection();

  // Returns the current overview UI focused view if there is one.
  views::View* GetOverviewFocusedView();

  void UpdateAccessibilityFocus();

 private:
  // Gets the list of traversable widgets in overview.
  std::vector<views::Widget*> GetTraversableWidgets(
      bool for_accessibility) const;

  // The overview session which owns this object. Guaranteed to be non-null for
  // the lifetime of `this`.
  const raw_ptr<OverviewSession> overview_session_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_FOCUS_CYCLER_H_

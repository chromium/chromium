// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FOCUS_CYCLER_H_
#define ASH_FOCUS_CYCLER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

// This class handles moving focus between a set of widgets and the main browser
// window.
class ASH_EXPORT FocusCycler {
 public:
  enum Direction { FORWARD, BACKWARD };

  FocusCycler();

  FocusCycler(const FocusCycler&) = delete;
  FocusCycler& operator=(const FocusCycler&) = delete;

  ~FocusCycler();

  // Returns the widget the FocusCycler is attempting to activate or NULL if
  // FocusCycler is not activating any widgets.
  const views::Widget* widget_activating() const { return widget_activating_; }

  // Add a widget to the focus cycle. The widget needs to have an
  // AccessiblePaneView as the content view.
  void AddWidget(views::Widget* widget);

  // Remove a widget from the focus cycle.
  void RemoveWidget(views::Widget* widget);

  // Move focus to the next widget.
  void RotateFocus(Direction direction, bool move_onto_next_widget = false);

  // Moves focus the specified widget. Returns true if the widget was activated.
  bool FocusWidget(views::Widget* widget);

  // Find a widget that matches the criteria given by |callback|
  // in the cycle list.
  views::Widget* FindWidget(
      base::RepeatingCallback<bool(views::Widget*)> callback);

 private:
  std::vector<raw_ptr<views::Widget, VectorExperimental>> widgets_;

  // See description above getter.
  raw_ptr<views::Widget> widget_activating_;
};

}  // namespace ash

#endif  // ASH_FOCUS_CYCLER_H_

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_NUDGE_H_
#define ASH_SYSTEM_TRAY_SYSTEM_NUDGE_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_observer.h"
#include "base/scoped_observation.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
class View;
}  // namespace views

namespace ash {

// Creates and manages the nudge widget and its contents view for a contextual
// system nudge. The nudge displays an icon and a label view in a shelf-colored
// system bubble with rounded corners.
class ASH_EXPORT SystemNudge : public ShelfObserver {
 public:
  SystemNudge(const std::string& name,
              int icon_size,
              int icon_label_spacing,
              int nudge_padding);
  SystemNudge(const SystemNudge&) = delete;
  SystemNudge& operator=(const SystemNudge&) = delete;
  ~SystemNudge() override;

  // ShelfObserver:
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;
  void OnHotseatStateChanged(HotseatState old_state,
                             HotseatState new_state) override;

  // Displays the nudge.
  void Show();

  // Closes the nudge.
  void Close();

  views::Widget* widget() { return widget_.get(); }

 protected:
  // Each SystemNudge subclass must override these methods to customize
  // their nudge by creating a label and getting an icon specific to the feature
  // being nudged. These will be called only when needed by Show().

  // Creates and initializes a view representing the label for the nudge.
  // Returns a views::View in case the subclass wishes to creates a StyledLabel,
  // Label, or something else entirely.
  virtual std::unique_ptr<views::View> CreateLabelView() const = 0;

  // Gets the VectorIcon shown to the side of the label for the nudge.
  virtual const gfx::VectorIcon& GetIcon() const = 0;

  // Gets the string to announce for accessibility. This will be used if a
  // screen reader is enabled when the view is shown.
  virtual std::u16string GetAccessibilityText() const = 0;

 private:
  class SystemNudgeView;

  struct SystemNudgeParams {
    // The name for the widget.
    std::string name;
    // The size of the icon.
    int icon_size;
    // The size of the space between icon and label.
    int icon_label_spacing;
    // The padding which separates the nudge's border with its inner contents.
    int nudge_padding;
  };

  // Calculate and set widget bounds based on a fixed width and a variable
  // height to correctly fit the label contents.
  void CalculateAndSetWidgetBounds();

  views::UniqueWidgetPtr widget_;

  SystemNudgeView* nudge_view_ = nullptr;  // not_owned

  aura::Window* const root_window_;

  SystemNudgeParams params_;

  base::ScopedObservation<Shelf, ShelfObserver> shelf_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_NUDGE_H_

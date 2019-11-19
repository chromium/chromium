// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_DETAILED_VIEW_DELEGATE_H_
#define ASH_SYSTEM_TRAY_DETAILED_VIEW_DELEGATE_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class Button;
class ButtonListener;
class Separator;
class View;
}  // namespace views

namespace ash {

class HoverHighlightView;
class TriView;
class UnifiedSystemTrayController;
class ViewClickListener;

// A delegate of TrayDetailedView that handles bubble related actions e.g.
// transition to the main view, closing the bubble, etc.
class ASH_EXPORT DetailedViewDelegate {
 public:
  explicit DetailedViewDelegate(UnifiedSystemTrayController* tray_controller);
  virtual ~DetailedViewDelegate();

  // Transition to the main view from the detailed view. |restore_focus| is true
  // if the title row has keyboard focus before transition. If so, the main view
  // should focus on the corresponding element of the detailed view.
  virtual void TransitionToMainView(bool restore_focus);

  // Close the bubble that contains the detailed view.
  virtual void CloseBubble();

  // Get the background color of the detailed view.
  virtual SkColor GetBackgroundColor();

  // Return true if overflow indicator of ScrollView is enabled.
  virtual bool IsOverflowIndicatorEnabled() const;

  // Return TriView used for the title row. It should have title label of
  // |string_id| in CENTER. TrayDetailedView will calls CreateBackButton() and
  // adds the returned view to START.
  virtual TriView* CreateTitleRow(int string_id);

  // Return the separator used between the title row and the contents. Caller
  // takes ownership of the returned view.
  virtual views::View* CreateTitleSeparator();

  // Configure a |view| to have a visible separator below.
  virtual void ShowStickyHeaderSeparator(views::View* view,
                                         bool show_separator);

  // Create a horizontal separator line to be drawn between rows in a detailed
  // view above the sub-header rows. Caller takes ownership of the returned
  // view.
  virtual views::Separator* CreateListSubHeaderSeparator();

  // Return a targetable row containing |icon| and |text|. Caller takes
  // ownership of the returned view.
  virtual HoverHighlightView* CreateScrollListItem(ViewClickListener* listener,
                                                   const gfx::VectorIcon& icon,
                                                   const base::string16& text);

  // Return the back button used in the title row. Caller takes ownership of the
  // returned view.
  virtual views::Button* CreateBackButton(views::ButtonListener* listener);

  // Return the info button used in the title row. Caller takes ownership of the
  // returned view.
  virtual views::Button* CreateInfoButton(views::ButtonListener* listener,
                                          int info_accessible_name_id);

  // Return the settings button used in the title row. Caller takes ownership of
  // the returned view.
  virtual views::Button* CreateSettingsButton(views::ButtonListener* listener,
                                              int setting_accessible_name_id);

  // Return the help button used in the title row. Caller takes ownership of the
  // returned view.
  virtual views::Button* CreateHelpButton(views::ButtonListener* listener);

 private:
  UnifiedSystemTrayController* const tray_controller_;

  DISALLOW_COPY_AND_ASSIGN(DetailedViewDelegate);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_DETAILED_VIEW_DELEGATE_H_

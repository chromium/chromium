// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_DETAILED_VIEW_H_
#define ASH_SYSTEM_TRAY_TRAY_DETAILED_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/view_click_listener.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class BoxLayout;
class Button;
class ProgressBar;
class ScrollView;
class Separator;
}  // namespace views

namespace ash {

class DetailedViewDelegate;
class HoverHighlightView;
class ScrollBorder;
class TriView;

class ASH_EXPORT TrayDetailedView : public views::View,
                                    public ViewClickListener,
                                    public views::ButtonListener {
 public:
  explicit TrayDetailedView(DetailedViewDelegate* delegate);
  ~TrayDetailedView() override;

  // ViewClickListener:
  // Don't override this --- override HandleViewClicked.
  void OnViewClicked(views::View* sender) final;

  // views::ButtonListener:
  // Don't override this --- override HandleButtonPressed.
  void ButtonPressed(views::Button* sender, const ui::Event& event) final;

 protected:
  // views::View:
  void Layout() override;
  int GetHeightForWidth(int width) const override;
  const char* GetClassName() const override;

  // Exposes the layout manager of this view to give control to subclasses.
  views::BoxLayout* box_layout() { return box_layout_; }

  // Creates the row containing the back button and title. For material design
  // this appears at the top of the view, for non-material design it appears
  // at the bottom.
  void CreateTitleRow(int string_id);

  // Creates a scrollable list. The list has a border at the bottom if there is
  // any other view between the list and the footer row at the bottom.
  void CreateScrollableList();

  // Adds a targetable row to |scroll_content_| containing |icon| and |text|.
  HoverHighlightView* AddScrollListItem(const gfx::VectorIcon& icon,
                                        const base::string16& text);

  // Add a child view to the scroll list.
  void AddScrollListChild(std::unique_ptr<views::View> child);

  // Adds a targetable row to |scroll_content_| containing |icon|, |text|, and a
  // checkbox. |checked| determines whether the checkbox is checked or not.
  // |enterprise_managed| determines whether or not there will be an enterprise
  // managed icon for that item.
  HoverHighlightView* AddScrollListCheckableItem(
      const gfx::VectorIcon& icon,
      const base::string16& text,
      bool checked,
      bool enterprise_managed = false);

  // Adds a targetable row to |scroll_content_| containing |text| and a
  // checkbox. |checked| determines whether the checkbox is checked or not.
  // |enterprise_managed| determines whether or not there will be an enterprise
  // managed icon for that item.
  HoverHighlightView* AddScrollListCheckableItem(
      const base::string16& text,
      bool checked,
      bool enterprise_managed = false);

  // Adds connected sub label to the |view| with appropriate style and updates
  // accessibility label.
  void SetupConnectedScrollListItem(HoverHighlightView* view);

  // Adds connected sub label with the device's battery percentage to the |view|
  // with appropriate style and updates accessibility label.
  void SetupConnectedScrollListItem(HoverHighlightView* view,
                                    base::Optional<uint8_t> battery_percentage);

  // Adds connecting sub label to the |view| with appropriate style and updates
  // accessibility label.
  void SetupConnectingScrollListItem(HoverHighlightView* view);

  // Adds a sticky sub header to |scroll_content_| containing |icon| and a text
  // represented by |text_id| resource id.
  TriView* AddScrollListSubHeader(const gfx::VectorIcon& icon, int text_id);

  // Adds a sticky sub header to |scroll_content_| containing a text represented
  // by |text_id| resource id.
  TriView* AddScrollListSubHeader(int text_id);

  // Removes (and destroys) all child views.
  void Reset();

  // Shows or hides the progress bar below the title row. It occupies the same
  // space as the separator, so when shown the separator is hidden. If
  // |progress_bar_| doesn't already exist it will be created.
  void ShowProgress(double value, bool visible);

  // Helper functions which create and return the settings and help buttons,
  // respectively, used in the material design top-most header row. The caller
  // assumes ownership of the returned buttons.
  views::Button* CreateInfoButton(int info_accessible_name_id);
  views::Button* CreateSettingsButton(int setting_accessible_name_id);
  views::Button* CreateHelpButton();

  // Create a horizontal separator line to be drawn between rows in a detailed
  // view above the sub-header rows. Caller takes ownership of the returned
  // view.
  views::Separator* CreateListSubHeaderSeparator();

  // Closes the bubble that contains the detailed view.
  void CloseBubble();

  TriView* tri_view() { return tri_view_; }
  views::ScrollView* scroller() const { return scroller_; }
  views::View* scroll_content() const { return scroll_content_; }

 private:
  friend class TrayDetailedViewTest;

  // Overridden to handle clicks on subclass-specific views.
  virtual void HandleViewClicked(views::View* view);

  // Overridden to handle button presses on subclass-specific buttons.
  virtual void HandleButtonPressed(views::Button* sender,
                                   const ui::Event& event);

  // Creates and adds subclass-specific buttons to the title row.
  virtual void CreateExtraTitleRowButtons();

  // Transition to main view from detailed view.
  void TransitionToMainView();

  DetailedViewDelegate* const delegate_;
  views::BoxLayout* box_layout_ = nullptr;
  views::ScrollView* scroller_ = nullptr;
  views::View* scroll_content_ = nullptr;
  views::ProgressBar* progress_bar_ = nullptr;

  ScrollBorder* scroll_border_ = nullptr;  // Weak reference

  // The container view for the top-most title row in material design.
  TriView* tri_view_ = nullptr;

  // The back button that appears in the material design title row. Not owned.
  views::Button* back_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TrayDetailedView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_DETAILED_VIEW_H_

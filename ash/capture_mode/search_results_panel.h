// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_SEARCH_RESULTS_PANEL_H_
#define ASH_CAPTURE_MODE_SEARCH_RESULTS_PANEL_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ash/wm/system_panel_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/display/display_observer.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace views {
class Button;
class Textfield;
}  // namespace views

namespace ash {

class AshWebView;
class SunfishSearchBoxView;

// Container for the search results view and other UI such as the search box,
// close button, etc.
class ASH_EXPORT SearchResultsPanel : public SystemPanelView,
                                      public display::DisplayObserver,
                                      public views::FocusChangeListener {
  METADATA_HEADER(SearchResultsPanel, SystemPanelView)

 public:
  SearchResultsPanel();
  SearchResultsPanel(const SearchResultsPanel&) = delete;
  SearchResultsPanel& operator=(const SearchResultsPanel&) = delete;
  ~SearchResultsPanel() override;

  static views::UniqueWidgetPtr CreateWidget(aura::Window* root,
                                             bool is_active);

  AshWebView* search_results_view() const { return search_results_view_; }
  views::Button* close_button() const { return close_button_; }

  views::Textfield* GetSearchBoxTextfield() const;

  std::vector<CaptureModeSessionFocusCycler::HighlightableView*>
  GetHighlightableItems() const;

  // Sets the search box URL, image thumbnail, and text.
  virtual void Navigate(const GURL& url);
  virtual void SetSearchBoxImage(const gfx::ImageSkia& image);
  void SetSearchBoxText(const std::u16string& text);

  // Refreshes the panel z-order. If `new_root` is not null, capture mode
  // session is active and will be used to determine the panel root. Else the
  // panel will be re-stacked on its native window's root window.
  void RefreshStackingOrder(aura::Window* new_root);

  // Returns true if the `CaptureModeSessionFocusCycler::HighlightHelper` for
  // this view has focus, otherwise returns false.
  bool IsTextfieldPseudoFocused() const;

  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // SystemPanelView:
  bool HasFocus() const override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(View* focused_before, View* focused_now) override;
  void OnDidChangeFocus(View* focused_before, View* focused_now) override;

 private:
  void OnCloseButtonPressed();

  // Refreshes the panel bounds to fit within the display work area. Note the
  // captured region and panel root and display must be updated prior to this.
  void RefreshPanelBounds();

  // Owned by the views hierarchy.
  raw_ptr<SunfishSearchBoxView> search_box_view_ = nullptr;
  raw_ptr<AshWebView> search_results_view_;
  raw_ptr<views::Button> close_button_;

  // Observes display and metrics changes.
  display::ScopedDisplayObserver display_observer_{this};

  base::WeakPtrFactory<SearchResultsPanel> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_SEARCH_RESULTS_PANEL_H_

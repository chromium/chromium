// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_SEARCH_RESULTS_PANEL_H_
#define ASH_CAPTURE_MODE_SEARCH_RESULTS_PANEL_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/system_panel_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
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
class ASH_EXPORT SearchResultsPanel : public SystemPanelView {
  METADATA_HEADER(SearchResultsPanel, SystemPanelView)

 public:
  SearchResultsPanel();
  SearchResultsPanel(const SearchResultsPanel&) = delete;
  SearchResultsPanel& operator=(const SearchResultsPanel&) = delete;
  ~SearchResultsPanel() override;

  static views::UniqueWidgetPtr CreateWidget(aura::Window* const root,
                                             const gfx::Rect& bounds);

  AshWebView* search_results_view() const { return search_results_view_; }
  views::Button* close_button() const { return close_button_; }

  views::Textfield* GetSearchBoxTextfield() const;

  // Sets the search box URL, image thumbnail, and text.
  virtual void Navigate(const GURL& url);
  virtual void SetSearchBoxImage(const gfx::ImageSkia& image);
  void SetSearchBoxText(const std::u16string& text);

  // SystemPanelView:
  bool HasFocus() const override;

 private:
  void OnCloseButtonPressed();

  // Owned by the views hierarchy.
  raw_ptr<SunfishSearchBoxView> search_box_view_;
  raw_ptr<AshWebView> search_results_view_;
  raw_ptr<views::Button> close_button_;

  base::WeakPtrFactory<SearchResultsPanel> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_SEARCH_RESULTS_PANEL_H_

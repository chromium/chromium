// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_PAGINATION_VIEW_H_
#define ASH_STYLE_PAGINATION_VIEW_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/pagination/pagination_model_observer.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class ScrollView;
}  // namespace views

namespace ash {

class PaginationModel;

// PaginationView is used to paginate the UI surface with multiple pages of
// contents. It has as many indicators as pages. Pressing an indicator will jump
// to the corresponding page. There is a maximum number of visible indicators.
// If the number of total pages exceeds the visible maximum, two arrow-shaped
// overflow buttons will be shown on both sides. The arrow buttons can also
// control pagination. The layout of a horizontal pagination view with arrow
// buttons is like below:
//                   +---+---+---+---+---+---+---+
//                   | < | o | o | o | o | o | > |
//                   +-|-+---+---+-|-+---+---+-|-+
//            backward arrow button    |    forward arrow button
//                             indicator
//
// When a page is selected, a selector dot (a solid circle) will move to the
// corresponding indicator. The selector dot has two motion effects:
// - If moves to a neighbor page, the selector dot will first be stretched into
// a pill shape until it connects the current indicator to the target indicator,
// and then shrink back to a circle at the target indicator position.
// - If jumps across multiple pages, the selector dot will first shrink at the
// current indicator position, and then expand at the target indicator position.
class ASH_EXPORT PaginationView : public views::View,
                                  public PaginationModelObserver {
  METADATA_HEADER(PaginationView, views::View)

 public:
  enum class Orientation {
    kHorizontal,
    kVertical,
  };

  // A paginaition view only binds with one pagination model, but the pagination
  // model may control multiple views. Therefore, pagination model should
  // outlive the pagination view.
  explicit PaginationView(PaginationModel* model,
                          Orientation orientation = Orientation::kHorizontal);
  PaginationView(const PaginationView&) = delete;
  PaginationView& operator=(const PaginationView*) = delete;
  ~PaginationView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;

 private:
  // A filled circle with pagination motion effects.
  class SelectorDotView;
  // The container of indicators.
  class IndicatorContainer;

  void CreateArrowButtons();
  void RemoveArrowButtons();
  void UpdateArrowButtonsVisiblity();
  void OnArrowButtonPressed(bool forward, const ui::Event& event);
  void MaybeSetUpScroll();

  bool ShouldShowSelectorDot() const;
  void CreateSelectorDot();
  void RemoveSelectorDot();
  void UpdateSelectorDot();
  void SetUpSelectorDotDeformation();

  // Returns true if the indicator button corresponding to the given page is
  // currently visible.
  bool IsIndicatorVisible(int page) const;

  // PaginationModelObserver:
  void TotalPagesChanged(int previous_page_count, int new_page_count) override;
  void SelectedPageChanged(int old_selected, int new_selected) override;
  void TransitionChanged() override;

  raw_ptr<PaginationModel> const model_;
  const Orientation orientation_;

  // The scroll view with an indicator container as its contents. The scroll
  // view is owned by this and the container is owned by the scroll view.
  raw_ptr<views::ScrollView> indicator_scroll_view_ = nullptr;
  raw_ptr<IndicatorContainer> indicator_container_ = nullptr;

  // The selector dot view which is owned by this.
  raw_ptr<SelectorDotView> selector_dot_ = nullptr;

  // The arrow buttons owned by this.
  raw_ptr<views::ImageButton> backward_arrow_button_ = nullptr;
  raw_ptr<views::ImageButton> forward_arrow_button_ = nullptr;

  base::ScopedObservation<PaginationModel, PaginationModelObserver>
      model_observation_{this};
};

}  // namespace ash

#endif  // ASH_STYLE_PAGINATION_VIEW_H_

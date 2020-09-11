// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/task_continuation_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/continue_browsing_chip.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_model.h"

namespace ash {

namespace {

constexpr int kTaskContinuationHeaderSpacing = 8;
constexpr gfx::Insets kTaskContinuationViewPadding(12, 4);
constexpr gfx::Insets kPhoneHubSubHeaderPadding(4, 32);
constexpr gfx::Size kTaskContinuationChipSize(170, 50);
constexpr int kTaskContinuationChipsInRow = 2;
constexpr int kTaskContinuationChipSpacing = 8;
constexpr int kTaskContinuationChipVerticalPadding = 5;

class HeaderView : public views::View {
 public:
  HeaderView() {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, kPhoneHubSubHeaderPadding));
    auto* header_label = AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_TASK_CONTINUATION_TITLE)));
    header_label->SetAutoColorReadabilityEnabled(false);
    header_label->SetSubpixelRenderingEnabled(false);
    header_label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
  }

  ~HeaderView() override = default;
  HeaderView(HeaderView&) = delete;
  HeaderView operator=(HeaderView&) = delete;

  // views::View:
  const char* GetClassName() const override { return "HeaderView"; }
};

class ChipsView : public views::View {
 public:
  ChipsView() {
    // TODO(leandre): Add task chip to bubble using real data from phone.
    AddTaskChip(new ContinueBrowsingChip());
    AddTaskChip(new ContinueBrowsingChip());
    AddTaskChip(new ContinueBrowsingChip());
    AddTaskChip(new ContinueBrowsingChip());
  }

  ~ChipsView() override = default;
  ChipsView(ChipsView&) = delete;
  ChipsView operator=(ChipsView&) = delete;

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    int width =
        kTaskContinuationChipSize.width() * kTaskContinuationChipsInRow +
        kTaskContinuationChipSpacing;
    int rows_num = std::ceil(
        (double)(task_chips_.view_size() / kTaskContinuationChipsInRow));
    int height = (kTaskContinuationChipSize.height() +
                  kTaskContinuationChipVerticalPadding) *
                     std::max(0, rows_num - 1) +
                 kTaskContinuationChipSize.height();
    return gfx::Size(width, height);
  }

  void Layout() override {
    views::View::Layout();
    CalculateIdealBounds();
    for (int i = 0; i < task_chips_.view_size(); ++i) {
      auto* button = task_chips_.view_at(i);
      button->SetBoundsRect(task_chips_.ideal_bounds(i));
    }
  }

  const char* GetClassName() const override { return "ChipsView"; }

 private:
  gfx::Point GetButtonPosition(int index) {
    int row = index / kTaskContinuationChipsInRow;
    int column = index % kTaskContinuationChipsInRow;
    int x = (kTaskContinuationChipSize.width() + kTaskContinuationChipSpacing) *
            column;
    int y = (kTaskContinuationChipSize.height() +
             kTaskContinuationChipVerticalPadding) *
            row;
    return gfx::Point(x, y);
  }

  void CalculateIdealBounds() {
    for (int i = 0; i < task_chips_.view_size(); ++i) {
      gfx::Rect tile_bounds =
          gfx::Rect(GetButtonPosition(i), kTaskContinuationChipSize);
      task_chips_.set_ideal_bounds(i, tile_bounds);
    }
  }

  void AddTaskChip(views::View* task_chip) {
    int view_size = task_chips_.view_size();
    task_chips_.Add(task_chip, view_size);
    AddChildView(task_chip);
  }

  views::ViewModelT<views::View> task_chips_;
};

}  // namespace

TaskContinuationView::TaskContinuationView() {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kTaskContinuationViewPadding,
      kTaskContinuationHeaderSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  AddChildView(std::make_unique<HeaderView>());
  AddChildView(std::make_unique<ChipsView>());
}

TaskContinuationView::~TaskContinuationView() = default;

const char* TaskContinuationView::GetClassName() const {
  return "TaskContinuationView";
}

}  // namespace ash

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/task_continuation_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/typography.h"
#include "ash/system/phonehub/continue_browsing_chip.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/phonehub/ui_constants.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance constants in dip.
constexpr int kTaskContinuationChipHeight = 96;
constexpr int kTaskContinuationChipsInRow = 2;
constexpr int kTaskContinuationChipSpacing = 8;
constexpr int kTaskContinuationChipHorizontalSidePadding = 4;
constexpr int kTaskContinuationChipVerticalPadding = 4;
constexpr int kHeaderLabelLineHeight = 48;

gfx::Size GetTaskContinuationChipSize() {
  int width =
      (kTrayMenuWidth - kBubbleHorizontalSidePaddingDip * 2 -
       kTaskContinuationChipHorizontalSidePadding * 2 -
       kTaskContinuationChipSpacing * (kTaskContinuationChipsInRow - 1)) /
      kTaskContinuationChipsInRow;
  return gfx::Size(width, kTaskContinuationChipHeight);
}

class HeaderView : public views::Label {
  METADATA_HEADER(HeaderView, views::Label)

 public:
  HeaderView() {
    SetText(
        l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_TASK_CONTINUATION_TITLE));
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_MIDDLE);
    SetAutoColorReadabilityEnabled(false);
    SetSubpixelRenderingEnabled(false);
    // TODO(b/322067753): Replace usage of |AshColorProvider| with
    // |cros_tokens|.
    SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
    TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosButton1,
                                          *this);
    SetLineHeight(kHeaderLabelLineHeight);
  }

  ~HeaderView() override = default;
  HeaderView(HeaderView&) = delete;
  HeaderView operator=(HeaderView&) = delete;
};

BEGIN_METADATA(HeaderView)
END_METADATA

}  // namespace

TaskContinuationView::TaskContinuationView(
    phonehub::PhoneModel* phone_model,
    phonehub::UserActionRecorder* user_action_recorder)
    : phone_model_(phone_model), user_action_recorder_(user_action_recorder) {
  SetID(PhoneHubViewID::kTaskContinuationView);

  phone_model_->AddObserver(this);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  AddChildView(std::make_unique<HeaderView>());
  chips_view_ = AddChildView(std::make_unique<TaskChipsView>());

  Update();
}

TaskContinuationView::~TaskContinuationView() {
  phone_model_->RemoveObserver(this);
}

void TaskContinuationView::OnModelChanged() {
  Update();
}

TaskContinuationView::TaskChipsView::TaskChipsView() = default;

TaskContinuationView::TaskChipsView::~TaskChipsView() = default;

void TaskContinuationView::TaskChipsView::AddTaskChip(views::View* task_chip) {
  size_t view_size = task_chips_.view_size();
  task_chips_.Add(task_chip, view_size);
  AddChildView(task_chip);
}

// views::View:
gfx::Size TaskContinuationView::TaskChipsView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  auto chip_size = GetTaskContinuationChipSize();
  int width = chip_size.width() * kTaskContinuationChipsInRow +
              kTaskContinuationChipSpacing +
              2 * kTaskContinuationChipHorizontalSidePadding;
  int rows_num =
      std::ceil((double)task_chips_.view_size() / kTaskContinuationChipsInRow);
  int height = (chip_size.height() + kTaskContinuationChipVerticalPadding) *
                   std::max(0, rows_num - 1) +
               chip_size.height() +
               2 * kTaskContinuationChipHorizontalSidePadding;
  return gfx::Size(width, height);
}

void TaskContinuationView::TaskChipsView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);
  CalculateIdealBounds();
  for (size_t i = 0; i < task_chips_.view_size(); ++i) {
    auto* button = task_chips_.view_at(i);
    button->SetBoundsRect(task_chips_.ideal_bounds(i));
  }
}

void TaskContinuationView::TaskChipsView::Reset() {
  task_chips_.Clear();
  RemoveAllChildViews();
}

gfx::Point TaskContinuationView::TaskChipsView::GetButtonPosition(int index) {
  auto chip_size = GetTaskContinuationChipSize();
  int row = index / kTaskContinuationChipsInRow;
  int column = index % kTaskContinuationChipsInRow;
  int x = (chip_size.width() + kTaskContinuationChipSpacing) * column +
          kTaskContinuationChipHorizontalSidePadding;
  int y = (chip_size.height() + kTaskContinuationChipVerticalPadding) * row +
          kTaskContinuationChipVerticalPadding;
  return gfx::Point(x, y);
}

void TaskContinuationView::TaskChipsView::CalculateIdealBounds() {
  for (size_t i = 0; i < task_chips_.view_size(); ++i) {
    gfx::Rect tile_bounds =
        gfx::Rect(GetButtonPosition(i), GetTaskContinuationChipSize());
    task_chips_.set_ideal_bounds(i, tile_bounds);
  }
}

BEGIN_METADATA(TaskContinuationView, TaskChipsView)
END_METADATA

void TaskContinuationView::Update() {
  chips_view_->Reset();

  if (!phone_model_->browser_tabs_model()) {
    SetVisible(false);
    return;
  }

  const phonehub::BrowserTabsModel& browser_tabs =
      phone_model_->browser_tabs_model().value();

  if (!browser_tabs.is_tab_sync_enabled() ||
      browser_tabs.most_recent_tabs().empty()) {
    SetVisible(false);
    return;
  }

  int index = 0;
  for (const phonehub::BrowserTabsModel::BrowserTabMetadata& metadata :
       browser_tabs.most_recent_tabs()) {
    chips_view_->AddTaskChip(new ContinueBrowsingChip(
        metadata, index, browser_tabs.most_recent_tabs().size(),
        user_action_recorder_));
    index++;
  }

  PreferredSizeChanged();
  SetVisible(true);
}

BEGIN_METADATA(TaskContinuationView)
END_METADATA

}  // namespace ash

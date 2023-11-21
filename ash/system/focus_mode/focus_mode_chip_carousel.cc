// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_chip_carousel.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/style_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {

constexpr auto kScrollViewInsets = gfx::Insets::TLBR(16, 0, 0, 0);
constexpr int kChipSpaceInbetween = 8;
constexpr int kChipHeight = 32;
constexpr int kChipMaxWidth = 192;
constexpr auto kChipInsets = gfx::Insets::TLBR(0, 8, 0, 12);
constexpr int kChipImageSize = 16;
constexpr int kChipCornerRadius = 16;
constexpr int kChipImageSpacing = 12;
constexpr size_t kMaxTasks = 5;

void SetupChip(views::LabelButton* chip) {
  chip->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  chip->SetBorder(views::CreatePaddedBorder(
      views::CreateThemedRoundedRectBorder(1, kChipHeight,
                                           cros_tokens::kCrosSysSeparator),
      kChipInsets));
  chip->SetLabelStyle(views::style::STYLE_BODY_3_MEDIUM);
  chip->SetMinSize(gfx::Size(0, kChipHeight));
  chip->SetMaxSize(gfx::Size(kChipMaxWidth, kChipHeight));
  chip->SetImageLabelSpacing(kChipImageSpacing);
  chip->SetImageModel(views::Button::STATE_NORMAL,
                      ui::ImageModel::FromVectorIcon(
                          kGlanceablesTasksIcon, cros_tokens::kCrosSysOnSurface,
                          kChipImageSize));
  views::FocusRing::Get(chip)->SetColorId(cros_tokens::kCrosSysFocusRing);
  views::InstallRoundRectHighlightPathGenerator(chip, gfx::Insets(1),
                                                kChipCornerRadius);
  views::InkDrop::Get(chip)->SetMode(views::InkDropHost::InkDropMode::ON);
  chip->SetHasInkDropActionOnClick(true);
  StyleUtil::ConfigureInkDropAttributes(
      chip, StyleUtil::kBaseColor | StyleUtil::kInkDropOpacity);
  chip->SetNotifyEnterExitOnChild(true);
}

}  // namespace

// `on_chip_pressed` will be called when a task chip is clicked, containing a
// string name of a task.
FocusModeChipCarousel::FocusModeChipCarousel(
    ChipPressedCallback on_chip_pressed)
    : views::ScrollView(views::ScrollView::ScrollWithLayers::kEnabled),
      on_chip_pressed_(std::move(on_chip_pressed)) {
  SetBorder(views::CreateEmptyBorder(kScrollViewInsets));
  SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  SetDrawOverflowIndicator(false);
  SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  SetBackgroundColor(absl::nullopt);

  scroll_contents_ = SetContents(std::make_unique<views::BoxLayoutView>());
  scroll_contents_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  scroll_contents_->SetBetweenChildSpacing(kChipSpaceInbetween);
}

FocusModeChipCarousel::~FocusModeChipCarousel() = default;

void FocusModeChipCarousel::SetTasks(const std::vector<std::u16string>& tasks) {
  scroll_contents_->RemoveAllChildViews();
  if (tasks.empty()) {
    return;
  }

  // Populate a maximum of `kMaxTasks` tasks.
  const size_t num_tasks = std::min(tasks.size(), kMaxTasks);
  int content_width = kChipSpaceInbetween * (num_tasks - 1) +
                      (kChipInsets.width() + kChipSpaceInbetween) * num_tasks;

  for (size_t i = 0; i < num_tasks; i++) {
    const std::u16string& task = tasks[i];
    views::LabelButton* chip =
        scroll_contents_->AddChildView(std::make_unique<views::LabelButton>(
            base::BindRepeating(on_chip_pressed_, task), task));
    SetupChip(chip);
    content_width += chip->GetPreferredSize().width();
  }

  // Now that the chips have populated, contract the contents to only be wide
  // enough to fit all the chips with no extra space, but not narrow enough to
  // make the chips contract.
  scroll_contents_->SetPreferredSize(gfx::Size(content_width, kChipHeight));
}

bool FocusModeChipCarousel::HasTasks() const {
  return !scroll_contents_->GetChildrenInZOrder().empty();
}

}  // namespace ash

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/recent_files_bubble.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/holding_space/downloads_section.h"
#include "ash/system/holding_space/holding_space_ui.h"
#include "ash/system/holding_space/screen_captures_section.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

const gfx::Insets kPlaceholderPadding =
    gfx::Insets::TLBR(36, 50, 36, 50) - kHoldingSpaceChildBubblePadding;
constexpr int kPlaceholderChildSpacing = 16;

}  // namespace

RecentFilesBubble::RecentFilesBubble(HoldingSpaceViewDelegate* delegate)
    : HoldingSpaceTrayChildBubble(delegate) {
  SetID(kHoldingSpaceRecentFilesBubbleId);
}

RecentFilesBubble::~RecentFilesBubble() = default;

const char* RecentFilesBubble::GetClassName() const {
  return "RecentFilesBubble";
}

std::unique_ptr<views::View> RecentFilesBubble::CreatePlaceholder() {
  if (!features::IsHoldingSpacePredictabilityEnabled())
    return nullptr;

  return views::Builder<views::View>()
      .SetID(kHoldingSpaceRecentFilesPlaceholderId)
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kPlaceholderPadding,
          kPlaceholderChildSpacing))
      .AddChild(views::Builder<views::ImageView>().SetImage(
          ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
              IDR_HOLDING_SPACE_RECENT_FILES_PLACEHOLDER_IMAGE)))
      .AddChild(
          holding_space_ui::CreateBubblePlaceholderLabel(
              IDS_ASH_HOLDING_SPACE_RECENT_FILES_PLACEHOLDER)
              .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER)
              .SetMultiLine(true))
      .Build();
}

std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>>
RecentFilesBubble::CreateSections() {
  std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>> sections;
  sections.push_back(std::make_unique<ScreenCapturesSection>(delegate()));
  sections.push_back(std::make_unique<DownloadsSection>(delegate()));
  return sections;
}

}  // namespace ash

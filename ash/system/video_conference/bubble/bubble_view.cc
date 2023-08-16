// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/bubble_view.h"

#include <memory>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/bubble/return_to_app_panel.h"
#include "ash/system/video_conference/bubble/set_value_effects_view.h"
#include "ash/system/video_conference/bubble/toggle_effects_view.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash::video_conference {

namespace {

constexpr int kLinuxAppWarningViewTopPadding = 12;
constexpr int kLinuxAppWarningViewSpacing = 1;
constexpr int kLinuxAppWarningIconSize = 16;

// Check if there's a linux app in the given `apps`.
bool HasLinuxApps(const MediaApps& apps) {
  for (auto& app : apps) {
    if (app->app_type == crosapi::mojom::VideoConferenceAppType::kCrostiniVm ||
        app->app_type == crosapi::mojom::VideoConferenceAppType::kPluginVm ||
        app->app_type == crosapi::mojom::VideoConferenceAppType::kBorealis) {
      return true;
    }
  }
  return false;
}

// A view that will be display when there's Linux app(s) running along with
// other media apps, used to warn users that effects cannot be applied to Linux
// apps.
class LinuxAppWarningView : public views::View {
 public:
  METADATA_HEADER(LinuxAppWarningView);

  LinuxAppWarningView() {
    SetID(BubbleViewID::kLinuxAppWarningView);
    SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
        .SetInteriorMargin(
            gfx::Insets::TLBR(kLinuxAppWarningViewTopPadding, 0, 0, 0))
        .SetDefault(views::kMarginsKey,
                    gfx::Insets::TLBR(0, kLinuxAppWarningViewSpacing, 0,
                                      kLinuxAppWarningViewSpacing));

    auto icon = std::make_unique<views::ImageView>();
    icon->SetImage(ui::ImageModel::FromVectorIcon(
        kVideoConferenceLinuxAppWarningIcon,
        cros_tokens::kCrosSysOnSurfaceVariant, kLinuxAppWarningIconSize));
    AddChildView(std::move(icon));

    auto label = std::make_unique<views::Label>();
    label->SetText(l10n_util::GetStringUTF16(
        IDS_ASH_VIDEO_CONFERENCE_BUBBLE_LINUX_APP_WARNING_TEXT));
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosAnnotation2,
                                          *label);
    AddChildView(std::move(label));
  }

  LinuxAppWarningView(const LinuxAppWarningView&) = delete;
  LinuxAppWarningView& operator=(const LinuxAppWarningView&) = delete;

  ~LinuxAppWarningView() override = default;
};

BEGIN_METADATA(LinuxAppWarningView, views::View);
END_METADATA

}  // namespace

BubbleView::BubbleView(const InitParams& init_params,
                       const MediaApps& media_apps,
                       VideoConferenceTrayController* controller)
    : TrayBubbleView(init_params),
      controller_(controller),
      media_apps_(media_apps) {
  SetID(BubbleViewID::kMainBubbleView);

  // Add a `FlexLayout` for the entire view.
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
}

BubbleView::~BubbleView() = default;

void BubbleView::AddedToWidget() {
  // `ReturnToAppPanel` resides in the top-level layout and isn't part of the
  // scrollable area (that can't be added until the `BubbleView` officially has
  // a parent widget).
  AddChildView(std::make_unique<ReturnToAppPanel>(media_apps_));

  const bool has_toggle_effects =
      controller_->effects_manager().HasToggleEffects();
  const bool has_set_value_effects =
      controller_->effects_manager().HasSetValueEffects();

  if (HasLinuxApps(media_apps_) &&
      (has_toggle_effects || has_set_value_effects)) {
    AddChildView(std::make_unique<LinuxAppWarningView>());
  }

  // Create the `views::ScrollView` to house the effects sections. This has to
  // be done here because `BubbleDialogDelegate::GetBubbleBounds` requires a
  // parent widget, which isn't officially assigned until after the call to
  // `ShowBubble` in `VideoConferenceTray::ToggleBubble`.
  auto* scroll_view = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view->SetAllowKeyboardScrolling(false);
  scroll_view->SetBackgroundColor(absl::nullopt);

  // TODO(b/262930924): Use the correct max_height.
  scroll_view->ClipHeightTo(/*min_height=*/0, /*max_height=*/300);
  scroll_view->SetDrawOverflowIndicator(false);
  scroll_view->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);

  auto* scroll_contents_view =
      scroll_view->SetContents(std::make_unique<views::BoxLayoutView>());
  scroll_contents_view->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  scroll_contents_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  scroll_contents_view->SetInsideBorderInsets(
      gfx::Insets::VH(16, kVideoConferenceBubbleHorizontalPadding));

  // Make the effects sections children of the `views::FlexLayoutView`, so that
  // they scroll (if more effects are present than can fit in the available
  // height).
  if (has_toggle_effects) {
    scroll_contents_view->AddChildView(
        std::make_unique<ToggleEffectsView>(controller_));
  }
  if (has_set_value_effects) {
    scroll_contents_view->AddChildView(
        std::make_unique<SetValueEffectsView>(controller_));
  }
}

void BubbleView::ChildPreferredSizeChanged(View* child) {
  PreferredSizeChanged();
  SizeToContents();
}

bool BubbleView::CanActivate() const {
  return true;
}

}  // namespace ash::video_conference

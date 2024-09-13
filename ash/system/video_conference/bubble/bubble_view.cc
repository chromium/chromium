// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/bubble_view.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "ash/system/camera/camera_effects_controller.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/bubble/return_to_app_panel.h"
#include "ash/system/video_conference/bubble/set_camera_background_view.h"
#include "ash/system/video_conference/bubble/set_value_effects_view.h"
#include "ash/system/video_conference/bubble/title_view.h"
#include "ash/system/video_conference/bubble/toggle_effects_view.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "media/capture/video/chromeos/mojom/effects_pipeline.mojom.h"
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
#include "ui/views/view_utils.h"

namespace ash::video_conference {

namespace {

constexpr int kLinuxAppWarningViewTopPadding = 12;
constexpr int kDLCErrorWarningLabelTopPadding = 0;
constexpr int kWarningViewSpacing = 1;
constexpr int kWarningIconSize = 16;

constexpr int kScrollViewBetweenChildSpacing = 16;

CameraEffectsController* GetCameraEffectsController() {
  return Shell::Get()->camera_effects_controller();
}

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

// Creates a view that will display a warning icon with text.
std::unique_ptr<views::View> CreateWarningView(
    int warning_view_id,
    int top_padding,
    std::optional<int> warning_message = std::nullopt) {
  auto view = std::make_unique<views::View>();
  view->SetID(warning_view_id);
  view->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetInteriorMargin(gfx::Insets::TLBR(top_padding, 0, 0, 0))
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets::TLBR(0, kWarningViewSpacing, 0, kWarningViewSpacing));

  auto icon = std::make_unique<views::ImageView>();
  icon->SetImage(ui::ImageModel::FromVectorIcon(
      kVideoConferenceWarningIcon, cros_tokens::kCrosSysOnSurfaceVariant,
      kWarningIconSize));
  view->AddChildView(std::move(icon));

  auto label = std::make_unique<views::Label>();
  // Set a view ID so the label can be modified if necessary.
  label->SetID(BubbleViewID::kWarningViewLabel);
  if (warning_message.has_value()) {
    label->SetText(l10n_util::GetStringUTF16(*warning_message));
  }
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosAnnotation2,
                                        *label);
  view->AddChildView(std::move(label));

  return view;
}

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
  AddChildView(std::make_unique<TitleView>(base::BindOnce(
      &BubbleView::CloseBubbleView, weak_factory_.GetWeakPtr())));
  // `ReturnToAppPanel` resides in the top-level layout and isn't part of the
  // scrollable area (that can't be added until the `BubbleView` officially has
  // a parent widget).
  AddChildView(std::make_unique<ReturnToAppPanel>(*media_apps_));

  const bool has_toggle_effects =
      controller_->GetEffectsManager().HasToggleEffects();
  const bool has_set_value_effects =
      controller_->GetEffectsManager().HasSetValueEffects();

  if (HasLinuxApps(*media_apps_) &&
      (has_toggle_effects || has_set_value_effects)) {
    AddChildView(CreateWarningView(
        BubbleViewID::kLinuxAppWarningView,
        /*top_padding=*/kLinuxAppWarningViewTopPadding,
        IDS_ASH_VIDEO_CONFERENCE_BUBBLE_LINUX_APP_WARNING_TEXT));
  }

  // Create the `views::ScrollView` to house the effects sections. This has to
  // be done here because `BubbleDialogDelegate::GetBubbleBounds` requires a
  // parent widget, which isn't officially assigned until after the call to
  // `ShowBubble` in `VideoConferenceTray::ToggleBubble`.
  auto* scroll_view = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view->SetAllowKeyboardScrolling(false);
  scroll_view->SetBackgroundColor(std::nullopt);

  // TODO(b/262930924): Use the correct max_height.
  scroll_view->ClipHeightTo(/*min_height=*/0, /*max_height=*/400);
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
  scroll_contents_view->SetBetweenChildSpacing(kScrollViewBetweenChildSpacing);

  // Make the effects sections children of the `views::FlexLayoutView`, so that
  // they scroll (if more effects are present than can fit in the available
  // height).
  if (has_toggle_effects) {
    scroll_contents_view->AddChildView(
        std::make_unique<ToggleEffectsView>(controller_));
    auto error_warning_label_container_view =
        CreateWarningView(BubbleViewID::kDLCDownloadsInErrorView,
                          /*top_padding=*/kDLCErrorWarningLabelTopPadding);
    // Visibility will for most cases be false, if a DLC has an error in
    // downloading, state updates will be fetched by any toggle effect's
    // `FeatureTile`, then pushed from the controller via
    // `OnDLCDownloadStateInError()`.
    error_warning_label_container_view->SetVisible(false);
    scroll_contents_view->AddChildView(
        std::move(error_warning_label_container_view));
  }
  if (has_set_value_effects) {
    scroll_contents_view->AddChildView(
        std::make_unique<SetValueEffectsView>(controller_));
  }

  if (GetCameraEffectsController()->IsEligibleForBackgroundReplace()) {
    set_camera_background_view_ = scroll_contents_view->AddChildView(
        std::make_unique<SetCameraBackgroundView>(this, controller_.get()));
  }
}

void BubbleView::ChildPreferredSizeChanged(View* child) {
  PreferredSizeChanged();
  SizeToContents();
}

bool BubbleView::CanActivate() const {
  return true;
}

void BubbleView::SetBackgroundReplaceUiVisible(bool visible) {
  CHECK(GetCameraEffectsController()->is_eligible_for_background_replace() &&
        set_camera_background_view_)
      << "Can't show set_camera_background_view before it is constructed.";

  views::AsViewClass<SetCameraBackgroundView>(set_camera_background_view_)
      ->SetBackgroundReplaceUiVisible(visible);
  ChildPreferredSizeChanged(set_camera_background_view_);
}

void BubbleView::OnDLCDownloadStateInError(
    bool add_warning_view,
    const std::u16string& feature_tile_title_string) {
  auto* dlc_error_container_view =
      GetViewByID(BubbleViewID::kDLCDownloadsInErrorView);
  if (!dlc_error_container_view) {
    return;
  }

  auto* dlc_error_label = static_cast<views::Label*>(
      dlc_error_container_view->GetViewByID(BubbleViewID::kWarningViewLabel));
  if (!dlc_error_label) {
    return;
  }

  if (add_warning_view) {
    if (std::size(feature_tile_error_string_ids_) == 2) {
      return;
    }
    feature_tile_error_string_ids_.emplace(feature_tile_title_string);
  } else {
    auto it = std::find(feature_tile_error_string_ids_.begin(),
                        feature_tile_error_string_ids_.end(),
                        feature_tile_title_string);
    if (it == feature_tile_error_string_ids_.end()) {
      return;
    }
    feature_tile_error_string_ids_.erase(it);
  }

  // If the one and only string was removed, hide the view and reset it.
  // Otherwise update the string.
  if (feature_tile_error_string_ids_.empty()) {
    dlc_error_container_view->SetVisible(false);
    dlc_error_label->SetText(std::u16string());
    return;
  }

  if (feature_tile_error_string_ids_.size() == 1) {
    dlc_error_label->SetText(l10n_util::GetStringFUTF16(
        IDS_ASH_VIDEO_CONFERENCE_BUBBLE_DLC_ERROR_ONE,
        *feature_tile_error_string_ids_.begin()));
    dlc_error_container_view->SetVisible(true);
    return;
  }

  // Only two are supported, adding more would require more custom handling for
  // the string below.
  if (feature_tile_error_string_ids_.size() > 2u) {
    return;
  }
  std::vector<std::u16string> string_ids(feature_tile_error_string_ids_.size());
  std::copy(feature_tile_error_string_ids_.begin(),
            feature_tile_error_string_ids_.end(), string_ids.begin());
  dlc_error_label->SetText(
      l10n_util::GetStringFUTF16(IDS_ASH_VIDEO_CONFERENCE_BUBBLE_DLC_ERROR_TWO,
                                 string_ids, /*offsets=*/nullptr));
  dlc_error_container_view->SetVisible(true);
}

BEGIN_METADATA(BubbleView)
END_METADATA

}  // namespace ash::video_conference

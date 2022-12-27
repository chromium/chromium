// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/return_to_app_panel.h"

#include <memory>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

namespace ash::video_conference {

namespace {

const int kReturnToAppPanelRadius = 16;
const int kReturnToAppPanelSpacing = 8;
const int kReturnToAppButtonSpacing = 12;
const int kReturnToAppButtonIconsSpacing = 2;

// Creates a view containing camera, microphone, and screen share icons that
// shows capturing state of a media app.
std::unique_ptr<views::View> CreateReturnToAppIconsContainer(
    bool is_capturing_camera,
    bool is_capturing_microphone,
    bool is_capturing_screen) {
  auto container = std::make_unique<views::View>();
  container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, kReturnToAppButtonIconsSpacing / 2, 0,
                                    kReturnToAppButtonIconsSpacing / 2));

  if (is_capturing_camera) {
    auto camera_icon = std::make_unique<views::ImageView>();
    camera_icon->SetImage(ui::ImageModel::FromVectorIcon(
        kPrivacyIndicatorsCameraIcon, cros_tokens::kCrosSysPositive));
    container->AddChildView(std::move(camera_icon));
  }

  if (is_capturing_microphone) {
    auto microphone_icon = std::make_unique<views::ImageView>();
    microphone_icon->SetImage(ui::ImageModel::FromVectorIcon(
        kPrivacyIndicatorsMicrophoneIcon, cros_tokens::kCrosSysPositive));
    container->AddChildView(std::move(microphone_icon));
  }

  if (is_capturing_screen) {
    auto screen_share_icon = std::make_unique<views::ImageView>();
    screen_share_icon->SetImage(ui::ImageModel::FromVectorIcon(
        kPrivacyIndicatorsScreenShareIcon, cros_tokens::kCrosSysPositive));
    container->AddChildView(std::move(screen_share_icon));
  }

  return container;
}

// Gets the display text representing a media app shown in the return to app
// panel.
std::u16string GetMediaAppDisplayText(
    mojo::StructPtr<crosapi::mojom::VideoConferenceMediaAppInfo>& media_app) {
  // Displays the url if it is valid. Otherwise, display app title.
  auto url = media_app->url;
  return url && url->is_valid() ? base::UTF8ToUTF16(url->GetContent())
                                : media_app->title;
}

}  // namespace

// -----------------------------------------------------------------------------
// ReturnToAppButton:

ReturnToAppButton::ReturnToAppButton(bool is_capturing_camera,
                                     bool is_capturing_microphone,
                                     bool is_capturing_screen,
                                     const std::u16string& display_text)
    : is_capturing_camera_(is_capturing_camera),
      is_capturing_microphone_(is_capturing_microphone),
      is_capturing_screen_(is_capturing_screen) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, kReturnToAppButtonSpacing / 2, 0,
                                    kReturnToAppButtonSpacing / 2));

  AddChildView(CreateReturnToAppIconsContainer(
      is_capturing_camera, is_capturing_microphone, is_capturing_screen));

  label_ = AddChildView(std::make_unique<views::Label>(display_text));
}

// -----------------------------------------------------------------------------
// ReturnToAppPanel:

ReturnToAppPanel::ReturnToAppPanel() {
  SetID(BubbleViewID::kReturnToApp);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, 0, kReturnToAppPanelSpacing, 0))
      .SetInteriorMargin(gfx::Insets::TLBR(12, 0, 0, 0));

  // Add running media apps buttons to the panel.
  VideoConferenceTrayController::Get()->GetMediaApps(base::BindOnce(
      &ReturnToAppPanel::AddButtonsToPanel, weak_ptr_factory_.GetWeakPtr()));

  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase, kReturnToAppPanelRadius));
}

ReturnToAppPanel::~ReturnToAppPanel() = default;

void ReturnToAppPanel::AddButtonsToPanel(MediaApps apps) {
  if (apps.size() < 1) {
    SetVisible(false);
    return;
  }

  if (apps.size() == 1) {
    auto& app = apps.front();
    AddChildView(std::make_unique<ReturnToAppButton>(
        app->is_capturing_camera, app->is_capturing_microphone,
        app->is_capturing_screen, GetMediaAppDisplayText(app)));
    return;
  }

  bool any_apps_capturing_camera = false;
  bool any_apps_capturing_microphone = false;
  bool any_apps_capturing_screen = false;

  for (auto& app : apps) {
    AddChildView(std::make_unique<ReturnToAppButton>(
        app->is_capturing_camera, app->is_capturing_microphone,
        app->is_capturing_screen, GetMediaAppDisplayText(app)));

    any_apps_capturing_camera |= app->is_capturing_camera;
    any_apps_capturing_microphone |= app->is_capturing_microphone;
    any_apps_capturing_screen |= app->is_capturing_screen;
  }

  auto summary_text = l10n_util::GetStringFUTF16Int(
      IDS_ASH_VIDEO_CONFERENCE_RETURN_TO_APP_SUMMARY_TEXT,
      static_cast<int>(apps.size()));

  AddChildViewAt(std::make_unique<ReturnToAppButton>(
                     any_apps_capturing_camera, any_apps_capturing_microphone,
                     any_apps_capturing_screen, summary_text),
                 0);
}

}  // namespace ash::video_conference
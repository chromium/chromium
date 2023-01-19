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
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

namespace ash::video_conference {

namespace {

const int kReturnToAppPanelRadius = 16;
const int kReturnToAppPanelSpacing = 8;
const int kReturnToAppButtonTopRowSpacing = 12;
const int kReturnToAppButtonSpacing = 16;
const int kReturnToAppButtonIconsSpacing = 2;
const int kReturnToAppIconSize = 20;

// Creates a view containing camera, microphone, and screen share icons that
// shows capturing state of a media app.
std::unique_ptr<views::View> CreateReturnToAppIconsContainer(
    bool is_capturing_camera,
    bool is_capturing_microphone,
    bool is_capturing_screen) {
  auto container = std::make_unique<views::View>();
  container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, kReturnToAppButtonIconsSpacing / 2, 0,
                                    kReturnToAppButtonIconsSpacing / 2));

  if (is_capturing_camera) {
    auto camera_icon = std::make_unique<views::ImageView>();
    camera_icon->SetImage(ui::ImageModel::FromVectorIcon(
        kPrivacyIndicatorsCameraIcon, cros_tokens::kCrosSysPositive,
        kReturnToAppIconSize));
    container->AddChildView(std::move(camera_icon));
  }

  if (is_capturing_microphone) {
    auto microphone_icon = std::make_unique<views::ImageView>();
    microphone_icon->SetImage(ui::ImageModel::FromVectorIcon(
        kPrivacyIndicatorsMicrophoneIcon, cros_tokens::kCrosSysPositive,
        kReturnToAppIconSize));
    container->AddChildView(std::move(microphone_icon));
  }

  if (is_capturing_screen) {
    auto screen_share_icon = std::make_unique<views::ImageView>();
    screen_share_icon->SetImage(ui::ImageModel::FromVectorIcon(
        kPrivacyIndicatorsScreenShareIcon, cros_tokens::kCrosSysPositive,
        kReturnToAppIconSize));
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

void ReturnToApp(const base::UnguessableToken& id) {
  // Returns early for the summary row, which has empty `id`.
  if (id.is_empty()) {
    return;
  }
  ash::VideoConferenceTrayController::Get()->ReturnToApp(id);
}

// A customized toggle button for the return to app panel, which rotates
// depending on the expand state.
class ReturnToAppExpandButton : public views::ImageButton,
                                ReturnToAppButton::Observer {
 public:
  ReturnToAppExpandButton(PressedCallback callback,
                          ReturnToAppButton* return_to_app_button)
      : views::ImageButton(std::move(callback)),
        return_to_app_button_(return_to_app_button) {
    return_to_app_button_->AddObserver(this);
  }

  ReturnToAppExpandButton(const ReturnToAppExpandButton&) = delete;
  ReturnToAppExpandButton& operator=(const ReturnToAppExpandButton&) = delete;

  ~ReturnToAppExpandButton() override {
    return_to_app_button_->RemoveObserver(this);
  }

  // views::ImageButton:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    // Rotate the canvas to rotate the button depending on the panel's expanded
    // state.
    gfx::ScopedCanvas scoped(canvas);
    canvas->Translate(gfx::Vector2d(size().width() / 2, size().height() / 2));
    if (!expanded_) {
      canvas->sk_canvas()->rotate(180.);
    }
    gfx::ImageSkia image = GetImageToPaint();
    canvas->DrawImageInt(image, -image.width() / 2, -image.height() / 2);
  }

 private:
  // ReturnToAppButton::Observer:
  void OnExpandedStateChanged(bool expanded) override {
    if (expanded_ == expanded) {
      return;
    }
    expanded_ = expanded;

    // Repaint to rotate the button.
    SchedulePaint();
  }

  // Indicates if this button (and also the parent panel) is in the expanded
  // state.
  bool expanded_ = false;

  // Owned by the views hierarchy. Will be destroyed after this view since it is
  // the parent.
  ReturnToAppButton* const return_to_app_button_;
};

}  // namespace

// -----------------------------------------------------------------------------
// ReturnToAppButton:

ReturnToAppButton::ReturnToAppButton(ReturnToAppPanel* panel,
                                     bool is_top_row,
                                     const base::UnguessableToken& id,
                                     bool is_capturing_camera,
                                     bool is_capturing_microphone,
                                     bool is_capturing_screen,
                                     const std::u16string& display_text)
    : views::Button(base::BindRepeating(&ReturnToApp, id)),
      is_capturing_camera_(is_capturing_camera),
      is_capturing_microphone_(is_capturing_microphone),
      is_capturing_screen_(is_capturing_screen),
      panel_(panel) {
  auto spacing = is_top_row ? kReturnToAppButtonTopRowSpacing / 2
                            : kReturnToAppButtonSpacing / 2;
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(is_top_row ? views::LayoutAlignment::kCenter
                                       : views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, spacing, 0, spacing));

  icons_container_ = AddChildView(CreateReturnToAppIconsContainer(
      is_capturing_camera, is_capturing_microphone, is_capturing_screen));
  if (!is_top_row) {
    icons_container_->SetPreferredSize(
        gfx::Size(/*width=*/kReturnToAppIconSize * panel->max_capturing_count(),
                  /*height=*/kReturnToAppIconSize));
  }

  label_ = AddChildView(std::make_unique<views::Label>(display_text));

  if (is_top_row) {
    auto expand_button = std::make_unique<ReturnToAppExpandButton>(
        base::BindRepeating(&ReturnToAppButton::OnExpandButtonToggled,
                            weak_ptr_factory_.GetWeakPtr()),
        this);
    expand_button->SetImageModel(
        views::Button::ButtonState::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(kUnifiedMenuExpandIcon,
                                       cros_tokens::kCrosSysSecondary, 16));
    expand_button->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_ASH_VIDEO_CONFERENCE_RETURN_TO_APP_SHOW_TOOLTIP));
    expand_button_ = AddChildView(std::move(expand_button));
  }

  // TODO(b/253646076): Double check accessible name for this button.
  SetAccessibleName(display_text);
}

ReturnToAppButton::~ReturnToAppButton() = default;

void ReturnToAppButton::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ReturnToAppButton::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ReturnToAppButton::OnExpandButtonToggled(const ui::Event& event) {
  expanded_ = !expanded_;

  for (auto& observer : observer_list_) {
    observer.OnExpandedStateChanged(expanded_);
  }

  icons_container_->SetVisible(!expanded_);
  auto tooltip_text_id =
      expanded_ ? IDS_ASH_VIDEO_CONFERENCE_RETURN_TO_APP_HIDE_TOOLTIP
                : IDS_ASH_VIDEO_CONFERENCE_RETURN_TO_APP_SHOW_TOOLTIP;
  expand_button_->SetTooltipText(l10n_util::GetStringUTF16(tooltip_text_id));
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
      .SetInteriorMargin(gfx::Insets::TLBR(12, 16, 8, 16));

  // Add running media apps buttons to the panel.
  VideoConferenceTrayController::Get()->GetMediaApps(base::BindOnce(
      &ReturnToAppPanel::AddButtonsToPanel, weak_ptr_factory_.GetWeakPtr()));

  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase, kReturnToAppPanelRadius));
}

ReturnToAppPanel::~ReturnToAppPanel() {
  // We only need to remove observer in case that there's a summary row
  // (multiple apps).
  if (summary_row_view_) {
    summary_row_view_->RemoveObserver(this);
  }
}

void ReturnToAppPanel::OnExpandedStateChanged(bool expanded) {
  for (auto* child : children()) {
    // Skip the first child since we always show the summary row. Otherwise,
    // show the other rows if `expanded` and vice versa.
    if (child == children().front()) {
      continue;
    }
    child->SetVisible(expanded);
  }
  PreferredSizeChanged();
}

void ReturnToAppPanel::AddButtonsToPanel(MediaApps apps) {
  if (apps.size() < 1) {
    SetVisible(false);
    return;
  }

  if (apps.size() == 1) {
    auto& app = apps.front();
    auto app_button = std::make_unique<ReturnToAppButton>(
        /*panel=*/this,
        /*is_top_row=*/true, app->id, app->is_capturing_camera,
        app->is_capturing_microphone, app->is_capturing_screen,
        GetMediaAppDisplayText(app));
    app_button->expand_button()->SetVisible(false);
    AddChildView(std::move(app_button));

    return;
  }

  bool any_apps_capturing_camera = false;
  bool any_apps_capturing_microphone = false;
  bool any_apps_capturing_screen = false;

  for (auto& app : apps) {
    max_capturing_count_ =
        std::max(max_capturing_count_, app->is_capturing_camera +
                                           app->is_capturing_microphone +
                                           app->is_capturing_screen);

    any_apps_capturing_camera |= app->is_capturing_camera;
    any_apps_capturing_microphone |= app->is_capturing_microphone;
    any_apps_capturing_screen |= app->is_capturing_screen;
  }

  auto summary_text = l10n_util::GetStringFUTF16Int(
      IDS_ASH_VIDEO_CONFERENCE_RETURN_TO_APP_SUMMARY_TEXT,
      static_cast<int>(apps.size()));

  summary_row_view_ = AddChildView(std::make_unique<ReturnToAppButton>(
      /*panel=*/this,
      /*is_top_row=*/true, /*app_id=*/base::UnguessableToken::Null(),
      any_apps_capturing_camera, any_apps_capturing_microphone,
      any_apps_capturing_screen, summary_text));
  summary_row_view_->AddObserver(this);

  for (auto& app : apps) {
    AddChildView(std::make_unique<ReturnToAppButton>(
        /*panel=*/this,
        /*is_top_row=*/false, app->id, app->is_capturing_camera,
        app->is_capturing_microphone, app->is_capturing_screen,
        GetMediaAppDisplayText(app)));
  }

  OnExpandedStateChanged(false);
}

}  // namespace ash::video_conference
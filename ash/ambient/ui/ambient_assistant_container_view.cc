// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_assistant_container_view.h"

#include <memory>
#include <string>

#include "ash/ambient/ui/ambient_assistant_dialog_plate.h"
#include "ash/ambient/ui/assistant_response_container_view.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kAvatarImageSizeDip = 32;

// Greeting message.
base::string16 GetGreetingMessage(const UserSession* user_session) {
  const std::string& username = user_session->user_info.display_name;
  return l10n_util::GetStringFUTF16(IDS_ASSISTANT_AMBIENT_GREETING_MESSAGE,
                                    base::UTF8ToUTF16(username));
}

}  // namespace

AmbientAssistantContainerView::AmbientAssistantContainerView(
    AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  InitLayout();

  // The AssistantViewDelegate should outlive AmbientAssistantContainerView.
  delegate_->AddUiModelObserver(this);
}

AmbientAssistantContainerView::~AmbientAssistantContainerView() {
  delegate_->RemoveUiModelObserver(this);
}

const char* AmbientAssistantContainerView::GetClassName() const {
  return "AmbientAssistantContainerView";
}

void AmbientAssistantContainerView::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  // TODO(meilinw): Define the expected behavior where multiple Assistant UIs
  // could exist at the same time (e.g. launcher embedded UI and ambient UI
  // for in-session Ambient Mode), but only one that is currently active
  // should be responding to Assistant events.
  if (assistant::util::IsStartingSession(new_visibility, old_visibility))
    SetVisible(true);
  else if (assistant::util::IsFinishingSession(new_visibility))
    SetVisible(false);
}

void AmbientAssistantContainerView::InitLayout() {
  SetPaintToLayer();
  SetBackground(views::CreateSolidBackground(SK_ColorWHITE));

  constexpr int kRightPaddingDip = 8;
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, 0, 0, kRightPaddingDip)));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Mic button and input query view.
  ambient_assistant_dialog_plate_ =
      AddChildView(std::make_unique<AmbientAssistantDialogPlate>(delegate_));

  // Response container view.
  assistant_response_container_view_ =
      AddChildView(std::make_unique<AssistantResponseContainerView>(delegate_));

  // Greeting label.
  const UserSession* active_user_session =
      Shell::Get()->session_controller()->GetUserSession(0);
  greeting_label_ = AddChildView(
      std::make_unique<views::Label>(GetGreetingMessage(active_user_session)));
  greeting_label_->SetEnabledColor(kTextColorSecondary);
  greeting_label_->SetFontList(
      ash::assistant::ui::GetDefaultFontList()
          .DeriveWithSizeDelta(8)
          .DeriveWithWeight(gfx::Font::Weight::NORMAL));
  greeting_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_CENTER);

  // Spacer.
  views::View* spacer = AddChildView(std::make_unique<views::View>());
  // Sets the flex weight to be 1 so the spacer view can be resized.
  layout_manager->SetFlexForView(spacer, 1);

  // Rounded avatar image view.
  avatar_view_ = AddChildView(std::make_unique<views::ImageView>());
  avatar_view_->SetImageSize(
      gfx::Size(kAvatarImageSizeDip, kAvatarImageSizeDip));
  avatar_view_->SetPreferredSize(
      gfx::Size(kAvatarImageSizeDip, kAvatarImageSizeDip));
  gfx::ImageSkia avatar = active_user_session->user_info.avatar.image;
  if (!avatar.isNull())
    avatar_view_->SetImage(avatar);

  SkPath circular_mask;
  constexpr int kClipCircleRadius = kAvatarImageSizeDip / 2;
  circular_mask.addCircle(kClipCircleRadius, kClipCircleRadius,
                          kClipCircleRadius);
  avatar_view_->set_clip_path(circular_mask);
}

}  // namespace ash

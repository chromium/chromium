// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_assistant_container_view.h"

#include <memory>
#include <string>

#include "ash/ambient/ui/ambient_assistant_dialog_plate.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/ambient/ui/assistant_response_container_view.h"
#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// Appearance.
constexpr int kAvatarImageSizeDip = 32;

constexpr int kAssistantPreferredHeightDip = 128;

// Greeting message.
base::string16 GetGreetingMessage(const UserSession* user_session) {
  DCHECK(user_session);
  const std::string& username = user_session->user_info.display_name;
  return l10n_util::GetStringFUTF16(IDS_ASSISTANT_AMBIENT_GREETING_MESSAGE,
                                    base::UTF8ToUTF16(username));
}

}  // namespace

AmbientAssistantContainerView::AmbientAssistantContainerView()
    : delegate_(Shell::Get()->assistant_controller()->view_delegate()) {
  DCHECK(delegate_);
  SetID(AmbientViewID::kAmbientAssistantContainerView);
  InitLayout();

  assistant_controller_observer_.Add(AssistantController::Get());
  AssistantUiController::Get()->GetModel()->AddObserver(this);
}

AmbientAssistantContainerView::~AmbientAssistantContainerView() {
  if (AssistantUiController::Get())
    AssistantUiController::Get()->GetModel()->RemoveObserver(this);
}

void AmbientAssistantContainerView::OnAssistantControllerDestroying() {
  AssistantUiController::Get()->GetModel()->RemoveObserver(this);
  assistant_controller_observer_.Remove(AssistantController::Get());
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
  views::FlexLayout* outer_layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  outer_layout->SetOrientation(views::LayoutOrientation::kVertical);
  outer_layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  outer_layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  auto* container = AddChildView(std::make_unique<views::View>());

  // Set a placeholder value for width. |CrossAxisAlignment::kStretch| will
  // expand the width to 100% of the parent.
  container->SetPreferredSize(
      {/*width=*/1, /*height=*/kAssistantPreferredHeightDip});
  container->SetPaintToLayer();
  container->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));

  views::FlexLayout* container_layout =
      container->SetLayoutManager(std::make_unique<views::FlexLayout>());
  constexpr int kRightPaddingDip = 8;
  container_layout->SetInteriorMargin({0, 0, 0, kRightPaddingDip});

  container_layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  container_layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  container_layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // Mic button and input query view.
  ambient_assistant_dialog_plate_ = container->AddChildView(
      std::make_unique<AmbientAssistantDialogPlate>(delegate_));

  // Response container view.
  assistant_response_container_view_ = container->AddChildView(
      std::make_unique<AssistantResponseContainerView>(delegate_));

  // Greeting label.
  const UserSession* active_user_session =
      Shell::Get()->session_controller()->GetUserSession(0);
  // TODO(meilinw): uses login user info instead as no active user session is
  // available on lock screen.
  if (active_user_session) {
    greeting_label_ = container->AddChildView(std::make_unique<views::Label>(
        GetGreetingMessage(active_user_session)));
    greeting_label_->SetEnabledColor(kTextColorSecondary);
    greeting_label_->SetFontList(
        assistant::ui::GetDefaultFontList()
            .DeriveWithSizeDelta(8)
            .DeriveWithWeight(gfx::Font::Weight::NORMAL));
    greeting_label_->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_CENTER);
  }

  // Spacer.
  views::View* spacer =
      container->AddChildView(std::make_unique<views::View>());
  // Allow the spacer to expand to push the avatar image to the end of the
  // container.
  spacer->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  // Rounded avatar image view.
  avatar_view_ = container->AddChildView(std::make_unique<views::ImageView>());
  avatar_view_->SetImageSize(
      gfx::Size(kAvatarImageSizeDip, kAvatarImageSizeDip));
  avatar_view_->SetPreferredSize(
      gfx::Size(kAvatarImageSizeDip, kAvatarImageSizeDip));
  // TODO(meilinw): uses login user info instead as no active user session is
  // available on lock screen.
  if (active_user_session) {
    gfx::ImageSkia avatar = active_user_session->user_info.avatar.image;
    if (!avatar.isNull())
      avatar_view_->SetImage(avatar);
  }

  SkPath circular_mask;
  constexpr int kClipCircleRadius = kAvatarImageSizeDip / 2;
  circular_mask.addCircle(kClipCircleRadius, kClipCircleRadius,
                          kClipCircleRadius);
  avatar_view_->SetClipPath(circular_mask);
}

BEGIN_METADATA(AmbientAssistantContainerView, views::View)
END_METADATA
}  // namespace ash

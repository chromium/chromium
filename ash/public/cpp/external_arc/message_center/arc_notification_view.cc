// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/message_center/arc_notification_view.h"

#include <algorithm>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_content_view.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_item.h"
#include "ash/public/cpp/message_center/arc_notification_constants.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/typography.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/notification_center/notification_grouping_controller.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/notification_view_base.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ash::ArcNotificationView*)

namespace {
// The animation duration must be 360ms because it's separately defined in
// Android side.
constexpr base::TimeDelta kArcNotificationAnimationDuration =
    base::Milliseconds(360);

// Create a view containing the title and message for the notification in a
// single line. This is used when a grouped child notification is in a
// collapsed parent notification.
views::Builder<views::BoxLayoutView> CreateCollapsedSummaryBuilder(
    const message_center::Notification& notification) {
  return views::Builder<views::BoxLayoutView>()
      .SetID(
          message_center::NotificationViewBase::ViewId::kCollapsedSummaryView)
      .SetInsideBorderInsets(ash::kGroupedCollapsedSummaryInsets)
      .SetBetweenChildSpacing(ash::kGroupedCollapsedSummaryLabelSpacing)
      .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
      .SetVisible(false)
      .AddChild(views::Builder<views::Label>()
                    .SetText(notification.title())
                    .SetFontList(
                        ash::TypographyProvider::Get()->ResolveTypographyToken(
                            ash::TypographyToken::kCrosButton2)))
      .AddChild(views::Builder<views::Label>()
                    .SetText(notification.message())
                    .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                    .SetTextStyle(views::style::STYLE_SECONDARY)
                    .SetFontList(
                        ash::TypographyProvider::Get()->ResolveTypographyToken(
                            ash::TypographyToken::kCrosBody2)));
}
}  // namespace

namespace ash {

DEFINE_UI_CLASS_PROPERTY_KEY(ArcNotificationView*,
                             kArcNotificationViewPropertyKey,
                             nullptr)

// static
ArcNotificationView* ArcNotificationView::FromView(views::View* view) {
  return view->GetProperty(kArcNotificationViewPropertyKey);
}

ArcNotificationView::ArcNotificationView(
    ArcNotificationItem* item,
    const message_center::Notification& notification,
    bool shown_in_popup)
    : message_center::MessageView(notification),
      item_(item),
      content_view_(new ArcNotificationContentView(item_, notification, this)),
      shown_in_popup_(shown_in_popup),
      is_group_child_(notification.group_child()) {
  DCHECK_EQ(message_center::NOTIFICATION_TYPE_CUSTOM, notification.type());
  DCHECK_EQ(kArcNotificationCustomViewType, notification.custom_view_type());

  SetProperty(kArcNotificationViewPropertyKey, this);

  item_->AddObserver(this);

  AddChildView(content_view_.get());

  AddChildView(CreateCollapsedSummaryBuilder(notification)
                   .CopyAddressTo(&collapsed_summary_view_)
                   .Build());

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets()));

  if (shown_in_popup) {
    layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
    layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF{kMessagePopupCornerRadius});
  }

  UpdateCornerRadius(message_center::kNotificationCornerRadius,
                     message_center::kNotificationCornerRadius);

  auto* const focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);
  // Focus control is delegated to its content view if it's available. So we
  // need to set the focus predicate to let `focus_ring` know the focus change
  // on the content.
  focus_ring->SetHasFocusPredicate(
      base::BindRepeating([](const views::View* view) {
        const auto* v = views::AsViewClass<ArcNotificationView>(view);
        CHECK(v);
        return v->HasFocus();
      }));
}

ArcNotificationView::~ArcNotificationView() {
  if (item_) {
    item_->RemoveObserver(this);
  }
}

void ArcNotificationView::OnContentFocused() {
  SchedulePaint();
  views::FocusRing::Get(this)->SchedulePaint();
}

void ArcNotificationView::OnContentBlurred() {
  SchedulePaint();
  views::FocusRing::Get(this)->SchedulePaint();
}

void ArcNotificationView::UpdateWithNotification(
    const message_center::Notification& notification) {
  message_center::MessageView::UpdateWithNotification(notification);
  is_group_child_ = notification.group_child();
  content_view_->Update(notification);
}

void ArcNotificationView::UpdateCornerRadius(int top_radius,
                                             int bottom_radius) {
  MessageView::UpdateCornerRadius(top_radius, bottom_radius);

  content_view_->UpdateCornerRadius(top_radius, bottom_radius);
}

void ArcNotificationView::UpdateBackgroundPainter() {
  if (is_group_child_) {
    SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
    return;
  }

  const auto* color_provider = GetColorProvider();
  const SkColor color_in_popup =
      color_provider->GetColor(cros_tokens::kCrosSysSystemBaseElevated);
  const SkColor color_in_message_center =
      color_provider->GetColor(cros_tokens::kCrosSysSystemOnBase);
  SetBackground(views::CreateBackgroundFromPainter(
      std::make_unique<message_center::NotificationBackgroundPainter>(
          top_radius(), bottom_radius(),
          shown_in_popup_ ? color_in_popup : color_in_message_center)));
}

void ArcNotificationView::UpdateControlButtonsVisibility() {
  content_view_->UpdateControlButtonsVisibility();
}

void ArcNotificationView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (content_view_->IsFocusable()) {
    // This data is never used since this view is never focused when the content
    // view is focusable.
    return;
  }
  CHECK(features::IsRenderArcNotificationsByChromeEnabled());
  message_center::MessageView::GetAccessibleNodeData(node_data);
}

message_center::NotificationControlButtonsView*
ArcNotificationView::GetControlButtonsView() const {
  return content_view_->GetControlButtonsView();
}

bool ArcNotificationView::IsExpanded() const {
  return item_ && item_->GetExpandState() ==
                      arc::mojom::ArcNotificationExpandState::EXPANDED;
}

bool ArcNotificationView::IsAutoExpandingAllowed() const {
  if (!item_) {
    return false;
  }

  // Disallow auto-expanding if the notification is bundled. This is consistent
  // behavior with Android since expanded height of bundle notification might be
  // too long vertically.
  return item_->GetNotificationType() !=
         arc::mojom::ArcNotificationType::BUNDLED;
}

void ArcNotificationView::SetExpanded(bool expanded) {
  if (!item_) {
    return;
  }

  auto expand_state = item_->GetExpandState();
  if (expanded) {
    if (expand_state == arc::mojom::ArcNotificationExpandState::COLLAPSED)
      item_->ToggleExpansion();
  } else {
    if (expand_state == arc::mojom::ArcNotificationExpandState::EXPANDED)
      item_->ToggleExpansion();
  }
}

bool ArcNotificationView::IsManuallyExpandedOrCollapsed() const {
  if (item_) {
    return item_->IsManuallyExpandedOrCollapsed();
  }
  return false;
}

void ArcNotificationView::OnContainerAnimationStarted() {
  content_view_->OnContainerAnimationStarted();
}

void ArcNotificationView::OnSettingsButtonPressed(const ui::Event& event) {
  MessageView::OnSettingsButtonPressed(event);
}

void ArcNotificationView::OnSnoozeButtonPressed(const ui::Event& event) {
  MessageView::OnSnoozeButtonPressed(event);

  if (item_) {
    return item_->OpenSnooze();
  }
}

void ArcNotificationView::OnContainerAnimationEnded() {
  content_view_->OnContainerAnimationEnded();
}

base::TimeDelta ArcNotificationView::GetBoundsAnimationDuration(
    const message_center::Notification&) const {
  return kArcNotificationAnimationDuration;
}

void ArcNotificationView::SetGroupedChildExpanded(bool expanded) {
  if (!collapsed_summary_view_) {
    return;
  }
  CHECK(features::IsRenderArcNotificationsByChromeEnabled());

  collapsed_summary_view_->SetVisible(!expanded);
  content_view_->SetVisible(expanded);
}

void ArcNotificationView::AnimateGroupedChildExpandedCollapse(bool expanded) {
  if (!collapsed_summary_view_) {
    return;
  }
  CHECK(features::IsRenderArcNotificationsByChromeEnabled());

  message_center_utils::InitLayerForAnimations(collapsed_summary_view_);
  message_center_utils::InitLayerForAnimations(content_view_);
  // Fade out `collapsed_summary_view_`, then fade in `content_view_` in
  // expanded state and vice versa in collapsed state.
  if (expanded) {
    message_center_utils::FadeOutView(
        collapsed_summary_view_,
        base::BindRepeating(
            [](base::WeakPtr<ash::ArcNotificationView> parent,
               views::View* collapsed_summary_view) {
              if (parent) {
                collapsed_summary_view->layer()->SetOpacity(1.0f);
                collapsed_summary_view->SetVisible(false);
              }
            },
            weak_factory_.GetWeakPtr(), collapsed_summary_view_),
        0, kCollapsedSummaryViewAnimationDurationMs, gfx::Tween::LINEAR,
        "Arc.NotificationView.CollapsedSummaryView.FadeOut."
        "AnimationSmoothness");
    message_center_utils::FadeInView(
        content_view_, kCollapsedSummaryViewAnimationDurationMs,
        kChildMainViewFadeInAnimationDurationMs, gfx::Tween::LINEAR,
        "Arc.NotificationView.ContentView.FadeIn.AnimationSmoothness");
    return;
  }

  message_center_utils::FadeOutView(
      content_view_,
      base::BindRepeating(
          [](base::WeakPtr<ash::ArcNotificationView> parent,
             views::View* content_view) {
            if (parent) {
              content_view->layer()->SetOpacity(1.0f);
              content_view->SetVisible(false);
            }
          },
          weak_factory_.GetWeakPtr(), content_view_),
      0, kChildMainViewFadeOutAnimationDurationMs, gfx::Tween::LINEAR,
      "Arc.NotificationView.ContentView.FadeOut.AnimationSmoothness");
  message_center_utils::FadeInView(
      collapsed_summary_view_, kChildMainViewFadeOutAnimationDurationMs,
      kCollapsedSummaryViewAnimationDurationMs, gfx::Tween::LINEAR,
      "Arc.NotificationView.CollapsedSummaryView.FadeIn.AnimationSmoothness");
}

void ArcNotificationView::AnimateSingleToGroup(
    const std::string& notification_id,
    std::string parent_id) {
  if (!collapsed_summary_view_) {
    return;
  }
  CHECK(features::IsRenderArcNotificationsByChromeEnabled());

  auto on_animation_ended = base::BindOnce(
      [](base::WeakPtr<ash::ArcNotificationView> parent,
         const std::string& notification_id, std::string parent_id) {
        if (!parent) {
          return;
        }

        auto* parent_notification =
            message_center::MessageCenter::Get()->FindNotificationById(
                parent_id);
        auto* child_notification =
            message_center::MessageCenter::Get()->FindNotificationById(
                notification_id);
        // The child and parent notifications are not guaranteed to exist.
        //  If they were deleted avoid the animation cleanup.
        if (!parent_notification || !child_notification) {
          return;
        }

        auto* grouping_controller =
            message_center_utils::GetGroupingControllerForNotificationView(
                parent.get());
        if (grouping_controller) {
          grouping_controller
              ->ConvertFromSingleToGroupNotificationAfterAnimation(
                  notification_id, parent_id, parent_notification);
        }
        parent->DeprecatedLayoutImmediately();
      },
      weak_factory_.GetWeakPtr(), notification_id, parent_id);

  std::pair<base::OnceClosure, base::OnceClosure> split =
      base::SplitOnceCallback(std::move(on_animation_ended));

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(std::move(split.first))
      .OnAborted(std::move(split.second))
      .Once()
      .SetDuration(
          base::Milliseconds(kConvertFromSingleToGroupFadeOutDurationMs));
}

void ArcNotificationView::OnSlideChanged(bool in_progress) {
  MessageView::OnSlideChanged(in_progress);
  content_view_->OnSlideChanged(in_progress);
}

void ArcNotificationView::Layout(PassKey) {
  // Setting the bounds before calling the parent to prevent double Layout.
  content_view_->SetBoundsRect(GetContentsBounds());

  LayoutSuperclass<message_center::MessageView>(this);

  // If the content view claims focus, defer focus handling to the content view.
  if (content_view_->IsFocusable()) {
    SetFocusBehavior(FocusBehavior::NEVER);
  }
}

bool ArcNotificationView::HasFocus() const {
  // In case that focus handling is deferred to the content view, asking the
  // content view about focus.
  return content_view_->IsFocusable() ? content_view_->HasFocus()
                                      : message_center::MessageView::HasFocus();
}

void ArcNotificationView::RequestFocus() {
  if (content_view_->IsFocusable())
    content_view_->RequestFocus();
  else
    message_center::MessageView::RequestFocus();
}

bool ArcNotificationView::OnKeyPressed(const ui::KeyEvent& event) {
  ui::InputMethod* input_method = content_view_->GetInputMethod();
  if (input_method) {
    ui::TextInputClient* text_input_client = input_method->GetTextInputClient();
    if (text_input_client &&
        text_input_client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE) {
      // If the focus is in an edit box, we skip the special key handling for
      // back space and return keys. So that these key events are sent to the
      // arc container correctly without being handled by the message center.
      return false;
    }
  }

  return message_center::MessageView::OnKeyPressed(event);
}

void ArcNotificationView::ChildPreferredSizeChanged(View* child) {
  PreferredSizeChanged();
}

bool ArcNotificationView::HandleAccessibleAction(
    const ui::AXActionData& action) {
  if (item_ && action.action == ax::mojom::Action::kDoDefault) {
    item_->ToggleExpansion();
    return true;
  }
  return false;
}

void ArcNotificationView::OnThemeChanged() {
  message_center::MessageView::OnThemeChanged();

  if (content_view_->background()) {
    background()->SetNativeControlColor(
        GetColorProvider()->GetColor(cros_tokens::kCrosSysSystemBaseElevated));
  }
}

void ArcNotificationView::OnItemDestroying() {
  DCHECK(item_);
  item_->RemoveObserver(this);
  item_ = nullptr;
}

aura::Window* ArcNotificationView::GetNativeContainerWindowForTest() const {
  return content_view_->GetNativeViewContainer();
}

BEGIN_METADATA(ArcNotificationView)
END_METADATA

}  // namespace ash

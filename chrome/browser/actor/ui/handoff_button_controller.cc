// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/handoff_button_controller.h"

#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/actor/resources/grit/actor_browser_resources.h"
#include "chrome/browser/actor/resources/grit/actor_common_resources.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/actor_ui_window_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/effects/SkImageFilters.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget_delegate.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#endif

namespace {

// A fixed vertical offset from the top of the window, used when the tab
// strip is not visible.
constexpr int kHandoffButtonTopOffset = 8;
constexpr int kHandoffButtonPreferredHeight = 44;
constexpr float kHandoffButtonShadowMargin = 15.0f;
constexpr float kBackgroundInset = 2.0f;
constexpr float kHandoffButtonCornerRadius = 48.0f;
constexpr int kHandoffButtonIconSize = 20;
constexpr gfx::Insets kHandoffButtonContentPadding =
    gfx::Insets::TLBR(10, 10, 10, 14);

// A customized LabelButton that shows a hand cursor on hover.
class HandoffLabelButton : public views::LabelButton {
  METADATA_HEADER(HandoffLabelButton, views::LabelButton)

 public:
  using views::LabelButton::LabelButton;
  ~HandoffLabelButton() override = default;

  // views::View:
  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return ui::mojom::CursorType::kHand;
  }
};

BEGIN_METADATA(HandoffLabelButton)
END_METADATA

// A custom BubbleFrameView that paints a gradient border.
class GradientBubbleFrameView : public views::BubbleFrameView {
  METADATA_HEADER(GradientBubbleFrameView, views::BubbleFrameView)

 public:
  GradientBubbleFrameView(const gfx::Insets& total_insets,
                          views::BubbleBorder::Arrow arrow_location,
                          const gfx::RoundedCornersF& corners)
      : views::BubbleFrameView(gfx::Insets(), total_insets),
        corner_radius_(corners) {
    auto border = std::make_unique<views::BubbleBorder>(
        arrow_location, views::BubbleBorder::Shadow::NO_SHADOW);
    border->set_draw_border_stroke(false);
    border->set_rounded_corners(corners);
    SetBubbleBorder(std::move(border));
  }
  ~GradientBubbleFrameView() override = default;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    constexpr float kShadowBlurSigma = 5.0f;
    constexpr float kShadowOffsetX = 0.0f;
    constexpr float kShadowOffsetY = 3.0f;

    gfx::RectF button_bounds_f(GetLocalBounds());
    button_bounds_f.Inset(kHandoffButtonShadowMargin);
    const float shadow_corner_radius = corner_radius_.upper_left();
    const float corner_radius = corner_radius_.upper_left();
    cc::PaintCanvas* paint_canvas = canvas->sk_canvas();
    SkRRect rrect;
    rrect.setRectXY(gfx::RectFToSkRect(button_bounds_f), corner_radius,
                    corner_radius);

    // Draw the shadow
    {
      cc::PaintCanvasAutoRestore auto_restore(paint_canvas, true);
      paint_canvas->translate(kShadowOffsetX, kShadowOffsetY);
      cc::PaintFlags shadow_flags;
      shadow_flags.setAntiAlias(true);
      SkPoint center = SkPoint::Make(button_bounds_f.CenterPoint().x(),
                                     button_bounds_f.CenterPoint().y());
      const SkColor colors[] = {
          SkColorSetARGB(255, 79, 161, 255), SkColorSetARGB(255, 79, 161, 255),
          SkColorSetARGB(255, 52, 107, 241), SkColorSetARGB(255, 52, 107, 241)};
      const SkScalar pos[] = {0.0f, 0.4f, 0.6f, 1.0f};
      std::vector<SkColor4f> color4fs;
      for (const auto& color : colors) {
        color4fs.push_back(SkColor4f::FromColor(color));
      }
      auto shader = cc::PaintShader::MakeSweepGradient(
          center.x(), center.y(), color4fs.data(), pos, color4fs.size(),
          SkTileMode::kClamp, 0, 360);
      shadow_flags.setShader(std::move(shader));
      auto blur_filter = sk_make_sp<cc::BlurPaintFilter>(
          kShadowBlurSigma, kShadowBlurSigma, SkTileMode::kDecal, nullptr);
      shadow_flags.setImageFilter(std::move(blur_filter));
      paint_canvas->drawRRect(rrect, shadow_flags);
    }

    // Create a slightly smaller rectangle for the background.
    gfx::RectF background_bounds_f = button_bounds_f;
    background_bounds_f.Inset(kBackgroundInset);
    const float background_corner_radius =
        shadow_corner_radius > kBackgroundInset
            ? shadow_corner_radius - kBackgroundInset
            : 0.f;
    SkRRect background_rrect;
    background_rrect.setRectXY(gfx::RectFToSkRect(background_bounds_f),
                               background_corner_radius,
                               background_corner_radius);
    cc::PaintFlags background_flags;
    background_flags.setAntiAlias(true);
    background_flags.setStyle(cc::PaintFlags::kFill_Style);
    background_flags.setColor(
        GetColorProvider()->GetColor(ui::kColorTextfieldBackground));
    paint_canvas->drawRRect(background_rrect, background_flags);
  }

 private:
  gfx::RoundedCornersF corner_radius_;
};

BEGIN_METADATA(GradientBubbleFrameView)
END_METADATA

std::unique_ptr<views::FrameView> CreateHandoffButtonFrameView(
    views::Widget* widget) {
  const gfx::Insets total_insets =
      gfx::Insets(kHandoffButtonShadowMargin) + gfx::Insets(kBackgroundInset);
  const gfx::RoundedCornersF corners(kHandoffButtonCornerRadius);
  auto frame_view = std::make_unique<GradientBubbleFrameView>(
      total_insets, views::BubbleBorder::Arrow::NONE, corners);
  frame_view->SetBackgroundColor(ui::kColorTextfieldBackground);
  return frame_view;
}

}  // namespace

namespace actor::ui {

using enum HandoffButtonState::ControlOwnership;
using ::ui::ImageModel;

HandoffButtonWidget::HandoffButtonWidget() = default;
HandoffButtonWidget::~HandoffButtonWidget() = default;

void HandoffButtonWidget::SetHoveredCallback(
    base::RepeatingCallback<void(bool)> callback) {
  hover_callback_ = std::move(callback);
}

void HandoffButtonWidget::OnMouseEvent(::ui::MouseEvent* event) {
  switch (event->type()) {
    case ::ui::EventType::kMouseEntered:
      hover_callback_.Run(true);
      break;
    case ::ui::EventType::kMouseExited:
      hover_callback_.Run(false);
      break;
    default:
      break;
  }
  views::Widget::OnMouseEvent(event);
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(HandoffButtonController,
                                      kHandoffButtonElementId);

HandoffButtonController::HandoffButtonController(
    views::View* anchor_view,
    ActorUiWindowController* window_controller)
    : anchor_view_(anchor_view), window_controller_(window_controller) {}

HandoffButtonController::~HandoffButtonController() = default;

void HandoffButtonController::UpdateState(HandoffButtonState state,
                                          bool is_visible,
                                          base::OnceClosure callback) {
  if (!state.is_active) {
    CloseButton(views::Widget::ClosedReason::kUnspecified);
    std::move(callback).Run();
    return;
  }
  is_visible_ = is_visible;
  ownership_ = state.controller;

  bool is_immersive = window_controller_->IsImmersiveModeEnabled();
  bool is_pinned = window_controller_->IsToolbarPinned();

  // Check if a layout change occurred that requires re-anchoring (immersive
  // mode toggled on or off, or a toolbar pin state change while in immersive
  // mode).
  bool layout_changed = (is_immersive != was_immersive_) ||
                        (is_immersive && (is_pinned != was_toolbar_pinned_));

  if (widget_ && layout_changed) {
    view_observer_.Reset();
    button_view_ = nullptr;
    widget_.reset();
    delegate_.reset();
  }

  was_immersive_ = is_immersive;
  was_toolbar_pinned_ = is_pinned;

  std::u16string text;
  std::u16string a11y_text;
  ImageModel icon;
  // TODO(crbug.com/454932877): Clean up kClient state changes if button removal
  // for kClient state is finalized.
  switch (state.controller) {
    case kActor:
      text = l10n_util::GetStringUTF16(IDS_TAKE_OVER_TASK_LABEL);
      a11y_text = l10n_util::GetStringUTF16(IDS_TAKE_OVER_TASK_A11Y_LABEL);
      icon = ImageModel::FromVectorIcon(vector_icons::kPauseIcon,
                                        ::ui::kColorLabelForeground,
                                        kHandoffButtonIconSize);
      break;
    case kClient:
      text = l10n_util::GetStringUTF16(IDS_GIVE_TASK_BACK_LABEL);
      a11y_text = l10n_util::GetStringUTF16(IDS_GIVE_TASK_BACK_A11Y_LABEL);
      icon = ImageModel::FromVectorIcon(vector_icons::kPlayArrowIcon,
                                        ::ui::kColorLabelForeground,
                                        kHandoffButtonIconSize);
      break;
  }

  // If the widget doesn't exist, create it with the correct initial state.
  if (!widget_) {
    CreateAndShowButton(text, a11y_text, icon);
  } else if (button_view_) {
    // If it already exists, update its content.
    button_view_->SetText(text);
    button_view_->SetAccessibleDescription(a11y_text);
    button_view_->SetImageModel(views::Button::STATE_NORMAL, icon);
    UpdateBounds();
  }

  if (is_immersive) {
    widget_->SetZOrderLevel(::ui::ZOrderLevel::kFloatingUIElement);
  } else {
    widget_->SetZOrderLevel(::ui::ZOrderLevel::kNormal);
  }

  if (is_visible_) {
    widget_->ShowInactive();
  } else {
    widget_->Hide();
  }
  std::move(callback).Run();
}

void HandoffButtonController::CreateAndShowButton(
    const std::u16string& text,
    const std::u16string& a11y_text,
    const ImageModel& icon) {
  CHECK(!widget_);

  // Create the button view.
  auto button_view = std::make_unique<HandoffLabelButton>(
      base::BindRepeating(&HandoffButtonController::OnButtonPressed,
                          weak_ptr_factory_.GetWeakPtr()),
      text);
  button_view_ = button_view.get();
  button_view_->SetAccessibleDescription(a11y_text);
  button_view_->SetEnabledTextColors(::ui::kColorLabelForeground);
  button_view_->SetImageModel(views::Button::STATE_NORMAL, icon);
  button_view_->SetProperty(views::kElementIdentifierKey,
                            kHandoffButtonElementId);
  button_view_->SetLabelStyle(views::style::STYLE_BODY_3_MEDIUM);
  button_view_->SetBorder(views::CreatePaddedBorder(
      button_view_->CreateDefaultBorder(),
      kHandoffButtonContentPadding - gfx::Insets(kBackgroundInset)));
  view_observer_.Observe(button_view_.get());

  auto widget_delegate = std::make_unique<views::WidgetDelegate>();
  widget_delegate->SetContentsView(std::move(button_view));
  widget_delegate->SetModalType(::ui::mojom::ModalType::kNone);
  widget_delegate->SetAccessibleWindowRole(ax::mojom::Role::kAlert);
  widget_delegate->SetShowCloseButton(false);
  widget_delegate->SetFrameViewFactory(
      base::BindRepeating(&CreateHandoffButtonFrameView));
  delegate_ = std::move(widget_delegate);

  // Create the Widget using the delegate.
  auto widget = std::make_unique<HandoffButtonWidget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::Ownership::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_BUBBLE);
  params.delegate = delegate_.get();
  params.parent = anchor_view_->GetWidget()->GetNativeView();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.remove_standard_frame = true;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.autosize = false;
  params.name = "HandoffButtonWidget";
  widget->Init(std::move(params));

  widget_ = std::move(widget);
  widget_->SetHoveredCallback(
      base::BindRepeating(&HandoffButtonController::UpdateButtonHoverStatus,
                          weak_ptr_factory_.GetWeakPtr()));
  widget_->MakeCloseSynchronous(
      base::BindOnce(&HandoffButtonController::OnWidgetDestroying,
                     weak_ptr_factory_.GetWeakPtr()));

  UpdateBounds();
}

gfx::Rect HandoffButtonController::GetHandoffButtonBounds() {
  CHECK(widget_);
  CHECK(anchor_view_);
  gfx::Size preferred_size = widget_->GetContentsView()->GetPreferredSize();
  preferred_size.set_height(kHandoffButtonPreferredHeight);

  const gfx::Rect anchor_bounds = anchor_view_->GetBoundsInScreen();

  const int x =
      anchor_bounds.x() + (anchor_bounds.width() - preferred_size.width()) / 2;

  // Calculate the Y coordinate based on tab strip visibility.
  bool is_tab_strip_visible =
      tab_interface_
          ? tab_interface_->GetBrowserWindowInterface()->IsTabStripVisible()
          : false;

  const int y =
      is_tab_strip_visible
          // Vertically center the button on the top edge of the anchor.
          ? anchor_bounds.y() - kHandoffButtonPreferredHeight
          // Position with a fixed offset from the top of the anchor.
          : anchor_bounds.y() - kHandoffButtonTopOffset;

  return gfx::Rect({x, y}, preferred_size);
}

void HandoffButtonController::OnWidgetDestroying(
    views::Widget::ClosedReason reason) {
  view_observer_.Reset();
  button_view_ = nullptr;
  widget_.reset();
  delegate_.reset();
}

void HandoffButtonController::CloseButton(views::Widget::ClosedReason reason) {
  // Before closing the button, reset hover and focus status to prevent stale
  // state propagation.
  if (base::FeatureList::IsEnabled(
          features::kGlicHandoffButtonResetFocusAndHoverStatus)) {
    UpdateButtonHoverStatus(false);
    UpdateButtonFocusStatus(false);
  }
  if (widget_ && !widget_->IsClosed()) {
    widget_->CloseWithReason(reason);
  }
}

void HandoffButtonController::UpdateButtonHoverStatus(bool is_hovered) {
  is_hovering_ = is_hovered;
  if (auto* tab_controller = GetTabController()) {
    tab_controller->OnHandoffButtonHoverStatusChanged();
  }
}

void HandoffButtonController::OnButtonPressed() {
  // If the Actor is currently in control, pressing the button
  // flips the state and pauses the task.
  if (auto* tab_controller = GetTabController()) {
    if (ownership_ == kActor) {
      tab_controller->SetActorTaskPaused();
#if BUILDFLAG(ENABLE_GLIC)
      BrowserWindowInterface* bwi = tab_interface_->GetBrowserWindowInterface();
      auto* glic_service =
          glic::GlicKeyedServiceFactory::GetGlicKeyedService(bwi->GetProfile());
      if (glic_service) {
        glic_service->ToggleUI(bwi, /*prevent_close=*/true,
                               glic::mojom::InvocationSource::kHandoffButton);
      }
#endif
    } else {
      tab_controller->SetActorTaskResume();
    }
  }
  actor::ui::LogHandoffButtonClick(ownership_);
}

void HandoffButtonController::UpdateBounds() {
  if (widget_) {
    widget_->SetBounds(GetHandoffButtonBounds());
  }
}

base::ScopedClosureRunner HandoffButtonController::RegisterTabInterface(
    tabs::TabInterface* tab_interface) {
  tab_interface_ = tab_interface;
  return base::ScopedClosureRunner(
      base::BindOnce(&HandoffButtonController::UnregisterTabInterface,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HandoffButtonController::UnregisterTabInterface() {
  tab_interface_ = nullptr;
  UpdateState(HandoffButtonState(), /*is_visible=*/false, base::DoNothing());
}

ActorUiTabControllerInterface* HandoffButtonController::GetTabController() {
  return tab_interface_
             ? ActorUiTabControllerInterface::From(tab_interface_.get())
             : nullptr;
}

bool HandoffButtonController::IsHovering() {
  return is_hovering_;
}

void HandoffButtonController::UpdateButtonFocusStatus(bool is_focused) {
  is_focused_ = is_focused;
  if (auto* tab_controller = GetTabController()) {
    tab_controller->OnHandoffButtonFocusStatusChanged();
  }
}

void HandoffButtonController::OnViewFocused(views::View* observed_view) {
  UpdateButtonFocusStatus(/*is_focused=*/true);
}

void HandoffButtonController::OnViewBlurred(views::View* observed_view) {
  UpdateButtonFocusStatus(/*is_focused=*/false);
}

bool HandoffButtonController::IsFocused() {
  return is_focused_;
}

base::WeakPtr<HandoffButtonController> HandoffButtonController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace actor::ui

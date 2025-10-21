// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/handoff_button_controller.h"

#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/actor_ui_window_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/effects/SkImageFilters.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

// A fixed vertical offset from the top of the window, used when the tab
// strip is not visible.
constexpr int kHandoffButtonTopOffset = 8;
constexpr int kHandoffButtonPreferredHeight = 44;
constexpr float kHandoffButtonShadowMargin = 15.0f;
constexpr float kHandoffButtonCornerRadius = 48.0f;
constexpr int kHandoffButtonIconSize = 20;

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
    constexpr float kBackgroundInset = 2.0f;

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
          SkColorSetARGB(255, 117, 93, 252), SkColorSetARGB(255, 93, 93, 252),
          SkColorSetARGB(255, 68, 137, 255), SkColorSetARGB(255, 68, 137, 255)};
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
  const gfx::Insets content_padding = gfx::Insets::VH(12, 20);
  const gfx::Insets total_insets =
      content_padding + gfx::Insets(kHandoffButtonShadowMargin);
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
    tabs::TabInterface& tab_interface)
    : tab_interface_(tab_interface) {}

HandoffButtonController::~HandoffButtonController() = default;

void HandoffButtonController::UpdateState(const HandoffButtonState& state,
                                          bool is_visible) {
  if (!state.is_active) {
    CloseButton(views::Widget::ClosedReason::kUnspecified);
    return;
  }
  is_visible_ = is_visible;
  ownership_ = state.controller;

  std::u16string text;
  ImageModel icon;
  switch (state.controller) {
    case kActor:
      text = TAKE_OVER_TASK_TEXT;
      icon = ImageModel::FromVectorIcon(vector_icons::kPauseIcon,
                                        ::ui::kColorLabelForeground,
                                        kHandoffButtonIconSize);
      break;
    case kClient:
      text = GIVE_TASK_BACK_TEXT;
      icon = ImageModel::FromVectorIcon(vector_icons::kPlayArrowIcon,
                                        ::ui::kColorLabelForeground,
                                        kHandoffButtonIconSize);
      break;
  }

  // If the widget doesn't exist, create it with the correct initial state.
  if (!widget_) {
    CreateAndShowButton(text, icon);
  } else {
    // If it already exists, update its content.
    button_view_->SetText(text);
    button_view_->SetImageModel(views::Button::STATE_NORMAL, icon);
    UpdateBounds();
  }

  UpdateVisibility();
}

void HandoffButtonController::CreateAndShowButton(const std::u16string& text,
                                                  const ImageModel& icon) {
  CHECK(!widget_);

  auto* tab_dialog_manager = GetTabDialogManager();

  // Create the button view.
  auto button_view = std::make_unique<views::LabelButton>(
      base::BindRepeating(&HandoffButtonController::OnButtonPressed,
                          weak_ptr_factory_.GetWeakPtr()),
      text);
  button_view_ = button_view.get();
  button_view_->SetEnabledTextColors(::ui::kColorLabelForeground);
  button_view_->SetImageModel(views::Button::STATE_NORMAL, icon);
  button_view_->SetProperty(views::kElementIdentifierKey,
                            kHandoffButtonElementId);

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
      views::Widget::InitParams::Ownership::CLIENT_OWNS_WIDGET);
  params.delegate = delegate_.get();
  params.parent = tab_dialog_manager->GetHostWidget()->GetNativeView();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.remove_standard_frame = true;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.autosize = false;
  params.name = "HandoffButtonWidget";
  widget->Init(std::move(params));

  auto tab_dialog_params = std::make_unique<tabs::TabDialogManager::Params>();
  tab_dialog_params->close_on_navigate = false;
  tab_dialog_params->close_on_detach = true;
  tab_dialog_params->disable_input = false;
  tab_dialog_params->animated = false;
  tab_dialog_params->should_show_inactive = true;
  tab_dialog_params->should_show_callback = base::BindRepeating(
      &HandoffButtonController::ShouldShowButton, base::Unretained(this));
  tab_dialog_params->get_dialog_bounds =
      base::BindRepeating(&HandoffButtonController::GetHandoffButtonBounds,
                          base::Unretained(this), widget.get());

  tab_dialog_manager->ShowDialog(widget.get(), std::move(tab_dialog_params));
  widget_ = std::move(widget);
  widget_->SetHoveredCallback(
      base::BindRepeating(&HandoffButtonController::UpdateButtonHoverStatus,
                          weak_ptr_factory_.GetWeakPtr()));

  widget_->MakeCloseSynchronous(base::BindOnce(
      &HandoffButtonController::CloseButton, weak_ptr_factory_.GetWeakPtr()));
}

void HandoffButtonController::ShouldShowButton(bool& show) {
  show = is_visible_;
}

gfx::Rect HandoffButtonController::GetHandoffButtonBounds(
    views::Widget* widget) {
  gfx::Size preferred_size = widget->GetContentsView()->GetPreferredSize();
  preferred_size.set_height(kHandoffButtonPreferredHeight);

  // TODO(crbug.com/447624564): After migrating the Handoff button off the TDM,
  // explore parenting the bounds of the widget on the contents webview bounds
  // instead.
  auto* anchor_view =
      BrowserElementsViews::From(tab_interface_->GetBrowserWindowInterface())
          ->RetrieveView(kActiveContentsWebViewRetrievalId);
  if (auto* window_controller = ActorUiWindowController::From(
          tab_interface_->GetBrowserWindowInterface())) {
    if (auto* contents_controller =
            window_controller->GetControllerForWebContents(
                tab_interface_->GetContents())) {
      anchor_view = contents_controller->contents_container_view();
    }
  }
  if (!anchor_view) {
    return gfx::Rect(preferred_size);
  }
  const gfx::Rect anchor_bounds = anchor_view->GetBoundsInScreen();

  const int x =
      anchor_bounds.x() + (anchor_bounds.width() - preferred_size.width()) / 2;

  // Calculate the Y coordinate based on tab strip visibility.
  const bool is_tab_strip_visible =
      tab_interface_->GetBrowserWindowInterface()->IsTabStripVisible();

  const int y =
      is_tab_strip_visible
          // Vertically center the button on the top edge of the anchor.
          ? anchor_bounds.y() - preferred_size.height()
          // Position with a fixed offset from the top of the anchor.
          : anchor_bounds.y() - kHandoffButtonTopOffset;

  return gfx::Rect({x, y}, preferred_size);
}

void HandoffButtonController::CloseButton(views::Widget::ClosedReason reason) {
  button_view_ = nullptr;
  if (widget_) {
    widget_->CloseNow();
    widget_.reset();
    delegate_.reset();
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
    } else {
      tab_controller->SetActorTaskResume();
    }
  }
  actor::ui::LogHandoffButtonClick(ownership_);
}

void HandoffButtonController::UpdateBounds() {
  GetTabDialogManager()->UpdateModalDialogBounds();
}

void HandoffButtonController::UpdateVisibility() {
  GetTabDialogManager()->UpdateDialogVisibility();
}

tabs::TabDialogManager* HandoffButtonController::GetTabDialogManager() {
  auto* features = tab_interface_->GetTabFeatures();
  CHECK(features);
  return features->tab_dialog_manager();
}

ActorUiTabControllerInterface* HandoffButtonController::GetTabController() {
  return ActorUiTabControllerInterface::From(&tab_interface_.get());
}

bool HandoffButtonController::IsHovering() {
  return is_hovering_;
}

}  // namespace actor::ui

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/idle_app_name_notification_view.h"

#include <string>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/utility/wm_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/extension.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ui {
class LayerAnimationSequence;
}  // namespace ui

namespace ash {
namespace {

// Color of the text of the warning message.
const SkColor kTextColor = SK_ColorBLACK;

// Color of the text of the warning message.
const SkColor kErrorTextColor = SK_ColorRED;

// Color of the window background.
const SkColor kWindowBackgroundColor = SK_ColorWHITE;

// Radius of the rounded corners of the window.
const int kWindowCornerRadius = 4;

// Creates and shows the message widget for |view| with |animation_time_ms|.
void CreateAndShowWidget(views::WidgetDelegateView* delegate,
                         int animation_time_ms) {
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();
  gfx::Size view_size = delegate->GetPreferredSize();
  gfx::Rect bounds((display_size.width() - view_size.width()) / 2,
                   -view_size.height(), view_size.width(), view_size.height());
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.accept_events = false;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.delegate = delegate;
  params.bounds = bounds;
  ash_util::SetupWidgetInitParamsForContainer(
      &params, ash::kShellWindowId_SettingBubbleContainer);
  views::Widget* widget = new views::Widget;
  widget->Init(std::move(params));
  gfx::NativeView native_view = widget->GetNativeView();
  native_view->SetName("KioskIdleAppNameNotification");

  // Note: We cannot use the Window show/hide animations since they are disabled
  // for kiosk by command line.
  ui::LayerAnimator* animator =
      new ui::LayerAnimator(base::Milliseconds(animation_time_ms));
  native_view->layer()->SetAnimator(animator);
  widget->Show();

  // We don't care about the show animation since it is off screen, so stop the
  // started animation and move the message into view.
  animator->StopAnimating();
  bounds.set_y((display_size.height() - view_size.height()) / 20);
  widget->SetBounds(bounds);

  // Allow to use the message for spoken feedback.
  delegate->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}

}  // namespace

// The class which implements the content view for the message.
class IdleAppNameNotificationDelegateView
    : public views::WidgetDelegateView,
      public ui::ImplicitAnimationObserver {
  METADATA_HEADER(IdleAppNameNotificationDelegateView,
                  views::WidgetDelegateView)

 public:
  // An idle message which will get shown from the caller and hides itself after
  // a time, calling |owner->CloseMessage| to inform the owner that it got
  // destroyed. The |app_name| is a string which gets used as message and
  // |error| is true if something is not correct.
  // |message_visibility_time_in_ms| ms's after creation the message will start
  // to remove itself from the screen.
  IdleAppNameNotificationDelegateView(IdleAppNameNotificationView* owner,
                                      const std::u16string& app_name,
                                      bool error,
                                      int message_visibility_time_in_ms)
      : owner_(owner), widget_closed_(false) {
    ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
    // Add the application name label to the message.
    AddLabel(app_name, rb->GetFontList(ui::ResourceBundle::BoldFont),
             error ? kErrorTextColor : kTextColor);
    SetLayoutManager(std::make_unique<views::FillLayout>());

    // Set a timer which will trigger to remove the message after the given
    // time.
    hide_timer_.Start(FROM_HERE,
                      base::Milliseconds(message_visibility_time_in_ms), this,
                      &IdleAppNameNotificationDelegateView::RemoveMessage);

    GetViewAccessibility().SetRole(ax::mojom::Role::kAlert);
    GetViewAccessibility().SetName(app_name);
  }

  IdleAppNameNotificationDelegateView(
      const IdleAppNameNotificationDelegateView&) = delete;
  IdleAppNameNotificationDelegateView& operator=(
      const IdleAppNameNotificationDelegateView&) = delete;

  ~IdleAppNameNotificationDelegateView() override {
    // The widget is already closing, but the other cleanup items need to be
    // performed.
    widget_closed_ = true;
    Close();
  }

  // Close the widget immediately. This can be called from the owner or from
  // this class.
  void Close() {
    // Stop the timer (if it was running).
    hide_timer_.Stop();
    // Inform our owner that we are going away.
    if (owner_) {
      IdleAppNameNotificationView* owner = owner_;
      owner_ = nullptr;
      owner->CloseMessage();
    }
    // Close the owning widget - if required.
    if (!widget_closed_) {
      widget_closed_ = true;
      GetWidget()->Close();
    }
  }

  // Animate the window away (and close once done).
  void RemoveMessage() {
    aura::Window* widget_view = GetWidget()->GetNativeView();
    ui::Layer* layer = widget_view->layer();
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.AddObserver(this);
    gfx::Rect rect = widget_view->bounds();
    rect.set_y(-GetPreferredSize().height());
    layer->SetBounds(rect);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(kWindowBackgroundColor);
    canvas->DrawRoundRect(GetLocalBounds(), kWindowCornerRadius, flags);
    views::WidgetDelegateView::OnPaint(canvas);
  }

  // ImplicitAnimationObserver overrides
  void OnImplicitAnimationsCompleted() override { Close(); }

 private:
  // Adds the label to the view, using |text| with a |font| and a |text_color|.
  void AddLabel(const std::u16string& text,
                const gfx::FontList& font,
                SkColor text_color) {
    views::Label* label = new views::Label;
    label->SetText(text);
    label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    label->SetFontList(font);
    label->SetEnabledColor(text_color);
    label->SetAutoColorReadabilityEnabled(false);
    AddChildView(label);
  }

  // A timer which calls us to remove the message from the screen.
  base::OneShotTimer hide_timer_;

  // The owner of this message which needs to get notified when the message
  // closes.
  raw_ptr<IdleAppNameNotificationView> owner_;

  // True if the widget got already closed.
  bool widget_closed_;
};

IdleAppNameNotificationView::IdleAppNameNotificationView(
    int message_visibility_time_in_ms,
    int animation_time_ms,
    const extensions::Extension* extension)
    : view_(nullptr) {
  ShowMessage(message_visibility_time_in_ms, animation_time_ms, extension);
}

IdleAppNameNotificationView::~IdleAppNameNotificationView() {
  CloseMessage();
}

void IdleAppNameNotificationView::CloseMessage() {
  if (view_) {
    IdleAppNameNotificationDelegateView* view = view_;
    view_ = nullptr;
    view->Close();
  }
}

bool IdleAppNameNotificationView::IsVisible() {
  return view_ != nullptr;
}

std::u16string IdleAppNameNotificationView::GetShownTextForTest() {
  ui::AXNodeData node_data;
  DCHECK(view_);
  view_->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  return node_data.GetString16Attribute(ax::mojom::StringAttribute::kName);
}

void IdleAppNameNotificationView::ShowMessage(
    int message_visibility_time_in_ms,
    int animation_time_ms,
    const extensions::Extension* extension) {
  DCHECK(!view_);

  std::u16string app_name;
  bool error = false;
  if (extension &&
      !base::ContainsOnlyChars(extension->name(), base::kWhitespaceASCII)) {
    app_name = base::UTF8ToUTF16(extension->name());
  } else {
    error = true;
    app_name = l10n_util::GetStringUTF16(
        IDS_IDLE_APP_NAME_UNKNOWN_APPLICATION_NOTIFICATION);
  }

  view_ = new IdleAppNameNotificationDelegateView(
      this,
      app_name,
      error,
      message_visibility_time_in_ms + animation_time_ms);
  CreateAndShowWidget(view_, animation_time_ms);
}

BEGIN_METADATA(IdleAppNameNotificationDelegateView)
END_METADATA

}  // namespace ash

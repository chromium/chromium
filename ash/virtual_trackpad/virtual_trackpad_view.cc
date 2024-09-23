// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/virtual_trackpad/virtual_trackpad_view.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/blurred_background_shield.h"
#include "ash/wm/window_util.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/gfx/canvas.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

// Amount to multiply each scroll event by. Makes the feature easier to use.
constexpr int kScrollMultiplier = 3;

// The number of fingers for the fake scroll events.
constexpr int kDefaultFingers = 3;
constexpr gfx::Size kDefaultSize(400, 400);
constexpr int kRoundedCorner = 30;
constexpr SkColor kTrackpadColor = SkColorSetARGB(0xFF, 0x66, 0x66, 0x66);
constexpr int kTrackpadBetweenChildSpacing = 10;
constexpr SkColor kTrackpadBorderColor = SK_ColorBLUE;
constexpr int kTrackpadBorderThickness = 6;
constexpr float kTrackpadAspectRatio = 1.4f;
constexpr float kAffordanceCircleRadius = 5.f;

constexpr float kTrackpadContainerOpacity = 0.6f;
constexpr int kTrackpadContainerPadding = 15;
constexpr SkColor kTrackpadContainerBackgroundColor = SK_ColorWHITE;

constexpr gfx::Size kButtonSize(48, 48);
constexpr float kButtonRounding = 32.f;
constexpr SkColor kSelectedTextColor = SK_ColorBLUE;
constexpr SkColor kUnselectedTextColor = SK_ColorWHITE;

views::Widget* g_fake_trackpad_widget = nullptr;

using FingerButtonOnClick = views::Button::PressedCallback;

// Creates a button to switch the number of fingers to perform gestures with.
// The button is housed inside `finger_buttons_panel_` and is tracked by
std::unique_ptr<views::LabelButton> CreateFingerButton(
    int num_finger,
    FingerButtonOnClick callback) {
  std::unique_ptr<views::LabelButton> finger_button =
      views::Builder<views::LabelButton>()
          .SetCallback(std::move(callback))
          .SetText(base::NumberToString16(num_finger))
          .SetPreferredSize(kButtonSize)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER)
          .SetBackground(
              views::CreateRoundedRectBackground(SK_ColorGRAY, kButtonRounding))
          .Build();
  auto* finger_button_ink_drop_host = views::InkDrop::Get(finger_button.get());
  finger_button_ink_drop_host->SetMode(
      views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER);
  views::InkDrop::UseInkDropForFloodFillRipple(finger_button_ink_drop_host);
  views::InstallCircleHighlightPathGenerator(finger_button.get());
  return finger_button;
}

}  // namespace

// -----------------------------------------------------------------------------
// TrackpadInternalSurfaceView:

// Captures mouse events and formats them before sending them to the event sink.
class TrackpadInternalSurfaceView : public views::View {
  METADATA_HEADER(TrackpadInternalSurfaceView, views::View)

 public:
  TrackpadInternalSurfaceView() = default;
  TrackpadInternalSurfaceView(const TrackpadInternalSurfaceView&) = delete;
  TrackpadInternalSurfaceView& operator=(const TrackpadInternalSurfaceView&) =
      delete;
  ~TrackpadInternalSurfaceView() override = default;

  void set_fingers(int fingers) { fingers_ = fingers; }
  int fingers() const { return fingers_; }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);

    cc::PaintFlags trackpad_flags;
    trackpad_flags.setStyle(cc::PaintFlags::kFill_Style);
    trackpad_flags.setColor(kTrackpadColor);
    trackpad_flags.setAntiAlias(true);

    gfx::Rect border_bounds = GetLocalBounds();
    canvas->DrawRoundRect(border_bounds, kRoundedCorner, trackpad_flags);

    border_bounds.Inset(kTrackpadBorderThickness / 2);
    cc::PaintFlags border_flags;
    border_flags.setStyle(cc::PaintFlags::kStroke_Style);
    border_flags.setColor(SK_ColorBLUE);
    border_flags.setAntiAlias(true);
    border_flags.setStrokeWidth(kTrackpadBorderThickness);
    canvas->DrawRoundRect(border_bounds, kRoundedCorner, border_flags);

    if (!scroll_data_) {
      return;
    }

    // Draw an affordance.
    cc::PaintFlags circle_flags;
    circle_flags.setStyle(cc::PaintFlags::kFill_Style);
    circle_flags.setColor(kTrackpadBorderColor);
    canvas->DrawCircle(scroll_data_->current_location, kAffordanceCircleRadius,
                       circle_flags);
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    scroll_data_ = ScrollData{event.location_f(), event.location_f()};
    SchedulePaint();
    GenerateScrollEvent(ui::EventType::kScrollFlingCancel, event);
    return true;
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    if (!scroll_data_) {
      return true;
    }

    // We generate the scroll event before we update `current_location` because
    // calculating the current scroll event's offset requires us to know where
    // it began, which is the `current_location` from the previous scroll.
    GenerateScrollEvent(ui::EventType::kScroll, event);
    CHECK(scroll_data_);
    scroll_data_->current_location = event.location_f();
    SchedulePaint();
    return true;
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (!scroll_data_) {
      return;
    }

    GenerateScrollEvent(ui::EventType::kScrollFlingStart, event);
    scroll_data_.reset();
    SchedulePaint();
  }

 private:
  // A struct containing the relevant data during a scroll session.
  struct ScrollData {
    gfx::PointF initial_location;
    gfx::PointF current_location;
  };

  // Creates a scroll event based on the mouse event on the trackpad and sends
  // it to the root window host.
  void GenerateScrollEvent(ui::EventType type, const ui::MouseEvent& event) {
    CHECK(scroll_data_);

    // `event.location_f()` is the position of the current mouse event while
    // `scroll_data_->current_location` is the position of the last mouse event.
    const gfx::Vector2dF distance =
        event.location_f() - scroll_data_->current_location;
    if (type == ui::EventType::kScrollFlingCancel) {
      CHECK_EQ(gfx::Vector2dF(), distance);
    }

    // Mimic the true behavior of scroll events by initially flipping the value
    // of vertical scroll offsets before they get sent further up the chain.
    const int y_multiplier =
        kScrollMultiplier * (window_util::IsNaturalScrollOn() ? 1 : -1);
    const gfx::PointF location = event.location_f();
    const gfx::PointF root_location = event.root_location_f();
    ui::ScrollEvent scroll_event(
        type, location, root_location, ui::EventTimeForNow(), /*flags=*/0,
        distance.x() * kScrollMultiplier, distance.y() * y_multiplier,
        /*x_offset_ordinal=*/0,
        /*y_offset_ordinal=*/0, fingers_);
    auto* host = GetWidget()->GetNativeWindow()->GetRootWindow()->GetHost();
    CHECK(host);
    std::ignore = host->SendEventToSink(&scroll_event);
  }

  // Contains the data during a scroll session. Empty when no scroll is
  // underway.
  std::optional<ScrollData> scroll_data_;

  int fingers_ = kDefaultFingers;
};

BEGIN_METADATA(TrackpadInternalSurfaceView)
END_METADATA

// -----------------------------------------------------------------------------
// VirtualTrackpadView:

VirtualTrackpadView::VirtualTrackpadView() {
  // Create the two children of this container.
  views::Label* finger_panel_label;
  finger_buttons_panel_ = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetBetweenChildSpacing(kTrackpadBetweenChildSpacing)
          .AddChild(
              views::Builder<views::Label>()
                  .CopyAddressTo(&finger_panel_label)
                  .SetText(u"Fingers")
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT))
          .Build());
  finger_buttons_panel_->SetFlexForView(finger_panel_label, 1);

  trackpad_view_ =
      AddChildView(std::make_unique<TrackpadInternalSurfaceView>());

  // Add the buttons that allow us to choose how many fingers our generated
  // scroll events should have.
  for (int num_finger : {3, 4}) {
    finger_buttons_[num_finger] =
        finger_buttons_panel_->AddChildView(CreateFingerButton(
            num_finger,
            base::BindRepeating(&VirtualTrackpadView::OnFingerButtonPressed,
                                base::Unretained(this), num_finger)));
  }
  UpdateFingerButtonsColors();

  SetPaintToLayer();
  layer()->SetOpacity(kTrackpadContainerOpacity);

  SetBorder(views::CreateEmptyBorder(kTrackpadContainerPadding));
  SetBackground(
      views::CreateSolidBackground(kTrackpadContainerBackgroundColor));

  blurred_background_ = std::make_unique<BlurredBackgroundShield>(
      this, SK_ColorTRANSPARENT, ColorProvider::kBackgroundBlurSigma,
      gfx::RoundedCornersF(
          static_cast<float>(chromeos::kTopCornerRadiusWhenRestored)));
}

VirtualTrackpadView::~VirtualTrackpadView() = default;

// static
void VirtualTrackpadView::Toggle() {
  // If we have a real trackpad, we should use that instead.
  if (!ui::DeviceDataManager::GetInstance()->GetTouchpadDevices().empty()) {
    return;
  }

  if (g_fake_trackpad_widget) {
    g_fake_trackpad_widget->Close();
    g_fake_trackpad_widget = nullptr;
    return;
  }

  auto delegate = std::make_unique<views::WidgetDelegate>();
  delegate->RegisterWindowClosingCallback(
      base::BindOnce([]() { g_fake_trackpad_widget = nullptr; }));
  delegate->SetOwnedByWidget(true);
  delegate->SetCanResize(true);
  delegate->SetTitle(u"Virtual Trackpad Simulator");

  // `SetContentsView()` must be done through the delegate. If we wanted to call
  // it directly, through `g_fake_trackpad_widget`, it would cause a CHECK to
  // fail because it is a widget with non-client views due to it being a
  // `TYPE_WINDOW`.
  delegate->SetContentsView(std::make_unique<VirtualTrackpadView>());

  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = delegate.release();
  // TODO(b/252556382): The bounds and root should be where the user last
  // closed the window if any.
  params.parent = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                      kShellWindowId_OverlayContainer);
  params.bounds = gfx::Rect(kDefaultSize);
  params.name = "VirtualTrackpadWidget";
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.accept_events = true;

  // The widget is owned by its native widget.
  g_fake_trackpad_widget = new views::Widget(std::move(params));
  g_fake_trackpad_widget->Show();

  // Used to extend bounds on the virtual trackpad window for resizing. Note
  // that we cannot use
  // `window_util::InstallResizeHandleWindowTargeterForWindow` since the virtual
  // trackpad window is not a toplevel window.
  auto targeter = std::make_unique<aura::WindowTargeter>();
  targeter->SetInsets(gfx::Insets(-chromeos::kResizeOutsideBoundsSize));
  g_fake_trackpad_widget->GetNativeWindow()->SetEventTargeter(
      std::move(targeter));
}

void VirtualTrackpadView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);

  // The height of the finger buttons container stays the same while the width
  // matches the parent width.
  gfx::Rect finger_buttons_bounds(GetContentsBounds());
  finger_buttons_bounds.set_height(
      finger_buttons_panel_->GetPreferredSize().height());
  finger_buttons_panel_->SetBoundsRect(finger_buttons_bounds);

  // Bounds after `finger_buttons_panel_` has been positioned. The trackpad
  // container keeps a certain aspect ratio so it's always in a rectangular
  // shape (even while resizing) that looks similar to an actual trackpad.
  gfx::Rect remaining_bounds(GetContentsBounds());
  remaining_bounds.Inset(
      gfx::Insets::TLBR(finger_buttons_bounds.height(), 0, 0, 0));

  auto trackpad_size = remaining_bounds.size();
  const float trackpad_preferred_width =
      remaining_bounds.height() * kTrackpadAspectRatio;
  if (trackpad_preferred_width < remaining_bounds.width()) {
    trackpad_size.set_width(trackpad_preferred_width);
  } else {
    trackpad_size.set_height(remaining_bounds.width() / kTrackpadAspectRatio);
  }
  remaining_bounds.ClampToCenteredSize(trackpad_size);
  trackpad_view_->SetBoundsRect(remaining_bounds);
}

// static
views::Widget* VirtualTrackpadView::GetWidgetForTesting() {
  return g_fake_trackpad_widget;
}

void VirtualTrackpadView::OnFingerButtonPressed(int num_fingers) {
  if (num_fingers != trackpad_view_->fingers()) {
    trackpad_view_->set_fingers(num_fingers);
    UpdateFingerButtonsColors();
  }
}

void VirtualTrackpadView::UpdateFingerButtonsColors() {
  constexpr views::Button::ButtonState kStates[] = {
      views::Button::STATE_NORMAL, views::Button::STATE_HOVERED,
      views::Button::STATE_PRESSED, views::Button::STATE_DISABLED};

  const int selected_finger_count = trackpad_view_->fingers();
  for (const auto& [finger, button] : finger_buttons_) {
    const bool active = finger == selected_finger_count;

    for (auto state : kStates) {
      button->SetTextColor(state,
                           active ? kSelectedTextColor : kUnselectedTextColor);
    }
    button->SetHighlighted(active);
  }
}

views::View* VirtualTrackpadView::GetTrackpadViewForTesting() {
  return trackpad_view_;
}

BEGIN_METADATA(VirtualTrackpadView)
END_METADATA

}  // namespace ash

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/message_center/arc_notification_content_view.h"

#include <memory>

#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_surface.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_view.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/notification_center/ash_notification_control_button_factory.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observation.h"
#include "components/exo/notification_surface.h"
#include "components/exo/surface.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class ArcNotificationContentView::MouseEnterExitHandler
    : public ui::EventHandler {
 public:
  explicit MouseEnterExitHandler(ArcNotificationContentView* owner)
      : owner_(owner) {
    DCHECK(owner);
  }

  MouseEnterExitHandler(const MouseEnterExitHandler&) = delete;
  MouseEnterExitHandler& operator=(const MouseEnterExitHandler&) = delete;

  ~MouseEnterExitHandler() override = default;

  // ui::EventHandler
  void OnMouseEvent(ui::MouseEvent* event) override {
    ui::EventHandler::OnMouseEvent(event);
    if (event->type() == ui::EventType::kMouseEntered ||
        event->type() == ui::EventType::kMouseExited) {
      owner_->UpdateControlButtonsVisibility();
    }
  }

 private:
  const raw_ptr<ArcNotificationContentView> owner_;
};

class ArcNotificationContentView::EventForwarder : public ui::EventHandler {
 public:
  explicit EventForwarder(ArcNotificationContentView* owner) : owner_(owner) {}

  EventForwarder(const EventForwarder&) = delete;
  EventForwarder& operator=(const EventForwarder&) = delete;

  ~EventForwarder() override = default;

  // Insert itself to pre-target handler lists of |window|
  void Observe(aura::Window* window) { observation_.Observe(window); }
  void Reset() { observation_.Reset(); }

  bool IsSlideCapturedByArc() const {
    return is_current_slide_handled_by_android_;
  }

 private:
  // ui::EventHandler
  void OnEvent(ui::Event* event) override {
    // Do not forward event targeted to the floating close button so that
    // keyboard press and tap are handled properly.
    if (owner_->floating_control_buttons_widget_ && event->target() &&
        owner_->floating_control_buttons_widget_->GetNativeWindow() ==
            event->target()) {
      return;
    }

    if (!owner_->item_ || !owner_->surface_)
      return;

    views::Widget* widget = owner_->GetWidget();
    if (!widget || !widget->GetNativeWindow()) {
      return;
    }

    // Forward the events to the containing widget, except for:
    // 1. Touches, because View should no longer receive touch events.
    //    See View::OnTouchEvent.
    // 2. Tap gestures are handled on the Android side, so ignore them.
    //    See https://crbug.com/709911.
    // 3. Key events. These are already forwarded by NotificationSurface's
    //    WindowDelegate.
    if (event->IsLocatedEvent()) {
      ui::LocatedEvent* located_event = event->AsLocatedEvent();
      located_event->target()->ConvertEventToTarget(widget->GetNativeWindow(),
                                                    located_event);
      if (located_event->type() == ui::EventType::kMouseEntered ||
          located_event->type() == ui::EventType::kMouseExited) {
        owner_->UpdateControlButtonsVisibility();
        widget->OnMouseEvent(located_event->AsMouseEvent());
        return;
      }

      if (located_event->type() == ui::EventType::kMouseMoved ||
          located_event->IsMouseWheelEvent()) {
        widget->OnMouseEvent(located_event->AsMouseEvent());
      } else if (located_event->IsScrollEvent()) {
        owner_->item_->CancelPress();
        widget->OnScrollEvent(located_event->AsScrollEvent());
        return;
      } else if (located_event->IsGestureEvent() &&
                 event->type() != ui::EventType::kGestureTap) {
        bool slide_handled_by_android = false;
        if ((event->type() == ui::EventType::kGestureScrollBegin ||
             event->type() == ui::EventType::kGestureScrollUpdate ||
             event->type() == ui::EventType::kGestureScrollEnd ||
             event->type() == ui::EventType::kGestureSwipe)) {
          gfx::RectF rect =
              owner_->surface_->GetContentWindow()->transform().MapRect(
                  gfx::RectF(owner_->item_->GetSwipeInputRect()));
          gfx::Point location = located_event->location();
          views::View::ConvertPointFromWidget(owner_, &location);
          bool contains = rect.Contains(gfx::PointF(location));

          if (contains && event->type() == ui::EventType::kGestureScrollBegin) {
            swipe_captured_ = true;
          }

          slide_handled_by_android = contains && swipe_captured_;
        }

        if (event->type() == ui::EventType::kGestureScrollBegin) {
          owner_->item_->CancelPress();
        }

        if (event->type() == ui::EventType::kGestureScrollEnd) {
          swipe_captured_ = false;
        }

        if (slide_handled_by_android &&
            event->type() == ui::EventType::kGestureScrollBegin) {
          is_current_slide_handled_by_android_ = true;
          owner_->message_view_->DisableSlideForcibly(true);
        } else if (is_current_slide_handled_by_android_ &&
                   event->type() == ui::EventType::kGestureScrollEnd) {
          is_current_slide_handled_by_android_ = false;
          owner_->message_view_->DisableSlideForcibly(false);
        }

        widget->OnGestureEvent(located_event->AsGestureEvent());
      }

      // Records UMA when user clicks/taps on the notification surface. Note
      // that here we cannot determine which actions are performed since
      // mouse/gesture events are directly forwarded to Android side.
      // Interactions with the notification itself e.g. toggling notification
      // settings are being captured as well, while clicks/taps on the close
      // button won't reach this. Interactions from keyboard are handled
      // separately in ArcNotificationItemImpl.
      if (event->type() == ui::EventType::kMouseReleased ||
          event->type() == ui::EventType::kGestureTap) {
        // TODO(b/185943161): Record this in arc::ArcMetricsService.
        UMA_HISTOGRAM_ENUMERATION(
            "Arc.UserInteraction",
            arc::UserInteractionType::NOTIFICATION_INTERACTION);
      }

      // When the ARC notification is slid out, all mouse presses and taps
      // should go to underlying widget so the swipe control buttons can
      // pressed. See crbug.com/965603.
      if (owner_->slide_in_progress()) {
        if (event->type() == ui::EventType::kMouseReleased ||
            event->type() == ui::EventType::kMousePressed) {
          widget->OnMouseEvent(event->AsMouseEvent());
        } else if (event->type() == ui::EventType::kGestureTap) {
          widget->OnGestureEvent(event->AsGestureEvent());
        }
      }
    }

    // If AXTree is attached to notification content view, notification surface
    // always gets focus. Tab key events are consumed by the surface, and tab
    // focus traversal gets stuck at Android notification. To prevent it, always
    // pass tab key event to focus manager of content view.
    // TODO(yawano): include elements inside Android notification in tab focus
    // traversal rather than skipping them.
    if (owner_->surface_->GetAXTreeId() != ui::AXTreeIDUnknown() &&
        event->IsKeyEvent()) {
      ui::KeyEvent* key_event = event->AsKeyEvent();
      if (key_event->key_code() == ui::VKEY_TAB &&
          (key_event->flags() == ui::EF_NONE ||
           key_event->flags() == ui::EF_SHIFT_DOWN)) {
        widget->GetFocusManager()->OnKeyEvent(*key_event);
      }
    }
  }

  // Some swipes are handled by Android alone. We don't want to capture swipe
  // events if we started a swipe on the chrome side then moved into the Android
  // swipe region. So, keep track of whether swipe has been 'captured' by
  // Android.
  bool swipe_captured_ = false;

  const raw_ptr<ArcNotificationContentView> owner_;
  bool is_current_slide_handled_by_android_ = false;

  base::ScopedObservation<ui::EventTarget, ui::EventHandler> observation_{this};
};

class ArcNotificationContentView::SlideHelper {
 public:
  explicit SlideHelper(ArcNotificationContentView* owner) : owner_(owner) {
    // Reset opacity to 1 to handle to case when the surface is sliding before
    // getting managed by this class, e.g. sliding in a popup before showing
    // in a message center view.
    if (owner_->surface_) {
      DCHECK(owner_->surface_->GetWindow());
      owner_->surface_->GetWindow()->layer()->SetOpacity(1.0f);
    }
  }

  SlideHelper(const SlideHelper&) = delete;
  SlideHelper& operator=(const SlideHelper&) = delete;

  virtual ~SlideHelper() = default;

  void Update(bool slide_in_progress) {
    if (slide_in_progress_ == slide_in_progress)
      return;

    slide_in_progress_ = slide_in_progress;

    if (slide_in_progress_)
      owner_->ShowCopiedSurface();
    else
      owner_->HideCopiedSurface();
  }

 private:
  const raw_ptr<ArcNotificationContentView> owner_;

  // True if the view is not at the original position.
  bool slide_in_progress_ = false;
};

// static
int ArcNotificationContentView::GetNotificationContentViewWidth() {
  return GetNotificationInMessageCenterWidth();
}

ArcNotificationContentView::ArcNotificationContentView(
    ArcNotificationItem* item,
    const message_center::Notification& notification,
    message_center::MessageView* message_view)
    : item_(item),
      notification_key_(item->GetNotificationKey()),
      event_forwarder_(std::make_unique<EventForwarder>(this)),
      mouse_enter_exit_handler_(std::make_unique<MouseEnterExitHandler>(this)),
      message_view_(message_view),
      control_buttons_view_(message_view) {
  DCHECK(message_view);
  control_buttons_view_.SetNotificationControlButtonFactory(
      std::make_unique<AshNotificationControlButtonFactory>());

  // `GetNotificationInMessageCenterWidth()` must be the the same as what is
  // defined in `ArcNotificationWrapperView` class in Android side.
  assert(
      GetNotificationInMessageCenterWidth() ==
      (chromeos::features::IsNotificationWidthIncreaseEnabled() ? 384 : 344));

  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetNotifyEnterExitOnChild(true);

  item_->IncrementWindowRefCount();
  item_->AddObserver(this);

  auto* surface_manager = ArcNotificationSurfaceManager::Get();
  if (surface_manager) {
    surface_manager->AddObserver(this);
    ArcNotificationSurface* surface =
        surface_manager->GetArcSurface(notification_key_);
    if (surface)
      OnNotificationSurfaceAdded(surface);
  }

  UpdateAccessibleRole();
  // Creates the control_buttons_view_, which collects all control buttons into
  // a horizontal box.
  control_buttons_view_.set_owned_by_client();
  Update(notification);

  // Create a layer as an anchor to insert surface copy during a slide.
  SetPaintToLayer();
  // SetFillsBoundsOpaquely causes overdraw and has performance implications.
  // See the comment in this method and --show-overdraw-feedback for detail.
  layer()->SetFillsBoundsOpaquely(false);
  UpdatePreferredSize();
}

ArcNotificationContentView::~ArcNotificationContentView() {
  SetSurface(nullptr);

  auto* surface_manager = ArcNotificationSurfaceManager::Get();
  if (surface_manager)
    surface_manager->RemoveObserver(this);
  if (item_) {
    item_->RemoveObserver(this);
    item_->DecrementWindowRefCount();
  }
  CHECK(!views::WidgetObserver::IsInObserverList());
}

void ArcNotificationContentView::Update(
    const message_center::Notification& notification) {
  control_buttons_view_.ShowSettingsButton(
      notification.should_show_settings_button());
  control_buttons_view_.ShowCloseButton(!notification.pinned());
  control_buttons_view_.ShowSnoozeButton(
      notification.should_show_snooze_button());
  UpdateControlButtonsVisibility();

  GetViewAccessibility().SetName(
      message_view_->CreateAccessibleName(notification));
  UpdateSnapshot();
}

message_center::NotificationControlButtonsView*
ArcNotificationContentView::GetControlButtonsView() {
  // |control_buttons_view_| is hosted in |floating_control_buttons_widget_| and
  // should not be used when there is no |floating_control_buttons_widget_|.
  return floating_control_buttons_widget_ ? &control_buttons_view_ : nullptr;
}

void ArcNotificationContentView::VisibilityChanged(View* starting_from,
                                                   bool is_visible) {
  // Need to explicitly set visibility for control_buttons_view_ to
  // make sure they don't capture focus when the notification is not
  // visible due to the message center being collapsed.
  control_buttons_view_.SetVisible(is_visible);
  UpdateControlButtonsVisibility();
}

void ArcNotificationContentView::UpdateControlButtonsVisibility() {
  if (!control_buttons_view_.parent())
    return;

  // If the visibility change is ongoing, skip this method to prevent an
  // infinite loop.
  if (updating_control_buttons_visibility_)
    return;

  DCHECK(floating_control_buttons_widget_);

  const bool target_visibility =
      GetVisible() && (control_buttons_view_.IsAnyButtonFocused() ||
                       (message_view_->GetMode() !=
                            message_center::MessageView::Mode::SETTING &&
                        IsMouseHovered()));

  if (target_visibility == floating_control_buttons_widget_->IsVisible())
    return;

  // Add the guard to prevent an infinite loop. Changing visibility may generate
  // an event and it may call this method again.
  base::AutoReset<bool> reset(&updating_control_buttons_visibility_, true);

  if (target_visibility)
    floating_control_buttons_widget_->Show();
  else
    floating_control_buttons_widget_->Hide();
}

void ArcNotificationContentView::UpdateCornerRadius(float top_radius,
                                                    float bottom_radius) {
  contents_radii_ = gfx::RoundedCornersF(top_radius, top_radius, bottom_radius,
                                         bottom_radius);
  if (GetWidget()) {
    SetCornerRadii(contents_radii_);
  }
}

void ArcNotificationContentView::OnSlideChanged(bool in_progress) {
  if (event_forwarder_->IsSlideCapturedByArc()) {
    // The callback is called by SlideOutController, but no animation actually
    // happens because it's forcibly disabled by EventForwarder.
    return;
  }
  slide_in_progress_ = in_progress;
  if (slide_helper_)
    slide_helper_->Update(in_progress);
}

void ArcNotificationContentView::OnContainerAnimationStarted() {
  ShowCopiedSurface();
}

void ArcNotificationContentView::OnContainerAnimationEnded() {
  HideCopiedSurface();
}

void ArcNotificationContentView::MaybeCreateFloatingControlButtons() {
  // Floating close button is a transient child of |surface_| and also part
  // of the hosting widget's focus chain. It could only be created when both
  // are present. Further, if we are being destroyed (|item_| is null), don't
  // create the control buttons.
  if (!surface_ || !GetWidget() || !item_ || control_buttons_view_.parent()) {
    return;
  }

  DCHECK(!floating_control_buttons_widget_);

  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_CONTROL);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = surface_->GetWindow();

  floating_control_buttons_widget_ = std::make_unique<views::Widget>();
  floating_control_buttons_widget_->Init(std::move(params));
  floating_control_buttons_widget_->SetContentsView(&control_buttons_view_);
  floating_control_buttons_widget_->GetNativeWindow()->AddPreTargetHandler(
      mouse_enter_exit_handler_.get());

  // Put the close button into the focus chain.
  floating_control_buttons_widget_->SetFocusTraversableParent(
      GetWidget()->GetFocusTraversable());
  floating_control_buttons_widget_->SetFocusTraversableParentView(this);

  DeprecatedLayoutImmediately();
}

void ArcNotificationContentView::SetSurface(ArcNotificationSurface* surface) {
  if (surface_ == surface)
    return;

  if (floating_control_buttons_widget_) {
    floating_control_buttons_widget_->GetNativeWindow()->RemovePreTargetHandler(
        mouse_enter_exit_handler_.get());
  }

  // Reset |floating_control_buttons_widget_| when |surface_| is changed.
  floating_control_buttons_widget_.reset();

  if (surface_) {
    DCHECK(surface_->GetWindow());
    DCHECK(surface_->GetContentWindow());
    surface_->GetContentWindow()->RemoveObserver(this);
    event_forwarder_->Reset();

    if (surface_->GetAttachedHost() == this) {
      DCHECK_EQ(this, surface_->GetAttachedHost());
      surface_->Detach();
    }
  }

  surface_ = surface;
  UpdateAccessibleRole();

  if (surface_) {
    DCHECK(surface_->GetWindow());
    DCHECK(surface_->GetContentWindow());
    surface_->GetContentWindow()->AddObserver(this);
    event_forwarder_->Observe(surface_->GetWindow());

    if (GetWidget()) {
      // Force to detach the surface.
      if (surface_->IsAttached()) {
        // The attached host must not be this. Since if it is, this should
        // already be detached above.
        DCHECK_NE(this, surface_->GetAttachedHost());
        surface_->Detach();
      }
      AttachSurface();

      if (activate_on_attach_) {
        ActivateWidget(true);
        activate_on_attach_ = false;
      }
    }
  }

  // Setting/resetting |surface_| changes the visibility of the snapshot so we
  // here request to paint.
  SchedulePaint();
}

void ArcNotificationContentView::UpdatePreferredSize() {
  gfx::Size preferred_size;
  if (surface_)
    preferred_size = surface_->GetSize();
  else if (item_)
    preferred_size = item_->GetSnapshot().size();

  if (preferred_size.IsEmpty())
    return;

  const int notification_width = GetNotificationInMessageCenterWidth();
  if (preferred_size.width() != notification_width) {
    const float scale =
        static_cast<float>(notification_width) / preferred_size.width();
    preferred_size.SetSize((notification_width),
                           preferred_size.height() * scale);
  }

  SetPreferredSize(preferred_size);
}

void ArcNotificationContentView::UpdateSnapshot() {
  // Bail if we have a |surface_| because it controls the sizes and paints UI.
  if (surface_)
    return;

  UpdatePreferredSize();
  SchedulePaint();
}

void ArcNotificationContentView::AttachSurface() {
  DCHECK(!native_view());

  // If the view is hidden, we attach the surface in
  // `ArcNotificationContentView::SetVisible()` when it gets visible.
  if (!GetVisible() || !GetWidget()) {
    return;
  }

  UpdatePreferredSize();
  surface_->Attach(this);

  // Creates slide helper after this view is added to its parent.
  slide_helper_ = std::make_unique<SlideHelper>(this);

  // Invokes Update() in case surface is attached during a slide.
  slide_helper_->Update(slide_in_progress_);

  // (Re-)create the floating buttons after |surface_| is attached to a widget.
  MaybeCreateFloatingControlButtons();
}

void ArcNotificationContentView::SetVisible(bool visible) {
  NativeViewHost::SetVisible(visible);
  if (visible) {
    EnsureSurfaceAttached();
  } else {
    EnsureSurfaceDetached();
  }
}

void ArcNotificationContentView::EnsureSurfaceAttached() {
  if (!surface_ || surface_->IsAttached()) {
    return;
  }
  AttachSurface();
}

void ArcNotificationContentView::EnsureSurfaceDetached() {
  if (!GetWidget()) {
    return;
  }

  if (surface_ && surface_->IsAttached()) {
    surface_->Detach();
  }
}

void ArcNotificationContentView::ShowCopiedSurface() {
  if (!surface_)
    return;
  DCHECK(surface_->GetWindow());
  surface_copy_ = ::wm::RecreateLayers(surface_->GetWindow());
  // |surface_copy_| is at (0, 0) in owner_->layer().
  gfx::Rect size(surface_copy_->root()->size());
  surface_copy_->root()->SetBounds(size);
  layer()->Add(surface_copy_->root());

  surface_copy_->root()->SetRoundedCornerRadius(contents_radii_);
  surface_copy_->root()->SetIsFastRoundedCorner(true);

  // Changes the opacity instead of setting the visibility, to keep
  // |EventFowarder| working.
  surface_->GetWindow()->layer()->SetOpacity(0.0f);
}

void ArcNotificationContentView::HideCopiedSurface() {
  if (!surface_ || !surface_copy_)
    return;
  DCHECK(surface_->GetWindow());
  surface_->GetWindow()->layer()->SetOpacity(1.0f);
  DeprecatedLayoutImmediately();
  surface_copy_.reset();
}

void ArcNotificationContentView::AddedToWidget() {
  if (attached_widget_)
    attached_widget_->RemoveObserver(this);

  attached_widget_ = GetWidget();
  attached_widget_->AddObserver(this);

  // Hide the copied surface since it may be visible by OnWidgetClosing().
  if (surface_copy_)
    HideCopiedSurface();

  SetCornerRadii(contents_radii_);
}

void ArcNotificationContentView::RemovedFromWidget() {
  if (attached_widget_) {
    attached_widget_->RemoveObserver(this);
    attached_widget_ = nullptr;
  }
}

void ArcNotificationContentView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  views::Widget* widget = GetWidget();

  if (!details.is_add) {
    // Resets slide helper when this view is removed from its parent.
    slide_helper_.reset();

    // Bail if this view is no longer attached to a widget or native_view() has
    // attached to a different widget.
    if (!widget ||
        (native_view() && views::Widget::GetTopLevelWidgetForNativeView(
                              native_view()) != widget)) {
      return;
    }
  }

  views::NativeViewHost::ViewHierarchyChanged(details);

  if (!widget || !surface_ || !details.is_add)
    return;

  if (surface_->IsAttached())
    surface_->Detach();
  AttachSurface();
}

void ArcNotificationContentView::Layout(PassKey) {
  base::AutoReset<bool> auto_reset_in_layout(&in_layout_, true);

  if (!surface_ || !GetWidget())
    return;

  bool is_surface_visible = (surface_->GetWindow()->layer()->opacity() != 0.0f);
  if (is_surface_visible) {
    // views::NativeViewHost::Layout() can be triggered only when the hosted
    // window is opaque, because that method calls
    // views::NativeViewHostAura::ShowWidget() and aura::Window::Show() which
    // DCHECKs the opacity of the window.
    LayoutSuperclass<views::NativeViewHost>(this);

    // Scale notification surface if necessary.
    gfx::Transform transform;
    const gfx::Size surface_size = surface_->GetSize();
    if (!surface_size.IsEmpty()) {
      const float factor =
          static_cast<float>(GetNotificationInMessageCenterWidth()) /
          surface_size.width();
      transform.Scale(factor, factor);
    }

    // Apply the transform to the surface content so that close button can
    // be positioned without the need to consider the transform.
    surface_->GetContentWindow()->SetTransform(transform);
  }

  if (floating_control_buttons_widget_) {
    const gfx::Rect contents_bounds = GetContentsBounds();

    gfx::Rect control_buttons_bounds(contents_bounds);
    const gfx::Size button_size = control_buttons_view_.GetPreferredSize();

    const int control_buttons_x = GetMirroredXWithWidthInView(
        control_buttons_bounds.right() - button_size.width() -
            message_center::kControlButtonPadding,
        button_size.width());
    control_buttons_bounds.set_x(control_buttons_x);
    control_buttons_bounds.set_y(control_buttons_bounds.y() +
                                 message_center::kControlButtonPadding);
    control_buttons_bounds.set_width(button_size.width());
    control_buttons_bounds.set_height(button_size.height());
    floating_control_buttons_widget_->SetBounds(control_buttons_bounds);
  }

  UpdateControlButtonsVisibility();
}

void ArcNotificationContentView::OnPaint(gfx::Canvas* canvas) {
  views::NativeViewHost::OnPaint(canvas);

  SkScalar radii[8] = {contents_radii_.upper_left(),
                       contents_radii_.upper_left(),  // top-left
                       contents_radii_.upper_right(),
                       contents_radii_.upper_right(),  // top-right
                       contents_radii_.lower_right(),
                       contents_radii_.lower_right(),  // bottom-right
                       contents_radii_.lower_left(),
                       contents_radii_.lower_left()};  // bottom-left
  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(GetLocalBounds()), radii,
                    SkPathDirection::kCCW);
  canvas->ClipPath(path, false);

  if (!surface_ && item_ && !item_->GetSnapshot().isNull()) {
    // Draw the snapshot if there is no surface and the snapshot is available.
    const gfx::Rect contents_bounds = GetContentsBounds();
    canvas->DrawImageInt(
        item_->GetSnapshot(), 0, 0, item_->GetSnapshot().width(),
        item_->GetSnapshot().height(), contents_bounds.x(), contents_bounds.y(),
        contents_bounds.width(), contents_bounds.height(), true /* filter */);
  } else {
    // Draw a clear background otherwise. The height of the view/ surface and
    // animation buffer size are not exactly synced and user may see the blank
    // area out of the surface.
    // TODO: This can be removed once both ARC and Chrome notifications have
    // smooth expansion animations.
    canvas->DrawColor(SK_ColorTRANSPARENT);
  }
}

void ArcNotificationContentView::OnMouseEntered(const ui::MouseEvent&) {
  UpdateControlButtonsVisibility();
}

void ArcNotificationContentView::OnMouseExited(const ui::MouseEvent&) {
  UpdateControlButtonsVisibility();
}

void ArcNotificationContentView::OnFocus() {
  auto* notification_view = ArcNotificationView::FromView(parent());
  CHECK(notification_view);

  NativeViewHost::OnFocus();
  notification_view->OnContentFocused();

  if (surface_ && surface_->GetAXTreeId() != ui::AXTreeIDUnknown())
    ActivateWidget(true);
}

void ArcNotificationContentView::OnBlur() {
  if (!parent()) {
    // OnBlur may be called when this view is being removed.
    return;
  }

  auto* notification_view = ArcNotificationView::FromView(parent());
  CHECK(notification_view);

  NativeViewHost::OnBlur();
  notification_view->OnContentBlurred();
}

void ArcNotificationContentView::OnThemeChanged() {
  View::OnThemeChanged();

  // Adjust control button color.
  control_buttons_view_.SetButtonIconColors(
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary));
}

void ArcNotificationContentView::OnRemoteInputActivationChanged(
    bool activated) {
  // Remove the focus from the currently focused view-control in the message
  // center before activating the window of ARC notification, so that unexpected
  // key handling doesn't happen (b/74415372).
  // Focusing notification surface window doesn't steal the focus from the
  // focused view control in the message center, so that input events handles
  // on both side wrongly without this.
  GetFocusManager()->ClearFocus();

  ActivateWidget(activated);
}

void ArcNotificationContentView::ActivateWidget(bool activate) {
  if (!GetWidget())
    return;

  // Make the widget active.
  if (activate) {
    GetWidget()->widget_delegate()->SetCanActivate(true);
    GetWidget()->Activate();

    if (surface_)
      surface_->FocusSurfaceWindow();
    else
      activate_on_attach_ = true;
  } else {
    GetWidget()->widget_delegate()->SetCanActivate(false);
  }
}

views::FocusTraversable* ArcNotificationContentView::GetFocusTraversable() {
  if (floating_control_buttons_widget_)
    return static_cast<views::internal::RootView*>(
        floating_control_buttons_widget_->GetRootView());
  return nullptr;
}

void ArcNotificationContentView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  if (surface_ && surface_->GetAXTreeId() != ui::AXTreeIDUnknown()) {
    GetViewAccessibility().SetChildTreeID(surface_->GetAXTreeId());
  } else {
    node_data->AddStringAttribute(
        ax::mojom::StringAttribute::kRoleDescription,
        l10n_util::GetStringUTF8(
            IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
  }
}

void ArcNotificationContentView::OnAccessibilityEvent(ax::mojom::Event event) {
  if (event == ax::mojom::Event::kTextSelectionChanged) {
    // Activate and request focus on notification content view. If text
    // selection changed event is dispatched, it indicates that user is going to
    // type something inside Android notification. Widget of message center is
    // not activated by default. We need to activate the widget. If other view
    // in message center has focus, it can consume key event. We need to request
    // focus to move it to this content view.
    ActivateWidget(true);
    RequestFocus();
  }
}

void ArcNotificationContentView::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (in_layout_)
    return;

  UpdatePreferredSize();
  DeprecatedLayoutImmediately();
}

void ArcNotificationContentView::OnWindowDestroying(aura::Window* window) {
  SetSurface(nullptr);
}

void ArcNotificationContentView::OnWidgetDestroying(views::Widget* widget) {
  // Actually this code doesn't show copied surface. Since it looks it doesn't
  // work during closing. This just hides the surface and revails hidden
  // snapshot: https://crbug.com/890701.
  ShowCopiedSurface();

  if (attached_widget_) {
    attached_widget_->RemoveObserver(this);
    attached_widget_ = nullptr;
  }
}

void ArcNotificationContentView::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  if (item_)
    item_->OnWindowActivated(active);
}

void ArcNotificationContentView::OnItemDestroying() {
  item_->RemoveObserver(this);
  item_ = nullptr;

  // Reset |surface_| with |item_| since no one is observing the |surface_|
  // after |item_| is gone and this view should be removed soon.
  SetSurface(nullptr);
}

void ArcNotificationContentView::OnItemContentChanged(
    arc::mojom::ArcNotificationShownContents content) {
  shown_content_ = content;

  bool is_normal_content_shown =
      (shown_content_ ==
       arc::mojom::ArcNotificationShownContents::CONTENTS_SHOWN);
  message_view_->SetSettingMode(!is_normal_content_shown);
}

void ArcNotificationContentView::OnNotificationSurfaceAdded(
    ArcNotificationSurface* surface) {
  if (!item_ || surface->GetNotificationKey() != notification_key_)
    return;

  SetSurface(surface);

  // Notify ax::mojom::Event::kChildrenChanged to force AXNodeData of this view
  // updated. As order of OnNotificationSurfaceAdded call is not guaranteed, we
  // are dispatching the event in both ArcNotificationContentView and
  // ArcAccessibilityHelperBridge.
  NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged, false);
}

void ArcNotificationContentView::OnNotificationSurfaceRemoved(
    ArcNotificationSurface* surface) {
  if (surface->GetNotificationKey() != notification_key_)
    return;

  SetSurface(nullptr);
}

void ArcNotificationContentView::OnNotificationSurfaceAXTreeIdChanged(
    ArcNotificationSurface* surface) {
  if (surface->GetNotificationKey() != notification_key_) {
    return;
  }

  UpdateAccessibleRole();
}

void ArcNotificationContentView::UpdateAccessibleRole() {
  if (surface_ && surface_->GetAXTreeId() != ui::AXTreeIDUnknown()) {
    GetViewAccessibility().SetRole(ax::mojom::Role::kClient);
  } else {
    GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  }
}

BEGIN_METADATA(ArcNotificationContentView)
END_METADATA

}  // namespace ash

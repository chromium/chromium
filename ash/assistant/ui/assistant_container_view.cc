// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_container_view.h"

#include <algorithm>
#include <set>
#include <vector>

#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_container_view_animator.h"
#include "ash/assistant/ui/assistant_main_view.h"
#include "ash/assistant/ui/assistant_mini_view.h"
#include "ash/assistant/ui/assistant_overlay.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_web_view.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Appearance.
constexpr SkColor kBackgroundColor = SK_ColorWHITE;

// AssistantContainerClientView ------------------------------------------------

// AssistantContainerClientView is the client view for AssistantContainerView
// which provides support for adding overlays to the Assistant view hierarchy.
// Because overlays are added to the AssistantContainerView client view, they
// paint to a higher level in the layer tree than do direct children of
// AssistantContainerView. This allows AssistantMainView, for example, to
// pseudo-parent overlays that draw over top of Assistant cards.
class AssistantContainerClientView : public views::ClientView,
                                     public views::ViewObserver {
 public:
  AssistantContainerClientView(views::Widget* widget,
                               views::View* contents_view)
      : views::ClientView(widget, contents_view) {}

  ~AssistantContainerClientView() override = default;

  // views::ClientView:
  const char* GetClassName() const override {
    return "AssistantContainerClientView";
  }

  void Layout() override {
    views::ClientView::Layout();
    for (AssistantOverlay* overlay : overlays_)
      Layout(overlay);
  }

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* view) override {
    view->RemoveObserver(this);

    // We need to keep |overlays_| in sync with the view hierarchy.
    auto it = overlays_.find(static_cast<AssistantOverlay*>(view));
    DCHECK(it != overlays_.end());
    overlays_.erase(it);
  }

  void OnViewPreferredSizeChanged(views::View* view) override {
    Layout(static_cast<AssistantOverlay*>(view));
    SchedulePaint();
  }

  void AddOverlays(std::vector<AssistantOverlay*> overlays) {
    for (AssistantOverlay* overlay : overlays) {
      overlays_.insert(overlay);
      overlay->AddObserver(this);
      AddChildView(overlay);
    }
  }

 private:
  void Layout(AssistantOverlay* overlay) {
    AssistantOverlay::LayoutParams layout_params = overlay->GetLayoutParams();
    gfx::Size preferred_size = overlay->GetPreferredSize();

    int left = layout_params.margins.left();
    int top = layout_params.margins.top();
    int width = std::min(preferred_size.width(), this->width());
    int height = preferred_size.height();

    // Gravity::kBottom.
    using Gravity = AssistantOverlay::LayoutParams::Gravity;
    if ((layout_params.gravity & Gravity::kBottom) != 0)
      top = this->height() - height - layout_params.margins.bottom();

    // Gravity::kCenterHorizontal.
    if ((layout_params.gravity & Gravity::kCenterHorizontal) != 0) {
      width = std::min(width, this->width() - layout_params.margins.width());
      left = (this->width() - width) / 2;
    }

    overlay->SetBounds(left, top, width, height);
  }

  std::set<AssistantOverlay*> overlays_;

  DISALLOW_COPY_AND_ASSIGN(AssistantContainerClientView);
};

// AssistantContainerEventTargeter ---------------------------------------------

class AssistantContainerEventTargeter : public aura::WindowTargeter {
 public:
  AssistantContainerEventTargeter() = default;
  ~AssistantContainerEventTargeter() override = default;

  // aura::WindowTargeter:
  bool SubtreeShouldBeExploredForEvent(aura::Window* window,
                                       const ui::LocatedEvent& event) override {
    if (window->GetProperty(assistant::ui::kOnlyAllowMouseClickEvents)) {
      if (event.type() != ui::ET_MOUSE_PRESSED &&
          event.type() != ui::ET_MOUSE_RELEASED) {
        return false;
      }
    }
    return aura::WindowTargeter::SubtreeShouldBeExploredForEvent(window, event);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantContainerEventTargeter);
};

// AssistantContainerLayout ----------------------------------------------------

// The AssistantContainerLayout calculates preferred size to fit the largest
// visible child. Children that are not visible are not factored in. During
// layout, children are horizontally centered and bottom aligned.
class AssistantContainerLayout : public views::LayoutManager {
 public:
  explicit AssistantContainerLayout(AssistantViewDelegate* delegate)
      : delegate_(delegate) {}
  ~AssistantContainerLayout() override = default;

  // views::LayoutManager:
  gfx::Size GetPreferredSize(const views::View* host) const override {
    // Our preferred width is the width of our largest visible child.
    int preferred_width = 0;
    for (const views::View* child : host->children()) {
      if (child->GetVisible()) {
        preferred_width =
            std::max(child->GetPreferredSize().width(), preferred_width);
      }
    }
    return gfx::Size(preferred_width,
                     GetPreferredHeightForWidth(host, preferred_width));
  }

  int GetPreferredHeightForWidth(const views::View* host,
                                 int width) const override {
    // Our preferred height is the height of our largest visible child.
    int preferred_height = 0;
    for (const views::View* child : host->children()) {
      if (child->GetVisible()) {
        preferred_height =
            std::max(child->GetHeightForWidth(width), preferred_height);
      }
    }

    // The height of container view should not exceed work area height to
    // ensure that the widget will not go offscreen even when the screen
    // becomes very short (physical size/resolution change, virtual keyboard
    // shows, etc). When the available work area height is less than
    // |preferred_height|, it anchors its children (e.g. AssistantMainView)
    // to the bottom and the top of the contents will be clipped.
    return std::min(preferred_height,
                    delegate_->GetUiModel()->usable_work_area().height());
  }

  void Layout(views::View* host) override {
    const int host_center_x = host->GetBoundsInScreen().CenterPoint().x();
    const int host_height = host->height();

    for (auto* child : host->children()) {
      const gfx::Size child_size = child->GetPreferredSize();

      // Children are horizontally centered. This means that both the |host|
      // and child views share the same center x-coordinate relative to the
      // screen. We use this center value when placing our children because
      // deriving center from the host width causes rounding inconsistencies
      // that are especially noticeable during animation.
      gfx::Point child_center(host_center_x, /*y=*/0);
      views::View::ConvertPointFromScreen(host, &child_center);
      int child_left = child_center.x() - child_size.width() / 2;

      // Children are bottom aligned.
      int child_top = host_height - child_size.height();

      child->SetBounds(child_left, child_top, child_size.width(),
                       child_size.height());
    }
  }

 private:
  AssistantViewDelegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(AssistantContainerLayout);
};

}  // namespace

// AssistantContainerView ------------------------------------------------------

AssistantContainerView::AssistantContainerView(AssistantViewDelegate* delegate)
    : delegate_(delegate),
      animator_(AssistantContainerViewAnimator::Create(delegate_, this)),
      focus_traversable_(this) {
  UpdateAnchor();

  set_accept_events(true);
  set_close_on_deactivate(false);
  set_color(kBackgroundColor);
  set_margins(gfx::Insets());
  set_shadow(views::BubbleBorder::Shadow::NO_ASSETS);
  set_title_margins(gfx::Insets());

  views::BubbleDialogDelegateView::CreateBubble(this);

  // Corner radius can only be set after bubble creation.
  GetBubbleFrameView()->SetCornerRadius(delegate_->GetUiModel()->ui_mode() ==
                                                AssistantUiMode::kMiniUi
                                            ? kMiniUiCornerRadiusDip
                                            : kCornerRadiusDip);

  // Initialize non-client view layer.
  GetBubbleFrameView()->SetPaintToLayer();
  GetBubbleFrameView()->layer()->SetFillsBoundsOpaquely(false);

  // The AssistantViewDelegate should outlive AssistantContainerView.
  delegate_->AddUiModelObserver(this);

  // Initialize |animator_| only after AssistantContainerView has been
  // fully constructed to give it a chance to perform additional initialization.
  animator_->Init();
}

AssistantContainerView::~AssistantContainerView() {
  delegate_->RemoveUiModelObserver(this);
}

const char* AssistantContainerView::GetClassName() const {
  return "AssistantContainerView";
}

void AssistantContainerView::AddedToWidget() {
  // Exclude the Assistant window for occlusion, so it doesn't trigger auto-pip.
  auto* window = GetWidget()->GetNativeWindow();
  occlusion_excluder_.emplace(window);
  window->SetEventTargeter(std::make_unique<AssistantContainerEventTargeter>());
}

ax::mojom::Role AssistantContainerView::GetAccessibleWindowRole() {
  return ax::mojom::Role::kWindow;
}

base::string16 AssistantContainerView::GetAccessibleWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_WINDOW);
}

int AssistantContainerView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

views::FocusTraversable* AssistantContainerView::GetFocusTraversable() {
  auto* focus_manager = GetFocusManager();
  if (focus_manager && focus_manager->GetFocusedView())
    return nullptr;

  if (!FindFirstFocusableView())
    return nullptr;

  return &focus_traversable_;
}

void AssistantContainerView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantContainerView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  // Do nothing. We override this method to prevent a super class implementation
  // from taking effect which would otherwise cause ChromeVox to read the entire
  // Assistant view hierarchy.
}

void AssistantContainerView::SizeToContents() {
  // We override this method to increase its visibility.
  views::BubbleDialogDelegateView::SizeToContents();
}

void AssistantContainerView::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  params->context = delegate_->GetRootWindowForNewWindows();
  params->corner_radius = kCornerRadiusDip;
  params->z_order = ui::ZOrderLevel::kFloatingWindow;
}

views::ClientView* AssistantContainerView::CreateClientView(
    views::Widget* widget) {
  AssistantContainerClientView* client_view =
      new AssistantContainerClientView(widget, GetContentsView());
  client_view->AddOverlays(assistant_main_view_->GetOverlays());
  return client_view;
}

void AssistantContainerView::Init() {
  SetLayoutManager(std::make_unique<AssistantContainerLayout>(delegate_));

  // We paint to our own layer. Some implementations of |animator_| mask to
  // bounds to clip child layers.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Main view.
  assistant_main_view_ =
      AddChildView(std::make_unique<AssistantMainViewDeprecated>(delegate_));

  // Mini view.
  assistant_mini_view_ =
      AddChildView(std::make_unique<AssistantMiniView>(delegate_));

  // Web view.
  assistant_web_view_ = AddChildView(std::make_unique<AssistantWebView>(
      delegate_,
      /*web_container_view_delegate=*/nullptr));

  // Update the view state based on the current UI mode.
  OnUiModeChanged(delegate_->GetUiModel()->ui_mode(),
                  /*due_to_interaction=*/false);
}

void AssistantContainerView::RequestFocus() {
  if (!GetWidget() || !GetWidget()->IsActive())
    return;

  switch (delegate_->GetUiModel()->ui_mode()) {
    case AssistantUiMode::kMiniUi:
      if (assistant_mini_view_)
        assistant_mini_view_->RequestFocus();
      break;
    case AssistantUiMode::kMainUi:
      if (assistant_main_view_)
        assistant_main_view_->RequestFocus();
      break;
    case AssistantUiMode::kWebUi:
      if (assistant_web_view_)
        assistant_web_view_->RequestFocus();
      break;
    case AssistantUiMode::kAmbientUi:
    case AssistantUiMode::kLauncherEmbeddedUi:
      NOTREACHED();
      break;
  }
}

void AssistantContainerView::UpdateAnchor() {
  // Align to the bottom, horizontal center of the current usable work area.
  const gfx::Rect& usable_work_area =
      delegate_->GetUiModel()->usable_work_area();
  const gfx::Rect anchor =
      gfx::Rect(usable_work_area.x(), usable_work_area.bottom(),
                usable_work_area.width(), 0);
  SetAnchorRect(anchor);
  SetArrow(views::BubbleBorder::Arrow::BOTTOM_CENTER);
}

void AssistantContainerView::OnUiModeChanged(AssistantUiMode ui_mode,
                                             bool due_to_interaction) {
  for (auto* child : children())
    child->SetVisible(false);

  switch (ui_mode) {
    case AssistantUiMode::kMiniUi:
      assistant_mini_view_->SetVisible(true);
      break;
    case AssistantUiMode::kMainUi:
      assistant_main_view_->SetVisible(true);
      break;
    case AssistantUiMode::kWebUi:
      assistant_web_view_->SetVisible(true);
      break;
    case AssistantUiMode::kAmbientUi:
    case AssistantUiMode::kLauncherEmbeddedUi:
      NOTREACHED();
      break;
  }

  PreferredSizeChanged();
  RequestFocus();
}

void AssistantContainerView::OnUsableWorkAreaChanged(
    const gfx::Rect& usable_work_area) {
  UpdateAnchor();

  // Call PreferredSizeChanged() to update animation params to avoid undesired
  // effects (e.g. resize animation of Assistant UI when zooming in/out screen).
  PreferredSizeChanged();
}

views::View* AssistantContainerView::FindFirstFocusableView() {
  if (!GetWidget() || !GetWidget()->IsActive())
    return nullptr;

  switch (delegate_->GetUiModel()->ui_mode()) {
    case AssistantUiMode::kMainUi:
      // AssistantMainView will sometimes explicitly specify a view to be
      // focused first. Other times it may defer to views::FocusSearch.
      return assistant_main_view_
                 ? assistant_main_view_->FindFirstFocusableView()
                 : nullptr;
    case AssistantUiMode::kMiniUi:
    case AssistantUiMode::kWebUi:
      // Default views::FocusSearch behavior is acceptable.
      return nullptr;
    case AssistantUiMode::kAmbientUi:
    case AssistantUiMode::kLauncherEmbeddedUi:
      NOTREACHED();
      return nullptr;
  }
}

SkColor AssistantContainerView::GetBackgroundColor() const {
  return kBackgroundColor;
}

int AssistantContainerView::GetCornerRadius() const {
  return GetBubbleFrameView()->corner_radius();
}

void AssistantContainerView::SetCornerRadius(int corner_radius) {
  GetBubbleFrameView()->SetCornerRadius(corner_radius);
}

ui::Layer* AssistantContainerView::GetNonClientViewLayer() {
  return GetBubbleFrameView()->layer();
}

}  // namespace ash

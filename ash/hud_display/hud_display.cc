// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/hud_display.h"

#include "ash/fast_ink/view_tree_host_root_view.h"
#include "ash/fast_ink/view_tree_host_widget.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/hud_display/graphs_container_view.h"
#include "ash/hud_display/hud_constants.h"
#include "ash/hud_display/hud_header_view.h"
#include "ash/hud_display/hud_properties.h"
#include "ash/hud_display/hud_settings_view.h"
#include "ash/hud_display/tab_strip.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace hud_display {
namespace {

// Header height.
constexpr int kHUDHeaderHeight =
    kHUDSettingsIconSize + 2 * kHUDSettingsIconBorder;

// Margin below header.
constexpr int kHUDHeaderMargin = 5;

// Graph height.
constexpr int kHUDGraphHeight = 300;

// Graph width/height including bordering reference lines.
constexpr int kHUDGraphWidthWithReferenceLines =
    kHUDGraphWidth + 2 * kHUDGraphReferenceLineWidth;
constexpr int kHUDGraphHeightWithReferenceLines =
    kHUDGraphHeight + 2 * kHUDGraphReferenceLineWidth;

// HUD window width.
constexpr int kHUDWidth = kHUDGraphWidthWithReferenceLines + 2 * kHUDInset;

// Top inset + header + header margin + bottom inset. Used to compute the HUD
// window height. Just add the graph height or settings height as appropriate.
constexpr int kHUDFrameHeight =
    kHUDInset + kHUDHeaderHeight + kHUDHeaderMargin + kHUDInset;

// HUD window height with graph.
constexpr int kHUDHeightWithGraph =
    kHUDFrameHeight + kHUDGraphHeightWithReferenceLines;

views::Widget* g_hud_widget = nullptr;

// True if HUD should be initialized as overlay.
bool g_hud_overlay_mode = true;

// ClientView that return HTNOWHERE by default. A child view can receive event
// by setting kHitTestComponentKey property to HTCLIENT.
class HTClientView : public views::ClientView {
  METADATA_HEADER(HTClientView, views::ClientView)

 public:
  HTClientView(HUDDisplayView* hud_display,
               views::Widget* widget,
               views::View* contents_view)
      : views::ClientView(widget, contents_view), hud_display_(hud_display) {}
  HTClientView(const HTClientView&) = delete;
  HTClientView& operator=(const HTClientView&) = delete;

  ~HTClientView() override = default;

  // views::ClientView
  int NonClientHitTest(const gfx::Point& point) override {
    return hud_display_->NonClientHitTest(point);
  }

  HUDDisplayView* GetHUDDisplayViewForTesting() { return hud_display_; }

 private:
  raw_ptr<HUDDisplayView> hud_display_;
};

BEGIN_METADATA(HTClientView)
END_METADATA

std::unique_ptr<views::ClientView> MakeClientView(views::Widget* widget) {
  auto view = std::make_unique<HUDDisplayView>();
  auto* weak_view = view.get();
  return std::make_unique<HTClientView>(weak_view, widget, view.release());
}

void InitializeFrameView(views::WidgetDelegate* delegate) {
  auto* frame_view = static_cast<NonClientFrameViewAsh*>(
      delegate->GetWidget()->non_client_view()->frame_view());
  // TODO(oshima): support component type with TYPE_WINDOW_FLAMELESS widget.
  if (frame_view)
    frame_view->SetFrameEnabled(false);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// HUDDisplayView, public:

BEGIN_METADATA(HUDDisplayView)
END_METADATA

// static
void HUDDisplayView::Destroy() {
  delete g_hud_widget;
  g_hud_widget = nullptr;
}

// static
void HUDDisplayView::Toggle() {
  if (g_hud_widget) {
    Destroy();
    return;
  }

  auto delegate = std::make_unique<views::WidgetDelegate>();
  delegate->SetClientViewFactory(base::BindOnce(&MakeClientView));
  delegate->RegisterWidgetInitializedCallback(
      base::BindOnce(&InitializeFrameView, base::Unretained(delegate.get())));
  delegate->SetOwnedByWidget(true);

  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = delegate.release();
  params.name = "HUDDisplay";
  params.parent = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                      kShellWindowId_OverlayContainer);
  params.bounds = gfx::Rect(kHUDWidth, kHUDHeightWithGraph);
  auto* widget = CreateViewTreeHostWidget(std::move(params));
  widget->GetLayer()->SetName("HUDDisplayView");

  ViewTreeHostRootView* root_view =
      static_cast<ViewTreeHostRootView*>(widget->GetRootView());
  root_view->SetIsOverlayCandidate(g_hud_overlay_mode);
  root_view->Init(widget->GetNativeView());
  widget->Show();

  g_hud_widget = widget;
}

// static
bool HUDDisplayView::IsShown() {
  return g_hud_widget;
}

HUDDisplayView::HUDDisplayView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  // Layout:
  // ----------------------
  // |      Header        | // Buttons, tabs, controls
  // ----------------------
  // |                    | // Data views full-size, z-stacked.
  // |      Data          |
  // |                    |
  // ----------------------

  // Create two child views for header and data. Vertically stacked.
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  header_view_ = AddChildView(std::make_unique<HUDHeaderView>(this));
  views::View* data = AddChildView(std::make_unique<views::View>());

  // Data view takes the rest of the host view.
  layout_manager->SetFlexForView(data, 1, /*use_min_size=*/false);

  // Setup header.

  header_view_->tab_strip()->AddTabButton(HUDDisplayMode::CPU, u"CPU");
  header_view_->tab_strip()->AddTabButton(HUDDisplayMode::MEMORY, u"RAM");
  header_view_->tab_strip()->AddTabButton(HUDDisplayMode::FPS, u"FPS");

  // Setup data.
  data->SetBackground(views::CreateSolidBackground(kHUDBackground));
  data->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kHUDHeaderMargin, kHUDInset, kHUDInset, kHUDInset)));

  // We have two child views z-stacked.
  // The bottom one is GraphsContainerView with all the graph lines.
  // The top one is settings UI overlay.
  data->SetLayoutManager(std::make_unique<views::FillLayout>());
  graphs_container_ =
      data->AddChildView(std::make_unique<GraphsContainerView>());
  settings_view_ = data->AddChildView(std::make_unique<HUDSettingsView>(this));
  settings_view_->SetVisible(false);

  // CPU display is active by default.
  SetDisplayMode(HUDDisplayMode::CPU);
}

HUDDisplayView::~HUDDisplayView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
}

// There is only one button.
void HUDDisplayView::OnSettingsToggle() {
  gfx::Rect bounds = g_hud_widget->GetWindowBoundsInScreen();
  // Here we are checking the settings visibility before we toggle it. We must
  // keep in mind that it is the opposite of what it will be.
  bounds.set_height(settings_view_->GetVisible()
                        ? kHUDHeightWithGraph
                        : kHUDFrameHeight +
                              settings_view_->GetPreferredSize().height());
  g_hud_widget->SetBounds(bounds);

  settings_view_->ToggleVisibility();
  graphs_container_->SetVisible(!settings_view_->GetVisible());
}

bool HUDDisplayView::IsOverlay() {
  return static_cast<ViewTreeHostRootView*>(GetWidget()->GetRootView())
      ->GetIsOverlayCandidate();
}

void HUDDisplayView::ToggleOverlay() {
  g_hud_overlay_mode = !g_hud_overlay_mode;
  static_cast<ViewTreeHostRootView*>(GetWidget()->GetRootView())
      ->SetIsOverlayCandidate(g_hud_overlay_mode);
}

// static
HUDDisplayView* HUDDisplayView::GetForTesting() {
  if (!g_hud_widget)
    return nullptr;

  HTClientView* client_view =
      static_cast<HTClientView*>(g_hud_widget->client_view());

  if (!client_view)
    return nullptr;

  return client_view->GetHUDDisplayViewForTesting();  // IN-TEST
}

HUDSettingsView* HUDDisplayView::GetSettingsViewForTesting() {
  return settings_view_;
}

void HUDDisplayView::ToggleSettingsForTesting() {
  OnSettingsToggle();
}

int HUDDisplayView::NonClientHitTest(const gfx::Point& point) {
  const View* view = GetEventHandlerForPoint(point);
  if (!view)
    return HTNOWHERE;

  return view->GetProperty(kHUDClickHandler);
}

void HUDDisplayView::SetDisplayMode(HUDDisplayMode display_mode) {
  graphs_container_->SetMode(display_mode);
  header_view_->tab_strip()->ActivateTab(display_mode);
}

}  // namespace hud_display
}  // namespace ash

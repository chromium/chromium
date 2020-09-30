// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/hud_display.h"

#include "ash/fast_ink/view_tree_host_root_view.h"
#include "ash/fast_ink/view_tree_host_widget.h"
#include "ash/hud_display/graphs_container_view.h"
#include "ash/hud_display/hud_constants.h"
#include "ash/hud_display/hud_header_view.h"
#include "ash/hud_display/hud_properties.h"
#include "ash/hud_display/hud_settings_view.h"
#include "ash/hud_display/tab_strip.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace hud_display {
namespace {

constexpr size_t kHUDGraphsInset = 5;

// Default HUDDisplayView height.
constexpr size_t kDefaultHUDGraphHeight = 300;

// Top border + Header height + margin + graph height + bottom border..
constexpr int kHUDViewDefaultHeight =
    kHUDInset + (kHUDSettingsIconSize + 2 * kSettingsIconBorder) +
    kHUDGraphsInset + kDefaultHUDGraphHeight + kHUDInset;

views::Widget* g_hud_widget = nullptr;

// ClientView that return HTNOWHERE by default. A child view can receive event
// by setting kHitTestComponentKey property to HTCLIENT.
class HTClientView : public views::ClientView {
 public:
  METADATA_HEADER(HTClientView);

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

 private:
  HUDDisplayView* hud_display_;
};

BEGIN_METADATA(HTClientView, views::ClientView)
END_METADATA

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// HUDDisplayView, public:

BEGIN_METADATA(HUDDisplayView, views::WidgetDelegateView)
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

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = new HUDDisplayView();
  params.parent = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                      kShellWindowId_OverlayContainer);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  // Adjust for 1px grid width around the graph.
  params.bounds =
      gfx::Rect(kDefaultGraphWidth + 2 * kHUDInset + 2 * kGridLineWidth,
                kHUDViewDefaultHeight + 2 * kGridLineWidth);
  auto* widget = CreateViewTreeHostWidget(std::move(params));
  widget->GetLayer()->SetName("HUDDisplayView");
  widget->Show();

  g_hud_widget = widget;
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

  // TODO: Add tab buttons via:
  header_view_->tab_strip()->AddTabButton(this, DisplayMode::CPU_DISPLAY,
                                          base::ASCIIToUTF16("CPU"));
  header_view_->tab_strip()->AddTabButton(this, DisplayMode::MEMORY_DISPLAY,
                                          base::ASCIIToUTF16("RAM"));

  // Setup data.
  data->SetBackground(views::CreateSolidBackground(kHUDBackground));
  data->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(kHUDGraphsInset, kHUDInset, kHUDInset, kHUDInset)));

  // We have two child views z-stacked.
  // The bottom one is GraphsContainerView with all the graph lines.
  // The top one is settings UI overlay.
  data->SetLayoutManager(std::make_unique<views::FillLayout>());
  graphs_container_ =
      data->AddChildView(std::make_unique<GraphsContainerView>());
  settings_view_ = data->AddChildView(std::make_unique<HUDSettingsView>());
  settings_view_->SetVisible(false);

  // CPU display is active by default.
  SetDisplayMode(DisplayMode::CPU_DISPLAY);
}

HUDDisplayView::~HUDDisplayView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
}

int HUDDisplayView::NonClientHitTest(const gfx::Point& point) {
  const View* view = GetEventHandlerForPoint(point);
  if (!view)
    return HTNOWHERE;

  return view->GetProperty(kHUDClickHandler);
}

void HUDDisplayView::SetDisplayMode(DisplayMode display_mode) {
  graphs_container_->SetMode(display_mode);
  header_view_->tab_strip()->ActivateTab(display_mode);
}

views::ClientView* HUDDisplayView::CreateClientView(views::Widget* widget) {
  return new HTClientView(this, widget, TransferOwnershipOfContentsView());
}

void HUDDisplayView::OnWidgetInitialized() {
  auto* frame_view = GetWidget()->non_client_view()->frame_view();
  // TODO(oshima): support component type with TYPE_WINDOW_FLAMELESS widget.
  if (frame_view) {
    frame_view->SetEnabled(false);
    frame_view->SetVisible(false);
  }
}

// There is only one button.
void HUDDisplayView::ButtonPressed(views::Button* /*sender*/,
                                   const ui::Event& /*event*/) {
  settings_view_->ToggleVisibility();
  graphs_container_->SetVisible(!settings_view_->GetVisible());
}

}  // namespace hud_display
}  // namespace ash

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_view.h"

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame_bounds_change_animation.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/web_contents.h"
#include "ui/events/event_observer.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace glic {

GlicView::GlicView(Profile* profile, const gfx::Size& initial_size) {
  SetProperty(views::kElementIdentifierKey, kGlicViewElementId);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto web_view = std::make_unique<views::WebView>(profile);
  web_view_ = web_view.get();
  web_view->SetSize(initial_size);
  AddChildView(std::move(web_view));
}

GlicView::~GlicView() = default;

// static
std::unique_ptr<views::Widget> GlicView::CreateWidget(
    Profile* profile,
    const gfx::Rect& initial_bounds) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
#if BUILDFLAG(IS_WIN)
  params.dont_show_in_taskbar = true;
  params.force_system_menu_for_frameless = true;
#endif
  params.bounds = initial_bounds;

  std::unique_ptr<views::Widget> widget =
      std::make_unique<views::Widget>(std::move(params));

  widget->SetContentsView(
      std::make_unique<GlicView>(profile, initial_bounds.size()));

  return widget;
}

GlicView* GlicView::FromWidget(views::Widget& widget) {
  return static_cast<GlicView*>(widget.GetContentsView());
}

void GlicView::SetDraggableAreas(
    const std::vector<gfx::Rect>& draggable_areas) {
  draggable_areas_.assign(draggable_areas.begin(), draggable_areas.end());
}

bool GlicView::IsPointWithinDraggableArea(const gfx::Point& point) {
  for (const gfx::Rect& rect : draggable_areas_) {
    if (rect.Contains(point)) {
      return true;
    }
  }
  return false;
}

}  // namespace glic

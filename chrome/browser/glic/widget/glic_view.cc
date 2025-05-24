// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_view.h"

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame_bounds_change_animation.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event_observer.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace glic {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(GlicView, kWebViewElementIdForTesting);

GlicView::GlicView(Profile* profile,
                   const gfx::Size& initial_size,
                   base::WeakPtr<ui::AcceleratorTarget> accelerator_delegate)
    : accelerator_delegate_(accelerator_delegate) {
  SetProperty(views::kElementIdentifierKey, kGlicViewElementId);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetSize(initial_size);
  // As there is no WebContents yet, this will apply the default background.
  UpdateBackgroundColor();
}

GlicView::~GlicView() = default;

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

void GlicView::UpdatePrimaryDraggableAreaOnResize() {
  if (draggable_areas_.empty()) {
    return;
  }

  draggable_areas_[0].set_width(width());
}

void GlicView::UpdateBackgroundColor() {
  std::optional<SkColor> client_background = GetClientBackgroundColor();
  if (client_background) {
    SetBackground(views::CreateSolidBackground(*client_background));
  } else {
    SetBackground(views::CreateSolidBackground(kColorGlicBackground));
  }
}

bool GlicView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (accelerator_delegate_) {
    return accelerator_delegate_->AcceleratorPressed(accelerator);
  }

  return false;
}

std::optional<SkColor> GlicView::GetClientBackgroundColor() {
  content::WebContents* host = GetWebContents();
  if (!host) {
    return std::nullopt;
  }

  std::vector<content::WebContents*> inner_contents =
      host->GetInnerWebContents();
  if (inner_contents.size() != 1ul) {
    return std::nullopt;
  }

  return inner_contents[0]->GetBackgroundColor();
}

BEGIN_METADATA(GlicView)
END_METADATA

}  // namespace glic

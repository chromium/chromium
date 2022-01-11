// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_name_view.h"

#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The font size increase for the template name view.
constexpr int kNameFontSizeDeltaDp = 4;

#if DCHECK_IS_ON()
bool IsDesksTemplatesGridWidget(const views::Widget* widget) {
  if (!widget)
    return false;

  auto* overview_controller = Shell::Get()->overview_controller();
  if (!overview_controller->InOverviewSession())
    return false;

  auto* session = overview_controller->overview_session();
  for (const auto& grid : session->grid_list()) {
    if (widget == grid->desks_templates_grid_widget())
      return true;
  }

  return false;
}
#endif  // DCHECK_IS_ON()

}  // namespace

DesksTemplatesNameView::DesksTemplatesNameView() {
  SetFontList(GetFontList().Derive(kNameFontSizeDeltaDp, gfx::Font::NORMAL,
                                   gfx::Font::Weight::BOLD));
}

DesksTemplatesNameView::~DesksTemplatesNameView() = default;

// static
void DesksTemplatesNameView::CommitChanges(views::Widget* widget) {
  // TODO(crbug.com/1277302): Refactor this logic to be shared with
  // `DeskNameView::CommitChanges`.
#if DCHECK_IS_ON()
  DCHECK(IsDesksTemplatesGridWidget(widget));
#endif  // DCHECK_IS_ON()

  auto* focus_manager = widget->GetFocusManager();
  focus_manager->ClearFocus();
  // Avoid having the focus restored to the same `DesksTemplatesNameView` when
  // the desks templates grid widget is refocused.
  focus_manager->SetStoredFocusView(nullptr);
}

void DesksTemplatesNameView::OnContentsChanged() {
  PreferredSizeChanged();
}

void DesksTemplatesNameView::SetTextAndElideIfNeeded(
    const std::u16string& text) {
  SetText(gfx::ElideText(text, GetFontList(), GetAvailableWidth(),
                         gfx::ELIDE_TAIL));
  full_text_ = text;
}

gfx::Size DesksTemplatesNameView::CalculatePreferredSize() const {
  const gfx::Size preferred_size = DesksTextfield::CalculatePreferredSize();
  // Use the available width if it is larger than the preferred width.
  const int preferred_width =
      std::clamp(preferred_size.width(), 1, GetAvailableWidth());
  return gfx::Size(preferred_width, kTemplateNameViewHeight);
}

int DesksTemplatesNameView::GetAvailableWidth() const {
  auto* parent_view = static_cast<const views::BoxLayoutView*>(parent());
  int available_width = parent_view->width() -
                        parent_view->GetInsideBorderInsets().width() -
                        GetInsets().width();
  const int between_child_spacing = parent_view->GetBetweenChildSpacing();
  for (auto* child : parent_view->children()) {
    if (child == this || !child->GetVisible())
      continue;
    // The width of `child` may be 0 if it is offscreen, so use the preferred
    // width instead.
    available_width -=
        (child->GetPreferredSize().width() + between_child_spacing);
  }

  return std::max(1, available_width);
}

BEGIN_METADATA(DesksTemplatesNameView, DesksTextfield)
END_METADATA

}  // namespace ash

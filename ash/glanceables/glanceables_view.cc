// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_view.h"

#include <memory>

#include "ash/glanceables/glanceables_restore_view.h"
#include "ash/glanceables/glanceables_up_next_view.h"
#include "ash/glanceables/glanceables_weather_view.h"
#include "ash/glanceables/glanceables_welcome_label.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

// Configures a section label, like "Up next".
void SetupSectionLabel(views::Label* label) {
  label->SetAutoColorReadabilityEnabled(false);
  label->SetFontList(gfx::FontList({"Google Sans"},
                                   gfx::Font::FontStyle::NORMAL, 18,
                                   gfx::Font::Weight::MEDIUM));
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
}

}  // namespace

GlanceablesView::GlanceablesView(bool show_session_restore) {
  // Inside border insets are set in OnBoundsChanged() when this view is added
  // to the widget.
  layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  welcome_label_ = AddChildView(std::make_unique<GlanceablesWelcomeLabel>());

  weather_view_ = AddChildView(std::make_unique<GlanceablesWeatherView>());
  weather_view_->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(8, 0, 0, 0));

  // Container for the left and right columns.
  views::View* container = AddChildView(std::make_unique<views::View>());
  const gfx::Insets container_insets = gfx::Insets::TLBR(48, 4, 0, 4);
  auto* container_layout =
      container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, container_insets));

  // Container for the views on the left.
  auto* left_column = container->AddChildView(std::make_unique<views::View>());
  left_column->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // The "Up next" label.
  up_next_label_ = left_column->AddChildView(std::make_unique<views::Label>());
  SetupSectionLabel(up_next_label_);
  up_next_label_->SetText(l10n_util::GetStringUTF16(IDS_GLANCEABLES_UP_NEXT));

  up_next_view_ =
      left_column->AddChildView(std::make_unique<GlanceablesUpNextView>());
  up_next_view_->SetProperty(views::kMarginsKey,
                             gfx::Insets::TLBR(12, 0, 0, 0));

  // Container for the views on the right.
  auto* right_column = container->AddChildView(std::make_unique<views::View>());
  right_column->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  if (show_session_restore) {
    // The "Restore last session" label.
    restore_session_label_ =
        right_column->AddChildView(std::make_unique<views::Label>());
    SetupSectionLabel(restore_session_label_);
    restore_session_label_->SetText(
        l10n_util::GetStringUTF16(IDS_GLANCEABLES_RESTORE_SESSION));

    restore_view_ =
        right_column->AddChildView(std::make_unique<GlanceablesRestoreView>());
    restore_view_->SetProperty(views::kMarginsKey,
                               gfx::Insets::TLBR(12, 0, 0, 0));
  }

  // Share space equally between the two columns.
  container_layout->SetFlexForView(left_column, 1);
  container_layout->SetFlexForView(right_column, 1);
}

GlanceablesView::~GlanceablesView() = default;

void GlanceablesView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  gfx::Rect local_bounds = GetLocalBounds();
  // This view fills the screen, so the margins are a fraction of the screen
  // height and width.
  const int vertical_margin = local_bounds.height() / 6;
  const int horizontal_margin = local_bounds.width() / 6;
  layout_->set_inside_border_insets(
      gfx::Insets::VH(vertical_margin, horizontal_margin));
}

void GlanceablesView::OnThemeChanged() {
  views::View::OnThemeChanged();
  // TODO(crbug.com/1353119): Use color provider.
  up_next_label_->SetEnabledColor(gfx::kGoogleGrey200);
  if (restore_session_label_)
    restore_session_label_->SetEnabledColor(gfx::kGoogleGrey200);
}

}  // namespace ash

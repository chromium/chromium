// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/activity_list_bubble/glic_activity_list_bubble.h"

#include <memory>
#include <utility>
#include <vector>

#include "chrome/browser/glic/browser_ui/activity_list_bubble/glic_activity_list_bubble_row_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(kActorTaskListBubbleView);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kActorTaskListBubbleScrollView);

namespace glic {

namespace {
const int kVerticalMargin = 8;
// Calculated as a max of 8 rows * 56 px per row. This is also inline with the
// extensions bubble max height (448) and the downloads bubble max height (450).
const int kMaxBubbleHeight = 448;
}  // namespace

GlicActivityListBubble::GlicActivityListBubble(
    std::vector<GlicActivityRowViewModel> row_models,
    base::RepeatingClosure on_destroyed)
    : row_models_(std::move(row_models)),
      on_destroyed_(std::move(on_destroyed)) {}

GlicActivityListBubble::~GlicActivityListBubble() {
  widget_observation_.Reset();
  Close();
}

void GlicActivityListBubble::Show(views::View* anchor_view) {
  auto contents_view = CreateContentsView();

  // If there are no rows, don't show the bubble.
  if (!contents_view || contents_view->children().empty()) {
    return;
  }

  std::unique_ptr<views::ScrollView> scroll_view =
      std::make_unique<views::ScrollView>(
          views::ScrollView::ScrollWithLayers::kEnabled);
  views::View* contents = scroll_view->SetContents(std::move(contents_view));
  scroll_view->ClipHeightTo(0, kMaxBubbleHeight);
  scroll_view->SetDrawOverflowIndicator(false);

  auto dialog_model =
      ui::DialogModel::Builder()
          .SetAccessibleTitle(
              l10n_util::GetStringUTF16(IDS_ACTOR_TASK_LIST_BUBBLE_A11Y_LABEL))
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  std::move(scroll_view),
                  views::BubbleDialogModelHost::FieldType::kMenuItem),
              kActorTaskListBubbleScrollView)
          .OverrideShowCloseButton(false)
          .Build();

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor_view, views::BubbleBorder::TOP_RIGHT);
  bubble->set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  bubble->set_margins(gfx::Insets::VH(kVerticalMargin, 0));
  // This bubble has no dialog buttons, so without an explicit initially focused
  // view the widget is activated with nothing focused inside it. In that state
  // BubbleDialogDelegate exposes the bubble as an alert dialog with no focused
  // descendant, which leaves screen readers unable to move into the bubble.
  // Focus the first actionable row instead so the bubble is exposed as a dialog
  // and keyboard focus lands on a control the user can activate.
  // Rows for tasks whose tab was closed are disabled and cannot take focus.
  // If every row is disabled there is nothing to focus, and the bubble keeps
  // its alert dialog role so its contents are still announced on show.
  for (views::View* row : contents->children()) {
    if (row->GetEnabled()) {
      bubble->SetInitiallyFocusedView(row);
      break;
    }
  }

  widget_ = views::BubbleDialogDelegate::CreateBubbleDeprecated(
      std::move(bubble), views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  if (!widget_) {
    return;
  }

  // Bubble can always show activated as it will only show in the active window.
  widget_->Show();
  widget_observation_.Reset();
  widget_observation_.Observe(widget_);
}

void GlicActivityListBubble::Close() {
  if (widget_) {
    widget_->Close();
    widget_ = nullptr;
  }
}

bool GlicActivityListBubble::IsShowing() const {
  return widget_ && widget_->IsVisible();
}

void GlicActivityListBubble::OnWidgetDestroyed(views::Widget* widget) {
  widget_observation_.Reset();
  widget_ = nullptr;
  if (on_destroyed_) {
    on_destroyed_.Run();
  }
}

std::unique_ptr<views::View> GlicActivityListBubble::CreateContentsView() {
  std::unique_ptr<views::View> contents_view =
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kVertical)
          .SetProperty(views::kElementIdentifierKey, kActorTaskListBubbleView)
          .Build();

  // Create rows in order of priority.
  for (const auto& model : row_models_) {
    contents_view->AddChildView(
        std::make_unique<GlicActivityListBubbleRowButton>(model));
  }
  return contents_view;
}

}  // namespace glic

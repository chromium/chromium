// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_selection_widget.h"

#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace glic {

namespace {

class GlicSelectionContentsView : public views::View {
  METADATA_HEADER(GlicSelectionContentsView, views::View)

 public:
  explicit GlicSelectionContentsView(base::RepeatingClosure on_click) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    button_ = AddChildView(std::make_unique<views::MdTextButton>(
        std::move(on_click), l10n_util::GetStringUTF16(
                                 IDS_GLIC_BUTTON_ENTRYPOINT_ASK_GEMINI_LABEL)));
    button_->SetStyle(ui::ButtonStyle::kProminent);
    button_->SetCornerRadius(16.0f);
    button_->SetCustomPadding(gfx::Insets::VH(4, 10));
    button_->SetBgColorIdOverride(ui::kColorSysPrimary);
    button_->SetEnabledTextColors(ui::kColorSysOnPrimary);
    button_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            GlicVectorIconManager::GetVectorIcon(IDR_GLIC_BUTTON_VECTOR_ICON),
            ui::kColorSysOnPrimary, 16));
  }

 private:
  raw_ptr<views::MdTextButton> button_ = nullptr;
};

BEGIN_METADATA(GlicSelectionContentsView)
END_METADATA

}  // namespace

// static
views::Widget* GlicSelectionWidgetDelegate::Show(
    content::WebContents* web_contents,
    const gfx::Rect& anchor_rect,
    base::RepeatingClosure on_click) {
  auto delegate = std::make_unique<GlicSelectionWidgetDelegate>(
      anchor_rect, std::move(on_click));
  if (web_contents) {
    delegate->set_parent_window(web_contents->GetNativeView());
  }
  views::Widget* widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(delegate));
  widget->ShowInactive();
  return widget;
}

GlicSelectionWidgetDelegate::GlicSelectionWidgetDelegate(
    const gfx::Rect& anchor_rect,
    base::RepeatingClosure on_click)
    : BubbleDialogDelegate(nullptr,
                           views::BubbleBorder::TOP_LEFT,
                           views::BubbleBorder::STANDARD_SHADOW,
                           /*autosize=*/true) {
  gfx::Rect adjusted_anchor = anchor_rect;
  // Align the left edge of the bubble near the right edge of the selection,
  // overlapping slightly so the icon sits under the end of the text.
  int overlap = 24;
  adjusted_anchor.set_x(
      std::max(anchor_rect.x(), anchor_rect.right() - overlap));
  adjusted_anchor.set_width(0);
  adjusted_anchor.Inset(gfx::Insets::TLBR(0, 0, -8, 0));
  SetAnchorRect(adjusted_anchor);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(false);
  // Remove default dialog margins so the custom button fills the entire bubble.
  set_margins(gfx::Insets(0));
  set_corner_radius(16);
  SetBackgroundColor(ui::ColorVariant(SK_ColorTRANSPARENT));
  SetCanActivate(false);

  auto button_click = base::BindRepeating(
      [](base::RepeatingClosure original_click,
         views::BubbleDialogDelegate* delegate) {
        if (delegate->GetWidget()) {
          delegate->GetWidget()->CloseWithReason(
              views::Widget::ClosedReason::kAcceptButtonClicked);
        }
        original_click.Run();
      },
      std::move(on_click), base::Unretained(this));

  SetContentsView(
      std::make_unique<GlicSelectionContentsView>(std::move(button_click)));
}

GlicSelectionWidgetDelegate::~GlicSelectionWidgetDelegate() = default;

}  // namespace glic

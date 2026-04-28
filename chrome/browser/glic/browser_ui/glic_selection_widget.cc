// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_selection_widget.h"

#include "base/strings/strcat.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace glic {

namespace {

constexpr size_t kMaxSelectionLengthForTooltip = 50;
constexpr int kIconSize = 20;

constexpr int kButtonCornerRadius = 14;

class GlicSelectionContentsView : public views::View {
  METADATA_HEADER(GlicSelectionContentsView, views::View)

 public:
  GlicSelectionContentsView(const std::u16string& selected_text,
                            base::RepeatingClosure on_ask_gemini,
                            base::RepeatingClosure on_copy,
                            base::RepeatingClosure on_copy_link) {
    auto layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(4, 4), 4);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    SetLayoutManager(std::move(layout));

    // Ask Gemini Button
    std::u16string truncated_text;
    if (selected_text.length() <= kMaxSelectionLengthForTooltip) {
      truncated_text = selected_text;
    } else {
      truncated_text = gfx::StringSlicer(selected_text, gfx::kEllipsisUTF16,
                                         /*elide_in_middle=*/true,
                                         /*elide_at_beginning=*/false)
                           .CutString(kMaxSelectionLengthForTooltip,
                                      /*insert_ellipsis=*/true);
    }
    auto ask_gemini_tooltip = l10n_util::GetStringFUTF16(
        IDS_GLIC_SELECTION_ASK_ABOUT,
        base::StrCat({u"\"", truncated_text, u"\""}));
    auto* ask_gemini_btn = AddChildView(std::make_unique<views::MdTextButton>(
        std::move(on_ask_gemini),
        l10n_util::GetStringUTF16(
            IDS_GLIC_BUTTON_ENTRYPOINT_ASK_GEMINI_LABEL)));
    ask_gemini_btn->SetStyle(ui::ButtonStyle::kText);
    ask_gemini_btn->SetTooltipText(ask_gemini_tooltip);
    ask_gemini_btn->SetImageLabelSpacing(4);
    ask_gemini_btn->SetEnabledTextColors(ui::kColorSysOnSurface);
    ask_gemini_btn->SetTextColor(views::Button::STATE_DISABLED,
                                 ui::kColorLabelForegroundDisabled);
    ask_gemini_btn->SetCustomPadding(
        views::LayoutProvider::Get()->GetInsetsMetric(
            views::INSETS_ICON_BUTTON));

    gfx::ImageSkia* icon_skia =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_GLIC_BUTTON_ALT_ICON);
    gfx::ImageSkia resized_icon = gfx::ImageSkiaOperations::CreateResizedImage(
        *icon_skia, skia::ImageOperations::RESIZE_BEST,
        gfx::Size(kIconSize, kIconSize));
    auto icon_model = ui::ImageModel::FromImageSkia(resized_icon);

    ask_gemini_btn->SetImageModel(views::Button::STATE_NORMAL, icon_model);
    ask_gemini_btn->SetImageModel(views::Button::STATE_HOVERED, icon_model);
    ask_gemini_btn->SetImageModel(views::Button::STATE_PRESSED, icon_model);
    ask_gemini_btn->SetImageModel(views::Button::STATE_DISABLED, icon_model);

    views::InkDrop::Get(ask_gemini_btn)
        ->SetMode(views::InkDropHost::InkDropMode::ON);
    ask_gemini_btn->SetHasInkDropActionOnClick(true);
    ask_gemini_btn->SetShowInkDropWhenHotTracked(true);
    views::InstallRoundRectHighlightPathGenerator(ask_gemini_btn, gfx::Insets(),
                                                  kButtonCornerRadius);
    views::InkDrop::Get(ask_gemini_btn)
        ->SetBaseColorCallback(base::BindRepeating(
            [](views::View* host) {
              return host->GetColorProvider()->GetColor(ui::kColorSysOnSurface);
            },
            base::Unretained(ask_gemini_btn)));
    CreateToolbarInkdropCallbacks(ask_gemini_btn, kColorToolbarInkDropHover,
                                  kColorToolbarInkDropRipple);

    // Copy Button
    auto copy_tooltip = gfx::LocateAndRemoveAcceleratorChar(
        l10n_util::GetStringUTF16(IDS_APP_COPY), nullptr, nullptr);
    auto* copy_btn = AddChildView(views::ImageButton::CreateIconButton(
        std::move(on_copy), vector_icons::kContentCopyIcon, copy_tooltip));
    copy_btn->SetTooltipText(copy_tooltip);
    copy_btn->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    copy_btn->SetBorder(
        views::CreateEmptyBorder(views::LayoutProvider::Get()->GetInsetsMetric(
            views::INSETS_ICON_BUTTON)));
    views::SetImageFromVectorIconWithColor(
        copy_btn, vector_icons::kContentCopyIcon, kIconSize,
        views::IconColors(ui::kColorSysOnSurface,
                          ui::kColorLabelForegroundDisabled,
                          ui::kColorSysOnSurface));
    CreateToolbarInkdropCallbacks(copy_btn, kColorToolbarInkDropHover,
                                  kColorToolbarInkDropRipple);

    // Copy Link Button
    auto copy_link_tooltip =
        l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_COPYLINKTOTEXT);
    copy_link_btn_ = AddChildView(views::ImageButton::CreateIconButton(
        std::move(on_copy_link), vector_icons::kLinkIcon, copy_link_tooltip));
    copy_link_btn_->SetTooltipText(copy_link_tooltip);
    copy_link_btn_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    copy_link_btn_->SetBorder(
        views::CreateEmptyBorder(views::LayoutProvider::Get()->GetInsetsMetric(
            views::INSETS_ICON_BUTTON)));
    views::SetImageFromVectorIconWithColor(
        copy_link_btn_, vector_icons::kLinkIcon, kIconSize,
        views::IconColors(ui::kColorSysOnSurface,
                          ui::kColorLabelForegroundDisabled,
                          ui::kColorSysOnSurface));
    CreateToolbarInkdropCallbacks(copy_link_btn_, kColorToolbarInkDropHover,
                                  kColorToolbarInkDropRipple);
    copy_link_btn_->SetEnabled(false);
  }

  void SetCopyLinkEnabled(bool enabled) {
    if (copy_link_btn_) {
      copy_link_btn_->SetEnabled(enabled);
    }
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    if (GetWidget() && GetWidget()->widget_delegate()) {
      // Force a background color update by temporarily clearing it, then
      // restoring it, which bypasses the color variant equality check.
      auto* bubble_delegate =
          GetWidget()->widget_delegate()->AsBubbleDialogDelegate();
      bubble_delegate->SetBackgroundColor(ui::ColorVariant());
      bubble_delegate->SetBackgroundColor(ui::ColorVariant(ui::kColorSysBase));
    }
  }

 private:
  raw_ptr<views::ImageButton> copy_link_btn_ = nullptr;
};

BEGIN_METADATA(GlicSelectionContentsView)
END_METADATA

}  // namespace

// static
views::Widget* GlicSelectionWidgetDelegate::Show(
    content::WebContents* web_contents,
    const gfx::Rect& anchor_rect,
    const std::u16string& selected_text,
    base::RepeatingClosure on_ask_gemini,
    base::RepeatingClosure on_copy,
    base::RepeatingClosure on_copy_link) {
  auto delegate = std::make_unique<GlicSelectionWidgetDelegate>(
      anchor_rect, selected_text, std::move(on_ask_gemini), std::move(on_copy),
      std::move(on_copy_link));
  if (web_contents) {
    delegate->set_parent_window(platform_util::GetViewForWindow(
        web_contents->GetTopLevelNativeWindow()));
  }
  views::Widget* widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(delegate));
  widget->ShowInactive();
  return widget;
}

GlicSelectionWidgetDelegate::GlicSelectionWidgetDelegate(
    const gfx::Rect& anchor_rect,
    const std::u16string& selected_text,
    base::RepeatingClosure on_ask_gemini,
    base::RepeatingClosure on_copy,
    base::RepeatingClosure on_copy_link)
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
  SetBackgroundColor(ui::ColorVariant(ui::kColorSysBase));
  SetCanActivate(false);

  auto button_click = [](base::RepeatingClosure original_click,
                         views::BubbleDialogDelegate* delegate) {
    if (delegate->GetWidget()) {
      delegate->GetWidget()->CloseWithReason(
          views::Widget::ClosedReason::kAcceptButtonClicked);
    }
    if (original_click) {
      original_click.Run();
    }
  };

  SetContentsView(std::make_unique<GlicSelectionContentsView>(
      selected_text,
      base::BindRepeating(button_click, std::move(on_ask_gemini),
                          base::Unretained(this)),
      base::BindRepeating(button_click, std::move(on_copy),
                          base::Unretained(this)),
      base::BindRepeating(button_click, std::move(on_copy_link),
                          base::Unretained(this))));
}

GlicSelectionWidgetDelegate::~GlicSelectionWidgetDelegate() = default;

void GlicSelectionWidgetDelegate::UpdateCopyLinkButton(bool enabled) {
  if (auto* contents_view =
          views::AsViewClass<GlicSelectionContentsView>(GetContentsView())) {
    contents_view->SetCopyLinkEnabled(enabled);
  }
}

}  // namespace glic

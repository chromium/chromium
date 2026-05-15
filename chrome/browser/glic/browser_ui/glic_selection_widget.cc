// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_selection_widget.h"

#include "base/strings/strcat.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
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
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace glic {

namespace {

constexpr size_t kMaxSelectionLengthForTooltip = 50;
constexpr int kIconSize = 14;

constexpr int kCornerRadius = 12;
// The two pills are visually grouped together by having a smaller border radius
// on the sides where they meet.
constexpr int kCornerRadiusInner = 4;

class GlicSelectionContentsView : public views::View {
  METADATA_HEADER(GlicSelectionContentsView, views::View)

 public:
  GlicSelectionContentsView(const std::u16string& selected_text,
                            bool initial_pinned_state,
                            base::RepeatingClosure on_ask_gemini,
                            base::RepeatingClosure on_copy,
                            base::RepeatingClosure on_copy_link,
                            base::RepeatingClosure on_toggle_pin,
                            base::RepeatingClosure on_dismiss) {
    SetNotifyEnterExitOnChild(true);

    auto border1 = std::make_unique<views::BubbleBorder>(
        views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW);
    border1->SetColor(ui::kColorSysSurface);
    border1->set_rounded_corners(gfx::RoundedCornersF(kCornerRadius));

    // BubbleBorders add a shadow inset on all sides. We use a negative
    // spacing here so the visible backgrounds of the pills are closer together
    // without their shadow insets pushing them far apart.
    constexpr int kVisualSpacing = 2;
    int spacing = kVisualSpacing - border1->GetInsets().right() -
                  border1->GetInsets().left();

    auto layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(0), spacing);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    SetLayoutManager(std::move(layout));

    ask_pill_ = AddChildView(std::make_unique<views::BoxLayoutView>());
    ask_pill_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    ask_pill_->SetInsideBorderInsets(gfx::Insets::VH(4, 4));
    ask_pill_->SetBetweenChildSpacing(4);
    ask_pill_->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    ask_pill_->SetBackground(
        std::make_unique<views::BubbleBackground>(border1.get()));
    ask_pill_->SetBorder(std::move(border1));

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
    auto* ask_gemini_btn =
        ask_pill_->AddChildView(std::make_unique<views::MdTextButton>(
            std::move(on_ask_gemini),
            l10n_util::GetStringUTF16(
                IDS_GLIC_BUTTON_ENTRYPOINT_ASK_GEMINI_LABEL)));
    ask_gemini_btn->SetStyle(ui::ButtonStyle::kText);
    ask_gemini_btn->SetTooltipText(ask_gemini_tooltip);
    ask_gemini_btn->SetImageLabelSpacing(4);
    ask_gemini_btn->SetEnabledTextColors(ui::kColorSysOnSurfaceVariant);
    ask_gemini_btn->SetTextColor(views::Button::STATE_DISABLED,
                                 ui::kColorLabelForegroundDisabled);
    ask_gemini_btn->SetLabelStyle(views::style::STYLE_BODY_5_MEDIUM);
    ask_gemini_btn->SetCustomPadding(gfx::Insets::TLBR(0, 2, 0, 6));

    gfx::ImageSkia* icon_skia =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_GLIC_BUTTON_ALT_ICON);
    gfx::ImageSkia resized_icon = gfx::ImageSkiaOperations::CreateResizedImage(
        *icon_skia, skia::ImageOperations::RESIZE_BEST,
        gfx::Size(kIconSize, kIconSize));

    auto generator = base::BindRepeating(
        [](gfx::ImageSkia icon, const ui::ColorProvider* color_provider) {
          if (!color_provider) {
            return icon;
          }
          SkColor circle_bg_color =
              color_provider->GetColor(ui::kColorSysBaseContainer);
          return gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
              10, circle_bg_color, icon);
        },
        resized_icon);

    auto icon_model = ui::ImageModel::FromImageGenerator(std::move(generator),
                                                         gfx::Size(20, 20));

    ask_gemini_btn->SetImageModel(views::Button::STATE_NORMAL, icon_model);
    ask_gemini_btn->SetImageModel(views::Button::STATE_HOVERED, icon_model);
    ask_gemini_btn->SetImageModel(views::Button::STATE_PRESSED, icon_model);
    ask_gemini_btn->SetImageModel(views::Button::STATE_DISABLED, icon_model);

    views::InkDrop::Get(ask_gemini_btn)
        ->SetMode(views::InkDropHost::InkDropMode::ON);
    ask_gemini_btn->SetHasInkDropActionOnClick(true);
    ask_gemini_btn->SetShowInkDropWhenHotTracked(true);
    views::InstallRoundRectHighlightPathGenerator(ask_gemini_btn, gfx::Insets(),
                                                  kCornerRadius);
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
    auto* copy_btn =
        ask_pill_->AddChildView(views::ImageButton::CreateIconButton(
            std::move(on_copy), vector_icons::kContentCopyIcon, copy_tooltip));
    copy_btn->SetTooltipText(copy_tooltip);
    copy_btn->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    copy_btn->SetBorder(
        views::CreateEmptyBorder(views::LayoutProvider::Get()->GetInsetsMetric(
            views::INSETS_VECTOR_IMAGE_BUTTON)));
    views::SetImageFromVectorIconWithColor(
        copy_btn, vector_icons::kContentCopyIcon, kIconSize,
        views::IconColors(ui::kColorSysOnSurfaceVariant,
                          ui::kColorLabelForegroundDisabled,
                          ui::kColorSysOnSurfaceVariant));
    CreateToolbarInkdropCallbacks(copy_btn, kColorToolbarInkDropHover,
                                  kColorToolbarInkDropRipple);

    // Copy Link Button
    auto copy_link_tooltip =
        l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_COPYLINKTOTEXT);
    copy_link_btn_ =
        ask_pill_->AddChildView(views::ImageButton::CreateIconButton(
            std::move(on_copy_link), omnibox::kShareChromeRefreshIcon,
            copy_link_tooltip));
    copy_link_btn_->SetTooltipText(copy_link_tooltip);
    copy_link_btn_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    copy_link_btn_->SetBorder(
        views::CreateEmptyBorder(views::LayoutProvider::Get()->GetInsetsMetric(
            views::INSETS_VECTOR_IMAGE_BUTTON)));
    views::SetImageFromVectorIconWithColor(
        copy_link_btn_, omnibox::kShareChromeRefreshIcon, kIconSize,
        views::IconColors(ui::kColorSysOnSurfaceVariant,
                          ui::kColorLabelForegroundDisabled,
                          ui::kColorSysOnSurfaceVariant));
    CreateToolbarInkdropCallbacks(copy_link_btn_, kColorToolbarInkDropHover,
                                  kColorToolbarInkDropRipple);
    copy_link_btn_->SetEnabled(false);

    // Pill 2
    dismiss_pill_ = AddChildView(std::make_unique<views::BoxLayoutView>());
    dismiss_pill_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    dismiss_pill_->SetInsideBorderInsets(gfx::Insets::VH(4, 4));
    dismiss_pill_->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    auto border2 = std::make_unique<views::BubbleBorder>(
        views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW);
    border2->SetColor(ui::kColorSysSurface);
    border2->set_rounded_corners(gfx::RoundedCornersF(
        kCornerRadiusInner, kCornerRadius, kCornerRadius, kCornerRadiusInner));
    dismiss_pill_->SetBackground(
        std::make_unique<views::BubbleBackground>(border2.get()));
    dismiss_pill_->SetBorder(std::move(border2));

    // Pin/Unpin Toggle Button or Dismiss Button
    if (features::kGlicSelectionPromptEnablePinning.Get()) {
      pin_btn_ =
          dismiss_pill_->AddChildView(views::ImageButton::CreateIconButton(
              std::move(on_toggle_pin), vector_icons::kCaretUpIcon,
              std::u16string()));
      pin_btn_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
      pin_btn_->SetBorder(views::CreateEmptyBorder(
          views::LayoutProvider::Get()->GetInsetsMetric(
              views::INSETS_VECTOR_IMAGE_BUTTON)));
      CreateToolbarInkdropCallbacks(pin_btn_, kColorToolbarInkDropHover,
                                    kColorToolbarInkDropRipple);
      SetPinned(initial_pinned_state);
    } else {
      auto dismiss_tooltip = l10n_util::GetStringUTF16(IDS_APP_CLOSE);
      dismiss_btn_ =
          dismiss_pill_->AddChildView(views::ImageButton::CreateIconButton(
              std::move(on_dismiss), vector_icons::kCloseIcon,
              dismiss_tooltip));
      dismiss_btn_->SetTooltipText(dismiss_tooltip);
      dismiss_btn_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
      dismiss_btn_->SetBorder(views::CreateEmptyBorder(
          views::LayoutProvider::Get()->GetInsetsMetric(
              views::INSETS_VECTOR_IMAGE_BUTTON)));
      views::SetImageFromVectorIconWithColor(
          dismiss_btn_, vector_icons::kCloseIcon, kIconSize,
          views::IconColors(ui::kColorSysOnSurfaceVariant,
                            ui::kColorLabelForegroundDisabled,
                            ui::kColorSysOnSurfaceVariant));
      CreateToolbarInkdropCallbacks(dismiss_btn_, kColorToolbarInkDropHover,
                                    kColorToolbarInkDropRipple);
    }

    dismiss_pill_->SetPaintToLayer();
    dismiss_pill_->layer()->SetFillsBoundsOpaquely(false);
    dismiss_pill_->layer()->SetOpacity(0.0f);
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    if (dismiss_pill_) {
      dismiss_pill_->layer()->SetOpacity(1.0f);
    }
    if (ask_pill_) {
      auto* bubble_border =
          static_cast<views::BubbleBorder*>(ask_pill_->GetBorder());
      if (bubble_border) {
        bubble_border->set_rounded_corners(
            gfx::RoundedCornersF(kCornerRadius, kCornerRadiusInner,
                                 kCornerRadiusInner, kCornerRadius));
        ask_pill_->SchedulePaint();
      }
    }
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    if (dismiss_pill_) {
      dismiss_pill_->layer()->SetOpacity(0.0f);
    }
    if (ask_pill_) {
      auto* bubble_border =
          static_cast<views::BubbleBorder*>(ask_pill_->GetBorder());
      if (bubble_border) {
        bubble_border->set_rounded_corners(gfx::RoundedCornersF(kCornerRadius));
        ask_pill_->SchedulePaint();
      }
    }
  }

  void SetCopyLinkEnabled(bool enabled) {
    if (copy_link_btn_) {
      copy_link_btn_->SetEnabled(enabled);
    }
  }

  void SetPinned(bool is_pinned) {
    if (pin_btn_) {
      const gfx::VectorIcon& icon =
          is_pinned ? vector_icons::kCaretDownIcon : vector_icons::kCaretUpIcon;
      views::SetImageFromVectorIconWithColor(
          pin_btn_, icon, kIconSize,
          views::IconColors(ui::kColorSysOnSurfaceVariant,
                            ui::kColorLabelForegroundDisabled,
                            ui::kColorSysOnSurfaceVariant));
      pin_btn_->SetTooltipText(l10n_util::GetStringUTF16(
          is_pinned ? IDS_TAB_SEARCH_BUTTON_CXMENU_UNPIN
                    : IDS_TAB_SEARCH_BUTTON_CXMENU_PIN));
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
      bubble_delegate->SetBackgroundColor(
          ui::ColorVariant(SK_ColorTRANSPARENT));
    }
  }

 private:
  raw_ptr<views::ImageButton> copy_link_btn_ = nullptr;
  raw_ptr<views::ImageButton> pin_btn_ = nullptr;
  raw_ptr<views::ImageButton> dismiss_btn_ = nullptr;
  raw_ptr<views::BoxLayoutView> ask_pill_ = nullptr;
  raw_ptr<views::BoxLayoutView> dismiss_pill_ = nullptr;
};

BEGIN_METADATA(GlicSelectionContentsView)
END_METADATA

}  // namespace

// static
views::Widget* GlicSelectionWidgetDelegate::Show(
    content::WebContents* web_contents,
    const gfx::Rect& anchor_rect,
    const std::u16string& selected_text,
    bool is_pinned,
    base::RepeatingClosure on_ask_gemini,
    base::RepeatingClosure on_copy,
    base::RepeatingClosure on_copy_link,
    base::RepeatingCallback<void(bool)> on_pin_toggled,
    base::RepeatingClosure on_dismiss) {
  // Both `GetContainerBounds` and `GetTextSelectionBounds` (from which
  // `anchor_rect` originates) return global screen coordinates in DIPs, so
  // relative positions and scales are compatible.
  gfx::Rect window_bounds =
      web_contents ? web_contents->GetContainerBounds() : gfx::Rect();
  auto delegate = std::make_unique<GlicSelectionWidgetDelegate>(
      anchor_rect, window_bounds, selected_text, is_pinned,
      std::move(on_ask_gemini), std::move(on_copy), std::move(on_copy_link),
      std::move(on_pin_toggled), std::move(on_dismiss));
  if (web_contents) {
    delegate->set_parent_window(platform_util::GetViewForWindow(
        web_contents->GetTopLevelNativeWindow()));
  }
  views::Widget* widget = views::BubbleDialogDelegate::CreateBubbleDeprecated(
      std::move(delegate),
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  widget->ShowInactive();
  return widget;
}

GlicSelectionWidgetDelegate::GlicSelectionWidgetDelegate(
    const gfx::Rect& anchor_rect,
    const gfx::Rect& window_bounds,
    const std::u16string& selected_text,
    bool is_pinned,
    base::RepeatingClosure on_ask_gemini,
    base::RepeatingClosure on_copy,
    base::RepeatingClosure on_copy_link,
    base::RepeatingCallback<void(bool)> on_pin_toggled,
    base::RepeatingClosure on_dismiss)
    : BubbleDialogDelegate(nullptr,
                           views::BubbleBorder::TOP_LEFT,
                           views::BubbleBorder::STANDARD_SHADOW,
                           /*autosize=*/true),
      original_anchor_rect_(anchor_rect),
      window_bounds_(window_bounds),
      is_pinned_(is_pinned),
      on_pin_toggled_callback_(std::move(on_pin_toggled)) {
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

  auto contents_view = std::make_unique<GlicSelectionContentsView>(
      selected_text, is_pinned_,
      base::BindRepeating(button_click, std::move(on_ask_gemini),
                          base::Unretained(this)),
      base::BindRepeating(button_click, std::move(on_copy),
                          base::Unretained(this)),
      base::BindRepeating(button_click, std::move(on_copy_link),
                          base::Unretained(this)),
      base::BindRepeating(&GlicSelectionWidgetDelegate::TogglePinState,
                          base::Unretained(this)),
      base::BindRepeating(button_click, std::move(on_dismiss),
                          base::Unretained(this)));

  SetContentsView(std::move(contents_view));

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(false);
  // Remove default dialog margins so the custom button fills the entire bubble.
  set_margins(gfx::Insets(0));
  set_corner_radius(kCornerRadius);
  SetBackgroundColor(ui::ColorVariant(SK_ColorTRANSPARENT));
  set_shadow(views::BubbleBorder::NO_SHADOW);
  SetCanActivate(false);

  UpdatePosition();
}

GlicSelectionWidgetDelegate::~GlicSelectionWidgetDelegate() = default;

void GlicSelectionWidgetDelegate::TogglePinState() {
  is_pinned_ = !is_pinned_;
  if (on_pin_toggled_callback_) {
    on_pin_toggled_callback_.Run(is_pinned_);
  }
  if (auto* contents_view =
          views::AsViewClass<GlicSelectionContentsView>(GetContentsView())) {
    contents_view->SetPinned(is_pinned_);
  }

  if (GetWidget() && GetWidget()->GetLayer()) {
    ui::ScopedLayerAnimationSettings settings(
        GetWidget()->GetLayer()->GetAnimator());
    settings.SetTransitionDuration(base::Milliseconds(250));
    settings.SetTweenType(gfx::Tween::EASE_OUT_2);
    UpdatePosition();
    SizeToContents();
  } else {
    UpdatePosition();
    SizeToContents();
  }
}

void GlicSelectionWidgetDelegate::UpdatePosition() {
  int total_width = GetContentsView()->GetPreferredSize().width();
  if (is_pinned_) {
    // The pill has a BubbleBorder with a STANDARD_SHADOW. This shadow adds
    // a large invisible inset (typically ~24px). We must subtract this inset
    // so the visual top of the pill aligns with our target. We also subtract
    // the default 4px arrow gap added by BubbleDialogDelegate for TOP_LEFT.
    int top_inset = 0;
    if (auto* contents_view = GetContentsView()) {
      if (!contents_view->children().empty()) {
        top_inset = contents_view->children()[0]->GetInsets().top();
      }
    }

    // Pinned to the top right area, overlapping toolbar.
    // window_bounds_ is in global screen coords.
    constexpr int kWindowEdgePadding = 16;
    int pinned_x = window_bounds_.right() - total_width - kWindowEdgePadding;

    int pinned_y = window_bounds_.y() - 8 - top_inset - 4;
    SetAnchorRect(gfx::Rect(pinned_x, pinned_y, 0, 0));
  } else {
    // Unpinned: anchored inline near selection.
    int ask_gemini_width = 120;
    if (auto* contents_view = GetContentsView()) {
      if (!contents_view->children().empty()) {
        auto* ask_pill = contents_view->children()[0].get();
        if (!ask_pill->children().empty()) {
          ask_gemini_width =
              ask_pill->children()[0]->GetPreferredSize().width();
        }
      }
    }
    gfx::Rect adjusted_anchor = original_anchor_rect_;
    int overlap = 4 + ask_gemini_width / 2;
    int target_x = std::max(original_anchor_rect_.x(),
                            original_anchor_rect_.right() - overlap);

    // TODO(crbug.com/507481568): Handle cases where the widget spills off the
    // bottom of the window, and use RTL code (see ui/gfx/text_utils.h) to flip
    // the layout when needed.
    if (!window_bounds_.IsEmpty()) {
      constexpr int kWindowEdgePadding = 16;
      target_x = std::max(target_x, window_bounds_.x() + kWindowEdgePadding);
      if (target_x + total_width >
          window_bounds_.right() - kWindowEdgePadding) {
        target_x = window_bounds_.right() - total_width - kWindowEdgePadding;
      }
    }

    adjusted_anchor.set_x(target_x);
    adjusted_anchor.set_width(0);
    adjusted_anchor.Inset(gfx::Insets::TLBR(0, 0, -8, 0));
    SetAnchorRect(adjusted_anchor);
  }
}

views::ClientView* GlicSelectionWidgetDelegate::CreateClientView(
    views::Widget* widget) {
  views::ClientView* client_view =
      views::BubbleDialogDelegate::CreateClientView(widget);
  if (client_view->layer()) {
    client_view->layer()->SetFillsBoundsOpaquely(false);
  }
  return client_view;
}

void GlicSelectionWidgetDelegate::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  params->shadow_type = views::Widget::InitParams::ShadowType::kNone;
}

void GlicSelectionWidgetDelegate::UpdateCopyLinkButton(bool enabled) {
  if (auto* contents_view =
          views::AsViewClass<GlicSelectionContentsView>(GetContentsView())) {
    contents_view->SetCopyLinkEnabled(enabled);
  }
}

}  // namespace glic

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_selection_widget.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace glic {

namespace {

constexpr size_t kMaxSelectionLengthForTooltip = 50;
constexpr int kIconSize = 16;

// Corner radius following Chrome Material 3 design specs:
// 10dp for compact floating pills, 16dp for larger card containers/buttons.
constexpr int kCornerRadius = 10;
constexpr int kSmallChipCornerRadius = 14;
constexpr float kSmallChipHoverScale = 1.10f;
constexpr base::TimeDelta kFadeInDuration = base::Milliseconds(250);
constexpr base::TimeDelta kHoverAnimationDuration = base::Milliseconds(100);

std::u16string GetCtaLabel() {
  std::string cta = features::kGlicSelectionPromptCta.Get();
  if (cta == features::kGlicSelectionPromptCtaTellMe) {
    return l10n_util::GetStringUTF16(IDS_GLIC_SELECTION_CTA_TELL_ME);
  }
  if (cta == features::kGlicSelectionPromptCtaExplain) {
    return l10n_util::GetStringUTF16(IDS_GLIC_SELECTION_CTA_EXPLAIN);
  }
  return l10n_util::GetStringUTF16(IDS_GLIC_BUTTON_ENTRYPOINT_ASK_GEMINI_LABEL);
}

class GlicSelectionContentsView : public views::View {
  METADATA_HEADER(GlicSelectionContentsView, views::View)

 public:
  GlicSelectionContentsView(GlicSelectionWidgetDelegate* widget_delegate,
                            const std::u16string& selected_text)
      : widget_delegate_(widget_delegate) {
    const bool is_small_chip =
        base::FeatureList::IsEnabled(features::kGlicSelectionSmallChip);
    SetNotifyEnterExitOnChild(true);
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

    auto border1 = std::make_unique<views::BubbleBorder>(
        views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW);
    border1->set_background_color(ui::kColorSysSurface);
    if (is_small_chip) {
      if (features::kGlicSelectionSmallChipOnTop.Get()) {
        border1->set_rounded_corners(
            gfx::RoundedCornersF(kSmallChipCornerRadius, kSmallChipCornerRadius,
                                 kSmallChipCornerRadius, 0));
      } else {
        border1->set_rounded_corners(gfx::RoundedCornersF(
            0, kSmallChipCornerRadius, kSmallChipCornerRadius,
            kSmallChipCornerRadius));
      }
    } else {
      border1->set_rounded_corners(gfx::RoundedCornersF(kCornerRadius));
    }

    // BubbleBorders add a shadow inset on all sides. We use a negative
    // spacing here so the visible backgrounds of the pills are closer together
    // without their shadow insets pushing them far apart.
    constexpr int kVisualSpacing = 2;
    int spacing = kVisualSpacing - border1->GetInsets().right() -
                  border1->GetInsets().left();

    constexpr int kHoverPadding = 4;
    auto layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        is_small_chip ? gfx::Insets(kHoverPadding) : gfx::Insets(0), spacing);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    SetLayoutManager(std::move(layout));

    ask_pill_ = AddChildView(std::make_unique<views::BoxLayoutView>());
    ask_pill_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    ask_pill_->SetInsideBorderInsets(
        is_small_chip ? gfx::Insets(0) : gfx::Insets::TLBR(2, 3, 2, 0));
    ask_pill_->SetBetweenChildSpacing(is_small_chip ? 0 : 2);
    ask_pill_->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    ask_pill_->SetBackground(
        std::make_unique<views::BubbleBackground>(border1.get()));
    ask_pill_->SetBorder(std::move(border1));
    if (is_small_chip) {
      ask_pill_->SetPaintToLayer();
      ask_pill_->layer()->SetFillsBoundsOpaquely(false);
    }

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
    std::u16string cta_label = is_small_chip ? u"" : GetCtaLabel();
    auto* ask_gemini_btn =
        ask_pill_->AddChildView(std::make_unique<views::MdTextButton>(
            base::BindRepeating(
                &GlicSelectionContentsView::OnAskGeminiButtonClicked,
                base::Unretained(this)),
            cta_label));
    ask_gemini_btn->SetProperty(
        views::kElementIdentifierKey,
        GlicSelectionWidgetDelegate::kAskGeminiButtonElementId);
    ask_gemini_btn->SetStyle(ui::ButtonStyle::kText);
    ask_gemini_btn->SetTooltipText(ask_gemini_tooltip);
    ask_gemini_btn->SetImageLabelSpacing(is_small_chip ? 0 : 6);
    ask_gemini_btn->SetEnabledTextColors(ui::kColorSysOnSurface);
    ask_gemini_btn->SetTextColor(views::Button::STATE_DISABLED,
                                 ui::kColorLabelForegroundDisabled);
    ask_gemini_btn->SetLabelStyle(views::style::STYLE_BODY_3_MEDIUM);
    if (is_small_chip) {
      constexpr int kSmallChipSize = 2 * kSmallChipCornerRadius;
      ask_gemini_btn->SetMinSize(gfx::Size(0, 0));
      ask_gemini_btn->SetPreferredSize(
          gfx::Size(kSmallChipSize, kSmallChipSize));
      ask_gemini_btn->SetBorder(views::NullBorder());
    } else {
      ask_gemini_btn->SetCustomPadding(gfx::Insets::TLBR(5, 6, 5, 6));
    }
    ask_gemini_btn->SetBgColorOverrideDeprecated(SK_ColorTRANSPARENT);
    ask_gemini_btn->SetInstallFocusRingOnFocus(false);

    ask_gemini_btn_ = ask_gemini_btn;

    gfx::ImageSkia* icon_skia =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_GLIC_BUTTON_ALT_ICON);
    gfx::ImageSkia resized_icon = gfx::ImageSkiaOperations::CreateResizedImage(
        *icon_skia, skia::ImageOperations::RESIZE_BEST,
        gfx::Size(kIconSize, kIconSize));
    active_icon_model_ = ui::ImageModel::FromImageSkia(resized_icon);

    auto inactive_generator = base::BindRepeating(
        [](const ui::ColorProvider* color_provider) -> gfx::ImageSkia {
          if (!color_provider) {
            return gfx::ImageSkia();
          }
          const gfx::VectorIcon& vector_icon =
              glic::GlicVectorIconManager::GetVectorIcon(
                  IDR_GLIC_BUTTON_VECTOR_ICON);
          return gfx::CreateVectorIcon(
              vector_icon, kIconSize,
              color_provider->GetColor(ui::kColorSysOnSurfaceSubtle));
        });

    inactive_icon_model_ = ui::ImageModel::FromImageGenerator(
        std::move(inactive_generator), gfx::Size(20, 20));

    ask_gemini_btn->SetImageModel(views::Button::STATE_NORMAL,
                                  inactive_icon_model_);

    if (is_small_chip) {
      ask_gemini_btn->GetViewAccessibility().SetName(ask_gemini_tooltip);
      ask_gemini_btn->SetShowInkDropWhenHotTracked(false);
      ask_gemini_btn->SetHasInkDropActionOnClick(false);
      views::InkDrop::Get(ask_gemini_btn)
          ->SetMode(views::InkDropHost::InkDropMode::OFF);
      ask_gemini_btn_subscription_ =
          ask_gemini_btn->AddStateChangedCallback(base::BindRepeating(
              &GlicSelectionContentsView::OnAskGeminiStateChanged,
              base::Unretained(this)));
    } else {
      ask_gemini_btn->SetImageModel(views::Button::STATE_HOVERED,
                                    active_icon_model_);
      ask_gemini_btn->SetImageModel(views::Button::STATE_PRESSED,
                                    active_icon_model_);
      ask_gemini_btn->SetImageModel(views::Button::STATE_DISABLED,
                                    inactive_icon_model_);
      views::InkDrop::Get(ask_gemini_btn)
          ->SetMode(views::InkDropHost::InkDropMode::ON);
      ask_gemini_btn->SetHasInkDropActionOnClick(true);
      ask_gemini_btn->SetShowInkDropWhenHotTracked(true);
      constexpr int kBtnCornerRadius = kCornerRadius - 2;
      views::InstallRoundRectHighlightPathGenerator(
          ask_gemini_btn, gfx::Insets(), kBtnCornerRadius);
      ask_gemini_btn->SetCornerRadius(kBtnCornerRadius);
    }

    if (features::kGlicSelectionShowCopyButtons.Get() && !is_small_chip) {
      // Copy Button
      auto copy_tooltip = gfx::LocateAndRemoveAcceleratorChar(
          l10n_util::GetStringUTF16(IDS_APP_COPY), nullptr, nullptr);
      auto* copy_btn =
          ask_pill_->AddChildView(views::ImageButton::CreateIconButton(
              base::BindRepeating(
                  &GlicSelectionWidgetDelegate::ActionDelegate::OnCopy,
                  base::Unretained(&widget_delegate_->action_delegate())),
              features::IsRoundedIconsEnabled()
                  ? vector_icons::kContentCopyIcon
                  : vector_icons::kContentCopyOldIcon,
              copy_tooltip));
      copy_btn->SetTooltipText(copy_tooltip);
      copy_btn->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
      copy_btn->SetBorder(views::CreateEmptyBorder(
          views::LayoutProvider::Get()->GetInsetsMetric(
              views::INSETS_VECTOR_IMAGE_BUTTON)));
      views::SetImageFromVectorIconWithColor(
          copy_btn,
          features::IsRoundedIconsEnabled() ? vector_icons::kContentCopyIcon
                                            : vector_icons::kContentCopyOldIcon,
          kIconSize,
          views::IconColors(ui::kColorSysOnSurfaceSubtle,
                            ui::kColorLabelForegroundDisabled,
                            ui::kColorSysOnSurfaceSubtle));
      CreateToolbarInkdropCallbacks(copy_btn, kColorToolbarInkDropHover,
                                    kColorToolbarInkDropRipple);

      // Copy Link Button
      auto copy_link_tooltip =
          l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_COPYLINKTOTEXT);
      copy_link_btn_ =
          ask_pill_->AddChildView(views::ImageButton::CreateIconButton(
              base::BindRepeating(
                  &GlicSelectionWidgetDelegate::ActionDelegate::OnCopyLink,
                  base::Unretained(&widget_delegate_->action_delegate())),
              features::IsRoundedIconsEnabled()
                  ? omnibox::kShareIcon
                  : omnibox::kShareChromeRefreshOldIcon,
              copy_link_tooltip));
      copy_link_btn_->SetTooltipText(copy_link_tooltip);
      copy_link_btn_->SetImageVerticalAlignment(
          views::ImageButton::ALIGN_MIDDLE);
      copy_link_btn_->SetBorder(views::CreateEmptyBorder(
          views::LayoutProvider::Get()->GetInsetsMetric(
              views::INSETS_VECTOR_IMAGE_BUTTON)));
      views::SetImageFromVectorIconWithColor(
          copy_link_btn_,
          features::IsRoundedIconsEnabled()
              ? omnibox::kShareIcon
              : omnibox::kShareChromeRefreshOldIcon,
          kIconSize,
          views::IconColors(ui::kColorSysOnSurfaceSubtle,
                            ui::kColorLabelForegroundDisabled,
                            ui::kColorSysOnSurfaceSubtle));
      CreateToolbarInkdropCallbacks(copy_link_btn_, kColorToolbarInkDropHover,
                                    kColorToolbarInkDropRipple);
      copy_link_btn_->SetEnabled(false);
    }

    if (!is_small_chip) {
      // Integrated Options Section (instead of separate pill)
      close_pill_ =
          ask_pill_->AddChildView(std::make_unique<views::BoxLayoutView>());
      close_pill_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
      close_pill_->SetInsideBorderInsets(gfx::Insets::TLBR(4, 0, 4, 3));
      close_pill_->SetBetweenChildSpacing(2);
      close_pill_->SetCrossAxisAlignment(
          views::BoxLayout::CrossAxisAlignment::kCenter);

      auto close_tooltip = l10n_util::GetStringUTF16(IDS_CLOSE);
      const gfx::VectorIcon& close_icon = features::IsRoundedIconsEnabled()
                                              ? vector_icons::kCloseIcon
                                              : vector_icons::kCloseOldIcon;
      close_btn_ =
          close_pill_->AddChildView(views::ImageButton::CreateIconButton(
              base::BindRepeating(
                  &GlicSelectionWidgetDelegate::ActionDelegate::OnHide,
                  base::Unretained(&widget_delegate_->action_delegate())),
              close_icon, close_tooltip));
      close_btn_->SetTooltipText(close_tooltip);
      close_btn_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
      close_btn_->SetBorder(views::CreateEmptyBorder(
          views::LayoutProvider::Get()->GetInsetsMetric(
              views::INSETS_VECTOR_IMAGE_BUTTON)));
      views::SetImageFromVectorIconWithColor(
          close_btn_, close_icon, kIconSize,
          views::IconColors(ui::kColorSysOnSurfaceSubtle,
                            ui::kColorLabelForegroundDisabled,
                            ui::kColorSysOnSurfaceSubtle));
      CreateToolbarInkdropCallbacks(close_btn_, kColorToolbarInkDropHover,
                                    kColorToolbarInkDropRipple);
      close_btn_subscription_ = close_btn_->AddStateChangedCallback(
          base::BindRepeating(&GlicSelectionContentsView::RefreshAskGeminiState,
                              base::Unretained(this)));

      close_pill_->SetPaintToLayer();
      close_pill_->layer()->SetFillsBoundsOpaquely(false);
    }
  }

  void RefreshAskGeminiState() {
    if (!ask_gemini_btn_ ||
        base::FeatureList::IsEnabled(features::kGlicSelectionSmallChip)) {
      return;
    }
    bool close_active =
        close_btn_ && (close_btn_->GetState() == views::Button::STATE_HOVERED ||
                       close_btn_->HasFocus());
    bool is_hovered = IsMouseHovered() && !close_active;
    ask_gemini_btn_->SetHotTracked(is_hovered);
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    RefreshAskGeminiState();
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    RefreshAskGeminiState();
  }

  void OnAskGeminiStateChanged() {
    if (!ask_gemini_btn_ || !ask_pill_ || !ask_pill_->layer()) {
      return;
    }
    bool is_hovered =
        ask_gemini_btn_->GetState() == views::Button::STATE_HOVERED;
    float scale = is_hovered ? kSmallChipHoverScale : 1.0f;

    int shadow_left = ask_pill_->GetInsets().left();
    int shadow_top = ask_pill_->GetInsets().top();
    gfx::PointF center(shadow_left + kSmallChipCornerRadius,
                       shadow_top + kSmallChipCornerRadius);
    gfx::Transform transform;
    transform.Translate(center.x(), center.y());
    transform.Scale(scale, scale);
    transform.Translate(-center.x(), -center.y());

    base::TimeDelta duration = gfx::Animation::ShouldRenderRichAnimation()
                                   ? kHoverAnimationDuration
                                   : base::TimeDelta();
    views::AnimationBuilder()
        .SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .Once()
        .SetDuration(duration)
        .SetTransform(ask_pill_->layer(), transform,
                      gfx::Tween::FAST_OUT_SLOW_IN);
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    if (widget_delegate_) {
      // During a theme change, the OS-level native window background is
      // automatically reset to opaque defaults without updating.
      widget_delegate_->SetBackgroundColor(
          ui::ColorVariant(SK_ColorTRANSPARENT));
    }
  }

  // Non-virtual helper methods:
  void SetCopyLinkEnabled(bool enabled) {
    if (copy_link_btn_) {
      copy_link_btn_->SetEnabled(enabled);
    }
  }

  void OnAskGeminiButtonClicked() {
    if (ask_gemini_btn_) {
      ask_gemini_btn_->SetEnabled(false);
    }
    if (widget_delegate_) {
      widget_delegate_->action_delegate().OnAskGemini();
    }
  }

 private:
  const raw_ptr<GlicSelectionWidgetDelegate> widget_delegate_;
  raw_ptr<views::MdTextButton> ask_gemini_btn_ = nullptr;
  ui::ImageModel inactive_icon_model_;
  ui::ImageModel active_icon_model_;
  raw_ptr<views::ImageButton> copy_link_btn_ = nullptr;
  raw_ptr<views::BoxLayoutView> ask_pill_ = nullptr;
  base::CallbackListSubscription ask_gemini_btn_subscription_;
  raw_ptr<views::ImageButton> close_btn_ = nullptr;
  raw_ptr<views::BoxLayoutView> close_pill_ = nullptr;
  base::CallbackListSubscription close_btn_subscription_;
};

BEGIN_METADATA(GlicSelectionContentsView)
END_METADATA

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(GlicSelectionWidgetDelegate,
                                      kAskGeminiButtonElementId);

GlicSelectionWidgetDelegate::GlicSelectionWidgetDelegate(
    ActionDelegate& action_delegate,
    const gfx::Rect& anchor_rect,
    const std::u16string& selected_text)
    : BubbleDialogDelegate(
          nullptr,
          base::FeatureList::IsEnabled(features::kGlicSelectionSmallChip)
              ? (features::kGlicSelectionSmallChipOnTop.Get()
                     ? views::BubbleBorder::BOTTOM_LEFT
                     : views::BubbleBorder::TOP_LEFT)
              : views::BubbleBorder::BOTTOM_RIGHT,
          views::BubbleBorder::STANDARD_SHADOW,
          /*autosize=*/true),
      action_delegate_(action_delegate),
      original_anchor_rect_(anchor_rect) {
  SetContentsView(
      std::make_unique<GlicSelectionContentsView>(this, selected_text));

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(false);
  // Remove default dialog margins so the custom button fills the entire bubble.
  set_margins(gfx::Insets(0));
  set_corner_radius(kCornerRadius);
  SetBackgroundColor(ui::ColorVariant(SK_ColorTRANSPARENT));
  set_shadow(views::BubbleBorder::NO_SHADOW);
  set_adjust_if_offscreen(false);

  // Flip the chip below the selection if it overflows the top of the page.
  const gfx::Rect page_bounds = action_delegate_->GetContainerBounds();
  if (!views::BubbleBorder::is_arrow_on_top(arrow()) &&
      !page_bounds.IsEmpty() &&
      original_anchor_rect_.y() -
              GetContentsView()->GetPreferredSize().height() <
          page_bounds.y()) {
    // TODO(b/535254667): Update the small chip's sharp corner when flipped.
    SetArrowWithoutResizing(
        base::FeatureList::IsEnabled(features::kGlicSelectionSmallChip)
            ? views::BubbleBorder::TOP_LEFT
            : views::BubbleBorder::TOP_RIGHT);
  }

  UpdatePosition();
}

GlicSelectionWidgetDelegate::~GlicSelectionWidgetDelegate() = default;

void GlicSelectionWidgetDelegate::ShowWidget() {
  widget_ = views::BubbleDialogDelegate::CreateBubble(
      this, base::BindOnce(&GlicSelectionWidgetDelegate::OnWidgetClose,
                           weak_ptr_factory_.GetWeakPtr()));
  widget_->ShowInactive();

  ui::Layer* anim_layer =
      GetContentsView() ? GetContentsView()->layer() : nullptr;
  if (anim_layer && gfx::Animation::ShouldRenderRichAnimation()) {
    anim_layer->SetOpacity(0.0f);
    ui::ScopedLayerAnimationSettings settings(anim_layer->GetAnimator());
    settings.SetTweenType(gfx::Tween::Type::EASE_IN_OUT);
    settings.SetTransitionDuration(kFadeInDuration);
    anim_layer->SetOpacity(1.0f);
  }
}

void GlicSelectionWidgetDelegate::CloseWidget() {
  OnWidgetClose(views::Widget::ClosedReason::kUnspecified);
}

void GlicSelectionWidgetDelegate::OnWidgetClose(
    views::Widget::ClosedReason reason) {
  if (widget_) {
    // Hide the widget immediately to provide instant visual feedback to the
    // user.
    widget_->Hide();
    // The widget cannot be destroyed synchronously here because this callback
    // is often called from within a Widget observer iteration (e.g., inside
    // OnWidgetActivationChanged). Doing so would destroy the observer list.
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(widget_));
  }
  action_delegate_->OnWidgetClose();
}

void GlicSelectionWidgetDelegate::UpdatePosition() {
  gfx::Rect adjusted_anchor = original_anchor_rect_;
  if (base::FeatureList::IsEnabled(features::kGlicSelectionSmallChip)) {
    int left_inset = 0;
    int y_inset = 0;
    const bool is_on_top = !views::BubbleBorder::is_arrow_on_top(arrow());
    if (auto* contents_view = GetContentsView()) {
      left_inset = contents_view->GetInsets().left();
      y_inset = is_on_top ? contents_view->GetInsets().bottom()
                          : contents_view->GetInsets().top();
      if (!contents_view->children().empty()) {
        views::View* pill_view = contents_view->children()[0];
        left_inset += pill_view->GetInsets().left();
        y_inset += is_on_top ? pill_view->GetInsets().bottom()
                             : pill_view->GetInsets().top();
      }
    }
    adjusted_anchor.Offset(original_anchor_rect_.width() - left_inset,
                           is_on_top ? y_inset : -y_inset);
  } else {
    int right_inset = 0;
    int visible_width = GetContentsView()->GetPreferredSize().width();
    if (auto* contents_view = GetContentsView()) {
      if (!contents_view->children().empty()) {
        views::View* pill_view = contents_view->children()[0];
        right_inset = pill_view->GetInsets().right();
        visible_width = pill_view->GetPreferredSize().width();
      }
    }
    adjusted_anchor.Offset(right_inset + (visible_width / 2), 0);
  }
  SetAnchorRect(adjusted_anchor);
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

gfx::Rect GlicSelectionWidgetDelegate::GetBubbleBounds() {
  gfx::Rect bounds = views::BubbleDialogDelegate::GetBubbleBounds();
  const gfx::Rect page_bounds = action_delegate_->GetContainerBounds();
  if (page_bounds.IsEmpty()) {
    return bounds;
  }
  bounds.set_x(std::clamp(
      bounds.x(), page_bounds.x(),
      std::max(page_bounds.x(), page_bounds.right() - bounds.width())));
  return bounds;
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

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_selection_widget.h"

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace glic {

namespace {

constexpr size_t kMaxSelectionLengthForTooltip = 50;
constexpr int kIconSize = 14;

constexpr int kCornerRadius = 10;
constexpr base::TimeDelta kFadeInDuration = base::Milliseconds(250);

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
    SetNotifyEnterExitOnChild(true);
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

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
    ask_pill_->SetInsideBorderInsets(gfx::Insets::VH(2, 2));
    ask_pill_->SetBetweenChildSpacing(2);
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
            base::BindRepeating(
                &GlicSelectionWidgetDelegate::ActionDelegate::OnAskGemini,
                base::Unretained(&widget_delegate_->action_delegate())),
            GetCtaLabel()));
    ask_gemini_btn->SetStyle(ui::ButtonStyle::kText);
    ask_gemini_btn->SetTooltipText(ask_gemini_tooltip);
    ask_gemini_btn->SetImageLabelSpacing(4);
    ask_gemini_btn->SetEnabledTextColors(ui::kColorSysOnSurface);
    ask_gemini_btn->SetTextColor(views::Button::STATE_DISABLED,
                                 ui::kColorLabelForegroundDisabled);
    ask_gemini_btn->SetLabelStyle(views::style::STYLE_BODY_2_MEDIUM);
    ask_gemini_btn->SetCustomPadding(gfx::Insets::TLBR(0, 4, 0, 6));
    ask_gemini_btn->SetBgColorOverrideDeprecated(SK_ColorTRANSPARENT);
    ask_gemini_btn->SetInstallFocusRingOnFocus(false);

    ask_gemini_btn_ = ask_gemini_btn;

    gfx::ImageSkia* icon_skia =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_GLIC_BUTTON_ALT_ICON);
    gfx::ImageSkia resized_icon = gfx::ImageSkiaOperations::CreateResizedImage(
        *icon_skia, skia::ImageOperations::RESIZE_BEST,
        gfx::Size(kIconSize, kIconSize));
    auto active_generator = base::BindRepeating(
        [](gfx::ImageSkia icon, const ui::ColorProvider* color_provider) {
          return icon;
        },
        resized_icon);

    active_icon_model_ = ui::ImageModel::FromImageGenerator(
        std::move(active_generator), gfx::Size(20, 20));

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
              color_provider->GetColor(ui::kColorSysOnSurfaceVariant));
        });

    inactive_icon_model_ = ui::ImageModel::FromImageGenerator(
        std::move(inactive_generator), gfx::Size(20, 20));

    ask_gemini_btn_->SetImageModel(views::Button::STATE_NORMAL,
                                   inactive_icon_model_);
    ask_gemini_btn_->SetImageModel(views::Button::STATE_HOVERED,
                                   active_icon_model_);
    ask_gemini_btn_->SetImageModel(views::Button::STATE_PRESSED,
                                   active_icon_model_);
    ask_gemini_btn_->SetImageModel(views::Button::STATE_DISABLED,
                                   inactive_icon_model_);

    views::InkDrop::Get(ask_gemini_btn)
        ->SetMode(views::InkDropHost::InkDropMode::ON);
    ask_gemini_btn->SetHasInkDropActionOnClick(true);
    ask_gemini_btn->SetShowInkDropWhenHotTracked(true);
    views::InstallRoundRectHighlightPathGenerator(ask_gemini_btn, gfx::Insets(),
                                                  kCornerRadius - 2);

    if (features::kGlicSelectionShowCopyButtons.Get()) {
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
          views::IconColors(ui::kColorSysOnSurfaceVariant,
                            ui::kColorLabelForegroundDisabled,
                            ui::kColorSysOnSurfaceVariant));
      CreateToolbarInkdropCallbacks(copy_link_btn_, kColorToolbarInkDropHover,
                                    kColorToolbarInkDropRipple);
      copy_link_btn_->SetEnabled(false);
    }

    // Integrated Options Section (instead of separate pill)
    control_pill_ =
        ask_pill_->AddChildView(std::make_unique<views::BoxLayoutView>());
    control_pill_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    // Extra space on the left to show the division from the CTA buttons.
    control_pill_->SetInsideBorderInsets(gfx::Insets(0));
    control_pill_->SetBetweenChildSpacing(2);
    control_pill_->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    auto close_tooltip = l10n_util::GetStringUTF16(IDS_CLOSE);
    const gfx::VectorIcon& close_icon =
        features::IsRoundedIconsEnabled()
            ? vector_icons::kCloseIcon
            : vector_icons::kCloseOldIcon;
    close_btn_ =
        control_pill_->AddChildView(views::ImageButton::CreateIconButton(
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
        views::IconColors(ui::kColorSysOnSurfaceVariant,
                          ui::kColorLabelForegroundDisabled,
                          ui::kColorSysOnSurfaceVariant));
    CreateToolbarInkdropCallbacks(close_btn_, kColorToolbarInkDropHover,
                                  kColorToolbarInkDropRipple);
    close_btn_subscription_ = close_btn_->AddStateChangedCallback(
        base::BindRepeating(&GlicSelectionContentsView::RefreshAskGeminiState,
                            base::Unretained(this)));

    control_pill_->SetPaintToLayer();
    control_pill_->layer()->SetFillsBoundsOpaquely(false);
  }

  void RefreshAskGeminiState() {
    if (!ask_gemini_btn_) {
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


  void UpdateAskGeminiIcon(bool is_hovered) {
    if (!ask_gemini_btn_) {
      return;
    }
    const ui::ImageModel& normal_model =
        is_hovered ? active_icon_model_ : inactive_icon_model_;
    ask_gemini_btn_->SetImageModel(views::Button::STATE_NORMAL, normal_model);
  }

 private:
  const raw_ptr<GlicSelectionWidgetDelegate> widget_delegate_;
  raw_ptr<views::MdTextButton> ask_gemini_btn_ = nullptr;
  ui::ImageModel inactive_icon_model_;
  ui::ImageModel active_icon_model_;
  raw_ptr<views::ImageButton> copy_link_btn_ = nullptr;
  raw_ptr<views::BoxLayoutView> ask_pill_ = nullptr;
  raw_ptr<views::ImageButton> close_btn_ = nullptr;
  raw_ptr<views::BoxLayoutView> control_pill_ = nullptr;
  base::CallbackListSubscription close_btn_subscription_;
};

BEGIN_METADATA(GlicSelectionContentsView)
END_METADATA

}  // namespace

GlicSelectionWidgetDelegate::GlicSelectionWidgetDelegate(
    ActionDelegate& action_delegate,
    const gfx::Rect& anchor_rect,
    const gfx::Rect& window_bounds,
    const std::u16string& selected_text)
    : BubbleDialogDelegate(nullptr,
                           views::BubbleBorder::BOTTOM_RIGHT,
                           views::BubbleBorder::STANDARD_SHADOW,
                           /*autosize=*/true),
      action_delegate_(action_delegate),
      original_anchor_rect_(anchor_rect),
      window_bounds_(window_bounds) {
  SetContentsView(
      std::make_unique<GlicSelectionContentsView>(this, selected_text));

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(false);
  // Remove default dialog margins so the custom button fills the entire bubble.
  set_margins(gfx::Insets(0));
  set_corner_radius(kCornerRadius);
  SetBackgroundColor(ui::ColorVariant(SK_ColorTRANSPARENT));
  set_shadow(views::BubbleBorder::NO_SHADOW);

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
  // Anchored inline near selection. Account for the invisible shadow inset
  // and half the visible width so the center of the popup pill aligns
  // exactly with the right edge of the selection.
  int right_inset = 0;
  int visible_width = GetContentsView()->GetPreferredSize().width();
  if (auto* contents_view = GetContentsView()) {
    if (!contents_view->children().empty()) {
      views::View* pill_view = contents_view->children()[0];
      right_inset = pill_view->GetInsets().right();
      visible_width = pill_view->GetPreferredSize().width();
    }
  }
  gfx::Rect adjusted_anchor = original_anchor_rect_;
  adjusted_anchor.Offset(right_inset + (visible_width / 2), 0);
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

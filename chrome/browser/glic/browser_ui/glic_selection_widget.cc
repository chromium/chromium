// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_selection_widget.h"

#include "base/command_line.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
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
#include "third_party/re2/src/re2/re2.h"
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
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace glic {

namespace {

constexpr size_t kMaxSelectionLengthForTooltip = 50;
constexpr int kIconSize = 16;

// Corner radius following Chrome Material 3 design specs:
// 10dp for compact floating pills, 16dp for larger card containers/buttons.
constexpr int kCornerRadius = 10;
constexpr base::TimeDelta kFadeInDuration = base::Milliseconds(250);

// Layout constants for inline explanation view:
constexpr int kExplanationContainerInsets = 8;
constexpr int kExplanationChildSpacing = 4;
constexpr int kExplanationCornerRadius = 16;
constexpr int kExplanationPreferredWidth = 320;
constexpr int kExplanationPreferredHeight = 200;
constexpr int kExplanationLabelMaxWidth = 300;

constexpr int kHeaderRowChildSpacing = 8;
constexpr int kButtonRowChildSpacing = 4;
constexpr int kLoadingRowVerticalPadding = 32;
constexpr int kThrobberDiameter = 24;
constexpr int kPillButtonCornerRadius = 16;
constexpr int kPillButtonImageLabelSpacing = 6;
constexpr int kAskMoreRowTopPadding = 4;
constexpr base::TimeDelta kBoundsAnimationDuration = base::Milliseconds(250);

// Regex pattern for inline markdown query links: * [Label](query:search_term) or - [Label](prompt:search_term)
constexpr char kQueryButtonPattern[] =
    R"(\s*[\*\-]\s*\[([^\]]+)\]\((?:query|prompt):([^)]+)\))";

// Regex pattern for cleaning inline query links: [text](query:foo) -> text
constexpr char kInlineQueryPattern[] =
    R"(\[([^\]]+)\]\((?:query|prompt):[^)]*\)?)";

enum class MarkdownElementType {
  kPlainText,
  kQueryButton,
};

struct MarkdownElement {
  MarkdownElementType type;
  std::string text;
  std::string query;
};

std::string CleanMarkdownText(const std::string& raw_text,
                              const re2::RE2& inline_query_regex) {
  std::string cleaned = raw_text;
  re2::RE2::GlobalReplace(&cleaned, inline_query_regex, "\\1");
  base::RemoveChars(cleaned, "[]", &cleaned);
  base::ReplaceSubstringsAfterOffset(&cleaned, 0, "**", "");
  base::ReplaceSubstringsAfterOffset(&cleaned, 0, "__", "");
  base::ReplaceSubstringsAfterOffset(&cleaned, 0, "`", "");

  if (base::StartsWith(cleaned, "* ") || base::StartsWith(cleaned, "- ")) {
    cleaned = "• " + cleaned.substr(2);
  }
  return cleaned;
}

std::vector<MarkdownElement> ParseMarkdownElements(
    const std::string& markdown_output,
    bool is_complete,
    const re2::RE2& query_button_regex,
    const re2::RE2& inline_query_regex) {
  std::vector<MarkdownElement> elements;
  std::vector<std::string> lines = base::SplitString(
      markdown_output, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  for (const std::string& line : lines) {
    std::string trimmed(base::TrimWhitespaceASCII(line, base::TRIM_ALL));
    // Filter out empty lines and markdown headers (lines starting with "# ") to
    // keep the compact inline card concise and readable.
    if (trimmed.empty() || base::StartsWith(trimmed, "# ")) {
      continue;
    }

    std::string btn_label, query_str;
    if (re2::RE2::FullMatch(trimmed, query_button_regex, &btn_label,
                            &query_str)) {
      base::RemoveChars(query_str, "<>", &query_str);
      query_str = base::UnescapeURLComponent(
          query_str,
          base::UnescapeRule::NORMAL | base::UnescapeRule::SPACES |
              base::UnescapeRule::PATH_SEPARATORS |
              base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);

      // Verify that if query_str represents a URL, it does not use a dangerous
      // scheme such as javascript: or data:.
      GURL parsed_url(query_str);
      if (parsed_url.is_valid() &&
          (parsed_url.SchemeIs(url::kJavaScriptScheme) ||
           parsed_url.SchemeIs(url::kDataScheme))) {
        continue;
      }

      elements.push_back(
          {MarkdownElementType::kQueryButton, btn_label, query_str});
      continue;
    }

    // While streaming (!is_complete), an incomplete markdown link (e.g. "* [..."
    // or "- [...") that has not yet finished streaming may fail the full regex
    // match above. Skip rendering the raw partial prefix until streaming
    // completes or the full button syntax is received.
    if (!is_complete &&
        (base::StartsWith(trimmed, "* [") ||
         base::StartsWith(trimmed, "- ["))) {
      continue;
    }

    elements.push_back(
        {MarkdownElementType::kPlainText,
         CleanMarkdownText(trimmed, inline_query_regex), ""});
  }
  return elements;
}

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

std::u16string GetSkillMenuLabel(
    const GlicSelectionWidgetDelegate::SkillOption& skill) {
  std::u16string name_utf16 = base::UTF8ToUTF16(skill.name);
  if (skill.icon.empty()) {
    return name_utf16;
  }
  return base::UTF8ToUTF16(skill.icon) + u" " + name_utf16;
}

class GlicSelectionContentsView : public views::View,
                                  public views::ContextMenuController,
                                  public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(GlicSelectionContentsView, views::View)

 public:
  GlicSelectionContentsView(GlicSelectionWidgetDelegate* widget_delegate,
                            const std::u16string& selected_text)
      : widget_delegate_(widget_delegate), selected_text_(selected_text) {
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
    ask_pill_->SetInsideBorderInsets(gfx::Insets::TLBR(2, 3, 2, 0));
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
                &GlicSelectionContentsView::OnAskGeminiButtonClicked,
                base::Unretained(this)),
            GetCtaLabel()));
    ask_gemini_btn->SetStyle(ui::ButtonStyle::kText);
    ask_gemini_btn->SetTooltipText(ask_gemini_tooltip);
    ask_gemini_btn->SetImageLabelSpacing(6);
    ask_gemini_btn->SetEnabledTextColors(ui::kColorSysOnSurface);
    ask_gemini_btn->SetTextColor(views::Button::STATE_DISABLED,
                                 ui::kColorLabelForegroundDisabled);
    ask_gemini_btn->SetLabelStyle(views::style::STYLE_BODY_3_MEDIUM);
    ask_gemini_btn->SetCustomPadding(gfx::Insets::TLBR(5, 6, 5, 6));
    ask_gemini_btn->SetBgColorOverrideDeprecated(SK_ColorTRANSPARENT);
    ask_gemini_btn->SetInstallFocusRingOnFocus(false);

    ask_gemini_btn_ = ask_gemini_btn;
    ask_gemini_btn_->set_context_menu_controller(this);

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
              color_provider->GetColor(ui::kColorSysOnSurfaceSubtle));
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
    ask_gemini_btn->SetCornerRadius(kCornerRadius - 2);

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

    // Integrated Options Section (instead of separate pill)
    close_pill_ =
        ask_pill_->AddChildView(std::make_unique<views::BoxLayoutView>());
    close_pill_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    close_pill_->SetInsideBorderInsets(gfx::Insets::TLBR(4, 0, 4, 3));
    close_pill_->SetBetweenChildSpacing(2);
    close_pill_->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    auto close_tooltip = l10n_util::GetStringUTF16(IDS_CLOSE);
    const gfx::VectorIcon& close_icon =
        features::IsRoundedIconsEnabled()
            ? vector_icons::kCloseIcon
            : vector_icons::kCloseOldIcon;
    close_btn_ = close_pill_->AddChildView(views::ImageButton::CreateIconButton(
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

  void UpdateContent(const std::u16string& selected_text) {
    selected_text_ = selected_text;
    if (ask_gemini_btn_) {
      ask_gemini_btn_->SetText(GetCtaLabel());
      ask_gemini_btn_->SetImageLabelSpacing(4);
    }
    if (copy_link_btn_) {
      copy_link_btn_->SetEnabled(false);
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

  void OnAskGeminiButtonClicked() {
    if (ask_gemini_btn_) {
      ask_gemini_btn_->SetEnabled(false);
    }
    if (widget_delegate_) {
      widget_delegate_->action_delegate().OnAskGemini();

      if (widget_delegate_->action_delegate().IsInlineFulfillmentSupported()) {
        expansion_timer_.Start(
            FROM_HERE, base::Milliseconds(150),
            base::BindOnce(&GlicSelectionContentsView::OnExpansionTimerFired,
                          base::Unretained(this)));
      }
    }
  }

  void OnExpansionTimerFired() {
    ShowInlineExplanation(/*markdown_output=*/"", /*is_complete=*/false,
                          /*error_message=*/"");
  }

  void OnQueryLinkClicked(const std::string& query,
                          const std::string& explanation_text) {
    if (widget_delegate_) {
      widget_delegate_->action_delegate().OnAskGeminiMoreAboutThis(
          base::UTF8ToUTF16(query), explanation_text);
    }
  }

  void OnAskGeminiMoreClicked(const std::u16string& selected_text,
                              const std::string& explanation_text) {
    if (widget_delegate_) {
      widget_delegate_->action_delegate().OnAskGeminiMoreAboutThis(
          selected_text, explanation_text);
    }
  }

  void OnSidePanelClicked() {
    if (widget_delegate_) {
      widget_delegate_->action_delegate().OnOpenInSidePanel();
    }
  }

  void OnCloseClicked() {
    if (widget_delegate_) {
      widget_delegate_->action_delegate().OnWidgetClose();
    }
  }

  bool is_explaining() const { return explanation_container_ != nullptr; }
  int initial_pill_width() const { return initial_pill_width_ > 0 ? initial_pill_width_ : 150; }

  void HidePills() {
    if (ask_pill_) {
      ask_pill_->SetVisible(false);
    }
    if (close_pill_) {
      close_pill_->SetVisible(false);
    }
  }

  void EnsureExplanationContainer() {
    HidePills();
    if (explanation_container_) {
      return;
    }

    if (ask_pill_ && initial_pill_width_ == 0) {
      initial_pill_width_ = ask_pill_->GetPreferredSize().width();
    }
    auto* layout = static_cast<views::BoxLayout*>(GetLayoutManager());
    layout->SetOrientation(views::BoxLayout::Orientation::kVertical);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);

    auto container = std::make_unique<views::BoxLayoutView>();
    container->SetOrientation(views::BoxLayout::Orientation::kVertical);
    container->SetInsideBorderInsets(gfx::Insets(kExplanationContainerInsets));
    container->SetBetweenChildSpacing(kExplanationChildSpacing);
    container->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kStart);

    auto border = std::make_unique<views::BubbleBorder>(
        views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW);
    border->SetColor(ui::kColorSysSurface);
    border->set_rounded_corners(
        gfx::RoundedCornersF(kExplanationCornerRadius));
    container->SetBackground(
        std::make_unique<views::BubbleBackground>(border.get()));
    container->SetBorder(std::move(border));
    container->SetProperty(views::kMarginsKey, gfx::Insets(0));
    container->SetPreferredSize(
        gfx::Size(kExplanationPreferredWidth, kExplanationPreferredHeight));
    explanation_container_ = AddChildView(std::move(container));
  }

  std::unique_ptr<views::View> BuildHeaderRow() {
    auto header_row = std::make_unique<views::BoxLayoutView>();
    header_row->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    header_row->SetBetweenChildSpacing(kHeaderRowChildSpacing);
    header_row->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    auto spacer = std::make_unique<views::View>();
    header_row->SetFlexForView(header_row->AddChildView(std::move(spacer)), 1);

    auto button_row = std::make_unique<views::BoxLayoutView>();
    button_row->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    button_row->SetBetweenChildSpacing(kButtonRowChildSpacing);
    button_row->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    auto* close_btn =
        button_row->AddChildView(views::ImageButton::CreateIconButton(
            base::BindRepeating(&GlicSelectionContentsView::OnCloseClicked,
                                base::Unretained(this)),
            vector_icons::kCloseIcon,
            l10n_util::GetStringUTF16(IDS_APP_CLOSE)));
    close_btn->SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_CLOSE));
    close_btn->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    close_btn->SetBorder(views::CreateEmptyBorder(
        views::LayoutProvider::Get()->GetInsetsMetric(
            views::INSETS_VECTOR_IMAGE_BUTTON)));
    views::SetImageFromVectorIconWithColor(
        close_btn, vector_icons::kCloseIcon, kIconSize,
        views::IconColors(ui::kColorSysOnSurfaceVariant,
                          ui::kColorLabelForegroundDisabled,
                          ui::kColorSysOnSurfaceVariant));
    CreateToolbarInkdropCallbacks(close_btn, kColorToolbarInkDropHover,
                                  kColorToolbarInkDropRipple);

    header_row->AddChildView(std::move(button_row));
    return header_row;
  }

  void BuildLoadingRow(views::View* parent) {
    auto loading_row = std::make_unique<views::BoxLayoutView>();
    loading_row->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    loading_row->SetMainAxisAlignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    loading_row->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    loading_row->SetInsideBorderInsets(
        gfx::Insets::VH(kLoadingRowVerticalPadding, 0));

    auto throbber = std::make_unique<views::Throbber>(kThrobberDiameter);
    throbber->Start();
    loading_row->AddChildView(std::move(throbber));
    parent->AddChildView(std::move(loading_row));
  }

  void BuildLink(views::View* parent,
                 const MarkdownElement& elem,
                 const std::string& markdown_output) {
    auto link = std::make_unique<views::MdTextButton>(
        base::BindRepeating(
            &GlicSelectionContentsView::OnQueryLinkClicked,
            base::Unretained(this), elem.query, markdown_output),
        base::UTF8ToUTF16(elem.text), views::style::CONTEXT_BUTTON);
    link->SetStyle(ui::ButtonStyle::kText);
    link->SetBgColorIdOverride(ui::kColorSysSurfaceVariant);
    link->SetCornerRadius(kPillButtonCornerRadius);
    link->SetEnabledTextColors(ui::kColorSysOnSurfaceVariant);
    link->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(vector_icons::kUndoIcon,
                                       ui::kColorSysOnSurfaceVariant,
                                       kIconSize));
    link->SetImageLabelSpacing(kPillButtonImageLabelSpacing);
    parent->AddChildView(std::move(link));
  }

  views::Label* BuildLabel(views::View* parent, const std::u16string& text) {
    auto label = std::make_unique<views::Label>(
        text, views::style::CONTEXT_LABEL, views::style::STYLE_BODY_2);
    label->SetMultiLine(true);
    label->SetSelectable(true);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetMaximumWidth(kExplanationLabelMaxWidth);
    return parent->AddChildView(std::move(label));
  }

  std::unique_ptr<views::View> BuildBodyView(
      const std::vector<MarkdownElement>& elements,
      const std::string& markdown_output) {
    auto body_view = std::make_unique<views::BoxLayoutView>();
    body_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
    body_view->SetBetweenChildSpacing(kExplanationChildSpacing);
    body_view->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kStart);

    if (markdown_output.empty()) {
      BuildLoadingRow(body_view.get());
    } else {
      for (const auto& elem : elements) {
        if (elem.type == MarkdownElementType::kQueryButton) {
          BuildLink(body_view.get(), elem, markdown_output);
        } else {
          BuildLabel(body_view.get(), base::UTF8ToUTF16(elem.text));
        }
      }
    }
    return body_view;
  }

  std::unique_ptr<views::View> BuildAskMoreRow(
      const std::string& markdown_output) {
    auto ask_more_row = std::make_unique<views::BoxLayoutView>();
    ask_more_row->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    ask_more_row->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);
    ask_more_row->SetInsideBorderInsets(
        gfx::Insets::TLBR(kAskMoreRowTopPadding, 0, 0, 0));

    auto ask_more_btn = std::make_unique<views::MdTextButton>(
        base::BindRepeating(
            &GlicSelectionContentsView::OnAskGeminiMoreClicked,
            base::Unretained(this), selected_text_, markdown_output),
        l10n_util::GetStringUTF16(IDS_GLIC_SELECTION_ASK_MORE_ABOUT_THIS),
        views::style::CONTEXT_BUTTON);
    ask_more_btn->SetStyle(ui::ButtonStyle::kText);
    ask_more_btn->SetBgColorIdOverride(ui::kColorSysSurfaceVariant);
    ask_more_btn->SetCornerRadius(kPillButtonCornerRadius);
    ask_more_btn->SetEnabledTextColors(ui::kColorSysOnSurfaceVariant);
    ask_more_btn->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(vector_icons::kUndoIcon,
                                       ui::kColorSysOnSurfaceVariant,
                                       kIconSize));
    ask_more_btn->SetImageLabelSpacing(kPillButtonImageLabelSpacing);
    ask_more_row->AddChildView(std::move(ask_more_btn));
    return ask_more_row;
  }

  void UpdateWidgetBounds(bool is_first_show,
                          bool is_complete,
                          bool has_content) {
    if (widget_delegate_ && (is_first_show || is_complete || has_content)) {
      if (is_first_show && widget_delegate_->GetWidget() &&
          widget_delegate_->GetWidget()->GetLayer()) {
        gfx::Point top_left =
            widget_delegate_->GetWidget()->GetWindowBoundsInScreen().origin();
        ui::ScopedLayerAnimationSettings animation(
            widget_delegate_->GetWidget()->GetLayer()->GetAnimator());
        animation.SetTransitionDuration(kBoundsAnimationDuration);
        animation.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
        widget_delegate_->SetArrowWithoutResizing(views::BubbleBorder::TOP_LEFT);
        widget_delegate_->SetAnchorRect(gfx::Rect(top_left, gfx::Size(0, 0)));
        widget_delegate_->SizeToContents();
      } else {
        widget_delegate_->SizeToContents();
      }
    }
  }

  void ShowInlineExplanation(const std::string& markdown_output,
                              bool is_complete,
                              const std::string& error_message) {
    expansion_timer_.Stop();
    bool is_first_show = (explanation_container_ == nullptr);

    EnsureExplanationContainer();
    // Clear previously rendered content (e.g., throbber or earlier streaming tokens)
    // inside the explanation container before re-building the updated view tree.
    explanation_container_->RemoveAllChildViews();

    if (!error_message.empty()) {
      BuildLabel(explanation_container_,
                 base::UTF8ToUTF16("Error: " + error_message));
    } else {
      explanation_container_->AddChildView(BuildHeaderRow());

      auto elements = ParseMarkdownElements(
          markdown_output, is_complete, query_button_regex_,
          inline_query_regex_);
      auto scroll_view = std::make_unique<views::ScrollView>();
      scroll_view->SetHorizontalScrollBarMode(
          views::ScrollView::ScrollBarMode::kDisabled);
      scroll_view->SetVerticalScrollBarMode(
          views::ScrollView::ScrollBarMode::kEnabled);
      scroll_view->SetContents(BuildBodyView(elements, markdown_output));

      explanation_container_->SetFlexForView(
          explanation_container_->AddChildView(std::move(scroll_view)), 1);

      if (!markdown_output.empty()) {
        explanation_container_->AddChildView(BuildAskMoreRow(markdown_output));
      }
    }

    UpdateWidgetBounds(is_first_show, is_complete, !markdown_output.empty());
  }

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override {
    if (!widget_delegate_ || !features::kGlicSelectionPromptSkills.Get()) {
      return;
    }
    command_id_to_skill_.clear();
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
    more_skills_submenu_model_.reset();

    std::vector<GlicSelectionWidgetDelegate::SkillOption> contextual_skills =
        widget_delegate_->action_delegate().GetContextualSkills();
    std::vector<GlicSelectionWidgetDelegate::SkillOption> user_skills =
        widget_delegate_->action_delegate().GetUserSkills();

    if (contextual_skills.empty() && user_skills.empty()) {
      return;
    }

    int next_command_id = GlicSelectionWidgetDelegate::kMinSkillCommandId;

    if (!user_skills.empty()) {
      menu_model_->AddTitle(
          l10n_util::GetStringUTF16(IDS_GLIC_SELECTION_YOUR_SKILLS));
      constexpr size_t kMaxTopLevelUserSkills = 2;
      for (size_t i = 0; i < user_skills.size() && i < kMaxTopLevelUserSkills;
           ++i) {
        int command_id = next_command_id++;
        command_id_to_skill_.push_back(user_skills[i]);
        menu_model_->AddItem(command_id, GetSkillMenuLabel(user_skills[i]));
      }

      if (user_skills.size() > kMaxTopLevelUserSkills) {
        more_skills_submenu_model_ =
            std::make_unique<ui::SimpleMenuModel>(this);
        for (size_t i = kMaxTopLevelUserSkills; i < user_skills.size(); ++i) {
          int command_id = next_command_id++;
          command_id_to_skill_.push_back(user_skills[i]);
          more_skills_submenu_model_->AddItem(
              command_id, GetSkillMenuLabel(user_skills[i]));
        }
        int submenu_command_id = next_command_id++;
        command_id_to_skill_.emplace_back(skills::Skill());
        menu_model_->AddSubMenu(
            submenu_command_id,
            l10n_util::GetStringUTF16(IDS_GLIC_SELECTION_MORE_SKILLS),
            more_skills_submenu_model_.get());
      }
    }

    if (!user_skills.empty() && !contextual_skills.empty()) {
      menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    }

    if (!contextual_skills.empty()) {
      menu_model_->AddTitle(
          l10n_util::GetStringUTF16(IDS_GLIC_SELECTION_FOR_THIS_PAGE));
      for (const auto& skill : contextual_skills) {
        int command_id = next_command_id++;
        command_id_to_skill_.push_back(skill);
        menu_model_->AddItem(command_id, GetSkillMenuLabel(skill));
      }
    }

    int run_flags =
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU;
    menu_runner_ =
        std::make_unique<views::MenuRunner>(menu_model_.get(), run_flags);
    menu_runner_->RunMenuAt(widget_delegate_->GetWidget(), nullptr,
                            ask_gemini_btn_->GetBoundsInScreen(),
                            views::MenuAnchorPosition::kTopLeft, source_type);
  }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    int index = command_id - GlicSelectionWidgetDelegate::kMinSkillCommandId;
    if (index >= 0 &&
        index < static_cast<int>(command_id_to_skill_.size())) {
      const auto& skill = command_id_to_skill_[index];
      if (!skill.id.empty() && widget_delegate_) {
        widget_delegate_->action_delegate().OnAskGeminiWithSkill(skill);
      }
    }
  }

  views::View* GetAskGeminiButtonForTesting() const { return ask_gemini_btn_; }
  bool IsContextMenuShowingForTesting() const {
    return menu_runner_ && menu_runner_->IsRunning();
  }
  ui::SimpleMenuModel* GetContextMenuModelForTesting() const {
    return menu_model_.get();
  }

 private:
  const re2::RE2 query_button_regex_{kQueryButtonPattern};
  const re2::RE2 inline_query_regex_{kInlineQueryPattern};
  int initial_pill_width_ = 0;
  const raw_ptr<GlicSelectionWidgetDelegate> widget_delegate_;
  std::u16string selected_text_;
  base::OneShotTimer expansion_timer_;
  raw_ptr<views::MdTextButton> ask_gemini_btn_ = nullptr;
  ui::ImageModel inactive_icon_model_;
  ui::ImageModel active_icon_model_;
  raw_ptr<views::ImageButton> copy_link_btn_ = nullptr;
  raw_ptr<views::BoxLayoutView> ask_pill_ = nullptr;
  raw_ptr<views::ImageButton> close_btn_ = nullptr;
  raw_ptr<views::BoxLayoutView> close_pill_ = nullptr;
  base::CallbackListSubscription close_btn_subscription_;
  raw_ptr<views::BoxLayoutView> explanation_container_ = nullptr;
  std::vector<GlicSelectionWidgetDelegate::SkillOption> command_id_to_skill_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> more_skills_submenu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
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
  if (auto* contents_view = GetContentsView()) {
    if (auto* glic_view =
            views::AsViewClass<GlicSelectionContentsView>(contents_view)) {
      if (glic_view->is_explaining()) {
        return;
      }
    }
  }
  int right_inset = 0;
  int visible_width = GetContentsView()->GetPreferredSize().width();
  if (auto* contents_view = GetContentsView()) {
    if (!contents_view->children().empty()) {
      views::View* pill_view = contents_view->children()[0];
      right_inset = pill_view->GetInsets().right();
      if (auto* glic_view =
              views::AsViewClass<GlicSelectionContentsView>(contents_view)) {
        if (glic_view->is_explaining()) {
          visible_width = glic_view->initial_pill_width();
        } else {
          visible_width = pill_view->GetPreferredSize().width();
        }
      } else {
        visible_width = pill_view->GetPreferredSize().width();
      }
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

void GlicSelectionWidgetDelegate::ShowInlineExplanation(
    const std::string& markdown_output,
    bool is_complete,
    const std::string& error_message) {
  if (auto* contents_view =
          views::AsViewClass<GlicSelectionContentsView>(GetContentsView())) {
    contents_view->ShowInlineExplanation(markdown_output, is_complete,
                                         error_message);
  }
}

views::View* GlicSelectionWidgetDelegate::GetAskGeminiButtonForTesting() {
  if (auto* contents =
          views::AsViewClass<GlicSelectionContentsView>(GetContentsView())) {
    return contents->GetAskGeminiButtonForTesting();
  }
  return nullptr;
}

bool GlicSelectionWidgetDelegate::IsContextMenuShowingForTesting() {
  if (auto* contents =
          views::AsViewClass<GlicSelectionContentsView>(GetContentsView())) {
    return contents->IsContextMenuShowingForTesting();
  }
  return false;
}

ui::SimpleMenuModel*
GlicSelectionWidgetDelegate::GetContextMenuModelForTesting() {
  if (auto* contents =
          views::AsViewClass<GlicSelectionContentsView>(GetContentsView())) {
    return contents->GetContextMenuModelForTesting();
  }
  return nullptr;
}

}  // namespace glic

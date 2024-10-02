// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/views/help_bubble_view_ash.h"

#include <initializer_list>
#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <utility>

#include "ash/bubble/bubble_utils.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "ash/user_education/user_education_help_bubble_controller.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/dot_indicator.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Minimum width of the bubble.
constexpr int kBubbleMinWidthDip = 200;
// Maximum width of the bubble. Longer strings will cause wrapping.
constexpr int kBubbleMaxWidthDip = 340;

// The insets from the bubble border to the text inside.
constexpr auto kBubbleContentsInsets = gfx::Insets::VH(16, 20);

// Corner radii for the help bubble. Note that when the help bubble is not
// center aligned with its anchor, the corner closest to the anchor has a
// smaller radius.
constexpr int kBubbleCornerRadiusDefault = 24;
constexpr int kBubbleCornerRadiusSmall = 2;

// Margins for the help bubble.
constexpr int kBubbleMargins = 8;

// Shadow elevation for the help bubble.
constexpr int kBubbleShadowElevation = 3;

// Translates from HelpBubbleArrow to the Views equivalent.
views::BubbleBorder::Arrow TranslateArrow(
    user_education::HelpBubbleArrow arrow) {
  switch (arrow) {
    case user_education::HelpBubbleArrow::kNone:
      return views::BubbleBorder::NONE;
    case user_education::HelpBubbleArrow::kTopLeft:
      return views::BubbleBorder::TOP_LEFT;
    case user_education::HelpBubbleArrow::kTopRight:
      return views::BubbleBorder::TOP_RIGHT;
    case user_education::HelpBubbleArrow::kBottomLeft:
      return views::BubbleBorder::BOTTOM_LEFT;
    case user_education::HelpBubbleArrow::kBottomRight:
      return views::BubbleBorder::BOTTOM_RIGHT;
    case user_education::HelpBubbleArrow::kLeftTop:
      return views::BubbleBorder::LEFT_TOP;
    case user_education::HelpBubbleArrow::kRightTop:
      return views::BubbleBorder::RIGHT_TOP;
    case user_education::HelpBubbleArrow::kLeftBottom:
      return views::BubbleBorder::LEFT_BOTTOM;
    case user_education::HelpBubbleArrow::kRightBottom:
      return views::BubbleBorder::RIGHT_BOTTOM;
    case user_education::HelpBubbleArrow::kTopCenter:
      return views::BubbleBorder::TOP_CENTER;
    case user_education::HelpBubbleArrow::kBottomCenter:
      return views::BubbleBorder::BOTTOM_CENTER;
    case user_education::HelpBubbleArrow::kLeftCenter:
      return views::BubbleBorder::LEFT_CENTER;
    case user_education::HelpBubbleArrow::kRightCenter:
      return views::BubbleBorder::RIGHT_CENTER;
  }
}

// Displays a simple "X" close button that will close a promo bubble view.
// The alt-text and button callback can be set based on the needs of the
// specific bubble.
class ClosePromoButton : public views::ImageButton {
  METADATA_HEADER(ClosePromoButton, views::ImageButton)

 public:
  ClosePromoButton(const std::u16string& accessible_name,
                   PressedCallback callback) {
    SetCallback(std::move(callback));
    views::ConfigureVectorImageButton(this);
    views::HighlightPathGenerator::Install(
        this,
        std::make_unique<views::CircleHighlightPathGenerator>(gfx::Insets()));
    GetViewAccessibility().SetName(accessible_name);
    SetTooltipText(accessible_name);

    constexpr int kIconSize = 16;
    SetImageModel(
        views::ImageButton::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            views::kIcCloseIcon, cros_tokens::kCrosSysOnSurface, kIconSize));

    constexpr float kCloseButtonFocusRingHaloThickness = 1.25f;
    views::FocusRing::Get(this)->SetHaloThickness(
        kCloseButtonFocusRingHaloThickness);
  }

  void OnThemeChanged() override {
    views::ImageButton::OnThemeChanged();
    StyleUtil::SetUpInkDropForButton(this);
    views::FocusRing::Get(this)->SetColorId(cros_tokens::kCrosSysOnSurface);
  }
};

BEGIN_METADATA(ClosePromoButton)
END_METADATA

class DotView : public views::View {
  METADATA_HEADER(DotView, views::View)

 public:
  DotView(gfx::Size size, bool should_fill)
      : size_(size), should_fill_(should_fill) {
    // In order to anti-alias properly, we'll grow by the stroke width and then
    // have the excess space be subtracted from the margins by the layout.
    SetProperty(views::kInternalPaddingKey, gfx::Insets(kStrokeWidth));
  }
  ~DotView() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    gfx::Size size = size_;
    const gfx::Insets* const insets = GetProperty(views::kInternalPaddingKey);
    size.Enlarge(insets->width(), insets->height());
    return size;
  }

  void OnPaint(gfx::Canvas* canvas) override {
    gfx::RectF local_bounds = gfx::RectF(GetLocalBounds());
    DCHECK_GT(local_bounds.width(), size_.width());
    DCHECK_GT(local_bounds.height(), size_.height());
    const gfx::PointF center_point = local_bounds.CenterPoint();
    const float radius = (size_.width() - kStrokeWidth) / 2.0f;

    const SkColor color =
        GetColorProvider()->GetColor(cros_tokens::kCrosSysOnSurface);
    if (should_fill_) {
      cc::PaintFlags fill_flags;
      fill_flags.setStyle(cc::PaintFlags::kFill_Style);
      fill_flags.setAntiAlias(true);
      fill_flags.setColor(color);
      canvas->DrawCircle(center_point, radius, fill_flags);
    }

    cc::PaintFlags stroke_flags;
    stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
    stroke_flags.setStrokeWidth(kStrokeWidth);
    stroke_flags.setAntiAlias(true);
    stroke_flags.setColor(color);
    canvas->DrawCircle(center_point, radius, stroke_flags);
  }

 private:
  static constexpr int kStrokeWidth = 1;

  const gfx::Size size_;
  const bool should_fill_;
};

constexpr int DotView::kStrokeWidth;

BEGIN_METADATA(DotView)
END_METADATA

// An `aura::WindowTargeter` that restricts located events to those within the
// area of the `gfx::Rect` given by `HelpBubbleViewAsh::GetHitRect()`.
class HelpBubbleWindowTargeter : public aura::WindowTargeter {
 public:
  explicit HelpBubbleWindowTargeter(HelpBubbleViewAsh* view)
      : view_tracker_(view) {}
  HelpBubbleWindowTargeter(const HelpBubbleWindowTargeter&) = delete;
  HelpBubbleWindowTargeter& operator=(const HelpBubbleWindowTargeter&) = delete;
  ~HelpBubbleWindowTargeter() override = default;

 private:
  // aura::WindowTargeter:
  std::unique_ptr<HitTestRects> GetExtraHitTestShapeRects(
      aura::Window* target) const override {
    if (!view_tracker_.view()) {
      return nullptr;
    }

    return std::make_unique<HitTestRects>(std::initializer_list<gfx::Rect>(
        {views::AsViewClass<const HelpBubbleViewAsh>(view_tracker_.view())
             ->GetHitRect()}));
  }

  const views::ViewTracker view_tracker_;
};

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(HelpBubbleViewAsh,
                                      kHelpBubbleElementIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(HelpBubbleViewAsh,
                                      kDefaultButtonIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(HelpBubbleViewAsh,
                                      kFirstNonDefaultButtonIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(HelpBubbleViewAsh, kBodyIconIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(HelpBubbleViewAsh, kBodyTextIdForTesting);

// Explicitly don't use the default DIALOG_SHADOW as it will show a black
// outline in dark mode on Mac. Use our own shadow instead. The shadow type is
// the same for all other platforms.
HelpBubbleViewAsh::HelpBubbleViewAsh(
    HelpBubbleId id,
    const internal::HelpBubbleAnchorParams& anchor,
    user_education::HelpBubbleParams params)
    : BubbleDialogDelegateView(anchor.view,
                               TranslateArrow(params.arrow),
                               views::BubbleBorder::STANDARD_SHADOW),
      id_(id) {
  SetCanActivate(true);

  // When hosted within a `views::ScrollView`, the anchor view may be
  // (partially) outside the viewport. Ensure that the anchor view is visible.
  CHECK(anchor.view);
  anchor.view->ScrollViewToVisible();

  UseCompactMargins();

  // Default timeout depends on whether non-close buttons are present.
  timeout_ = params.timeout.value_or(
      params.buttons.empty() ? user_education::kDefaultTimeoutWithoutButtons
                             : user_education::kDefaultTimeoutWithButtons);
  if (!timeout_.is_zero()) {
    timeout_callback_ = std::move(params.timeout_callback);
  }
  SetCancelCallback(std::move(params.dismiss_callback));

  // A body text provided from extended properties should take precedence
  // over the default body text provided from help bubble `params` since
  // extended properties are the ChromeOS-specific mechanism for overriding
  // platform agnostic behaviors.
  std::u16string body_text;
  if (auto body_text_from_extended_properties =
          user_education_util::GetHelpBubbleBodyText(
              params.extended_properties)) {
    body_text = base::UTF8ToUTF16(body_text_from_extended_properties.value());
  } else {
    body_text = params.body_text;
  }

  // An accessible name provided from extended properties should take precedence
  // over the default accessible name provided from help bubble `params` since
  // extended properties are the ChromeOS-specific mechanism for overriding
  // platform agnostic behaviors.
  if (auto accessible_name_from_extended_properties =
          user_education_util::GetHelpBubbleAccessibleName(
              params.extended_properties)) {
    accessible_name_ =
        base::UTF8ToUTF16(accessible_name_from_extended_properties.value());
  } else {
    accessible_name_ = params.title_text;
    if (!accessible_name_.empty()) {
      accessible_name_ += u". ";
    }
    accessible_name_ +=
        params.screenreader_text.empty() ? body_text : params.screenreader_text;
  }

  screenreader_hint_text_ = params.keyboard_navigation_hint;

  // Since we don't have any controls for the user to interact with (we're just
  // an information bubble), override our role to kAlert.
  SetAccessibleWindowRole(ax::mojom::Role::kAlert);

  // Layout structure:
  //
  // [***ooo      x]  <--- progress container
  // [@ TITLE     x]  <--- top text container
  //    body text
  // [    cancel ok]  <--- button container
  //
  // Notes:
  // - The close button's placement depends on the presence of a progress
  //   indicator.
  // - The body text takes the place of TITLE if there is no title.
  // - If there is both a title and icon, the body text is manually indented to
  //   align with the title; this avoids having to nest an additional vertical
  //   container.
  // - Unused containers are set to not be visible.
  views::View* const progress_container =
      AddChildView(std::make_unique<views::View>());
  views::View* const top_text_container =
      AddChildView(std::make_unique<views::View>());
  views::View* const button_container =
      AddChildView(std::make_unique<views::View>());

  // Add progress indicator (optional) and its container.
  if (params.progress) {
    DCHECK(params.progress->second);
    // TODO(crbug.com/40176811): surface progress information in a11y tree
    for (int i = 0; i < params.progress->second; ++i) {
      // TODO(crbug.com/40176811): formalize dot size
      progress_container->AddChildView(std::make_unique<DotView>(
          gfx::Size(8, 8), i < params.progress->first));
    }
  } else {
    progress_container->SetVisible(false);
  }

  // A body icon provided from extended properties should take precedence over a
  // body icon provided from help bubble `params` since extended properties are
  // the ChromeOS-specific mechanism for overriding platform agnostic behaviors.
  const gfx::VectorIcon* body_icon = params.body_icon;
  if (auto body_icon_from_extended_properties =
          user_education_util::GetHelpBubbleBodyIcon(
              params.extended_properties)) {
    body_icon = &body_icon_from_extended_properties->get();
  }

  // Add the body icon (optional).
  constexpr int kBodyIconSize = 20;
  constexpr int kBodyIconBackgroundSize = 24;
  if (body_icon && (body_icon != &gfx::kNoneIcon)) {
    icon_view_ = top_text_container->AddChildViewAt(
        views::Builder<views::ImageView>()
            .SetAccessibleName(params.body_icon_alt_text)
            .SetImage(ui::ImageModel::FromVectorIcon(
                *body_icon, cros_tokens::kCrosSysDialogContainer,
                kBodyIconSize))
            .SetPreferredSize(
                gfx::Size(kBodyIconBackgroundSize, kBodyIconBackgroundSize))
            .SetProperty(views::kElementIdentifierKey, kBodyIconIdForTesting)
            .Build(),
        0);
  }

  // Add title (optional) and body label.
  if (!params.title_text.empty()) {
    labels_.push_back(
        top_text_container->AddChildView(bubble_utils::CreateLabel(
            TypographyToken::kCrosBody1, params.title_text)));
    views::Label* label = AddChildViewAt(
        bubble_utils::CreateLabel(TypographyToken::kCrosBody1, body_text),
        GetIndexOf(button_container).value());
    labels_.push_back(label);
    label->SetProperty(views::kElementIdentifierKey, kBodyTextIdForTesting);
  } else {
    views::Label* label = top_text_container->AddChildView(
        bubble_utils::CreateLabel(TypographyToken::kCrosBody1, body_text));
    labels_.push_back(label);
    label->SetProperty(views::kElementIdentifierKey, kBodyTextIdForTesting);
  }

  // Set common label properties.
  for (views::Label* label : labels_) {
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetMultiLine(true);
    label->SetElideBehavior(gfx::NO_ELIDE);
  }

  // Add close button.
  std::u16string alt_text = params.close_button_alt_text;

  // This can be empty if a test doesn't set it. Set a reasonable default to
  // avoid an assertion (generated when a button with no text has no
  // accessible name).
  if (alt_text.empty()) {
    alt_text = l10n_util::GetStringUTF16(IDS_CLOSE);
  }

  // Since we set the cancel callback, we will use CancelDialog() to dismiss.
  close_button_ =
      (params.progress ? progress_container : top_text_container)
          ->AddChildView(std::make_unique<ClosePromoButton>(
              alt_text, base::BindRepeating(&DialogDelegate::CancelDialog,
                                            base::Unretained(this))));

  // Add other buttons.
  if (!params.buttons.empty()) {
    auto run_callback_and_close = [](HelpBubbleViewAsh* bubble_view,
                                     base::OnceClosure callback) {
      // We want to call the button callback before deleting the bubble in case
      // the caller needs to do something with it, but the callback itself
      // could close the bubble. Therefore, we need to ensure that the
      // underlying bubble view is not deleted before trying to close it.
      views::ViewTracker tracker(bubble_view);
      std::move(callback).Run();
      auto* const view = tracker.view();
      if (view && view->GetWidget() && !view->GetWidget()->IsClosed()) {
        view->GetWidget()->Close();
      }
    };

    // We will hold the default button to add later, since where we add it in
    // the sequence depends on platform style.
    std::unique_ptr<views::LabelButton> default_button;
    for (user_education::HelpBubbleButtonParams& button_params :
         params.buttons) {
      auto button = std::make_unique<PillButton>(
          base::BindOnce(run_callback_and_close, base::Unretained(this),
                         std::move(button_params.callback)),
          button_params.text,
          button_params.is_default ? PillButton::Type::kPrimaryWithoutIcon
                                   : PillButton::Type::kSecondaryWithoutIcon);
      button->SetMinSize(gfx::Size(0, 0));
      if (button_params.is_default) {
        DCHECK(!default_button);
        default_button = std::move(button);
        default_button->SetProperty(views::kElementIdentifierKey,
                                    kDefaultButtonIdForTesting);
      } else {
        non_default_buttons_.push_back(
            button_container->AddChildView(std::move(button)));
      }
    }

    if (!non_default_buttons_.empty()) {
      non_default_buttons_.front()->SetProperty(
          views::kElementIdentifierKey, kFirstNonDefaultButtonIdForTesting);
    }

    // Add the default button if there is one based on platform style.
    if (default_button) {
      if (views::PlatformStyle::kIsOkButtonLeading) {
        default_button_ =
            button_container->AddChildViewAt(std::move(default_button), 0);
      } else {
        default_button_ =
            button_container->AddChildView(std::move(default_button));
      }
    }
  } else {
    button_container->SetVisible(false);
  }

  // Set up layouts. This is the default vertical spacing that is also used to
  // separate progress indicators for symmetry.
  // TODO(dfried): consider whether we could take font ascender and descender
  // height and factor them into margin calculations.
  const views::LayoutProvider* layout_provider = views::LayoutProvider::Get();
  const int default_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);

  // Create primary layout (vertical).
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(kBubbleContentsInsets)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, 0, default_spacing, 0))
      .SetIgnoreDefaultMainAxisMargins(true);

  // Set up top row container layout.
  const int kCloseButtonHeight = 24;
  auto& progress_layout =
      progress_container
          ->SetLayoutManager(std::make_unique<views::FlexLayout>())
          ->SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
          .SetMinimumCrossAxisSize(kCloseButtonHeight)
          .SetDefault(views::kMarginsKey,
                      gfx::Insets::TLBR(0, default_spacing, 0, 0))
          .SetIgnoreDefaultMainAxisMargins(true);
  progress_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(progress_layout.GetDefaultFlexRule()));

  // Close button should float right in whatever container it's in.
  if (close_button_) {
    close_button_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                 views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kUnbounded)
            .WithAlignment(views::LayoutAlignment::kEnd));
    close_button_->SetProperty(views::kMarginsKey,
                               gfx::Insets::TLBR(0, default_spacing, 0, 0));
  }

  // Icon view should have padding between it and the title or body label.
  if (icon_view_) {
    icon_view_->SetProperty(views::kMarginsKey,
                            gfx::Insets::TLBR(0, 0, 0, default_spacing));
  }

  // Set label flex properties. This ensures that if the width of the bubble
  // maxes out the text will shrink on the cross-axis and grow to multiple
  // lines without getting cut off.
  const views::FlexSpecification text_flex(
      views::LayoutOrientation::kVertical,
      views::MinimumFlexSizeRule::kPreferred,
      views::MaximumFlexSizeRule::kPreferred,
      /* adjust_height_for_width = */ true,
      views::MinimumFlexSizeRule::kScaleToMinimum);

  for (views::Label* label : labels_) {
    label->SetProperty(views::kFlexBehaviorKey, text_flex);
  }

  auto& top_text_layout =
      top_text_container
          ->SetLayoutManager(std::make_unique<views::FlexLayout>())
          ->SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
          .SetIgnoreDefaultMainAxisMargins(true);
  top_text_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(top_text_layout.GetDefaultFlexRule()));

  // If the body icon is present, labels after the first are not parented to
  // the top text container, but still need to be inset to align with the
  // title.
  if (icon_view_) {
    const int indent = kBubbleContentsInsets.left() + kBodyIconBackgroundSize +
                       default_spacing;
    for (size_t i = 1; i < labels_.size(); ++i) {
      labels_[i]->SetProperty(views::kMarginsKey,
                              gfx::Insets::TLBR(0, indent, 0, 0));
    }
  }

  // Set up button container layout.
  // Add in the default spacing between bubble content and bottom/buttons.
  button_container->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          layout_provider->GetDistanceMetric(
              views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
          0, 0, 0));

  // Create button container internal layout.
  auto& button_layout =
      button_container->SetLayoutManager(std::make_unique<views::FlexLayout>())
          ->SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
          .SetDefault(
              views::kMarginsKey,
              gfx::Insets::TLBR(0,
                                layout_provider->GetDistanceMetric(
                                    views::DISTANCE_RELATED_BUTTON_HORIZONTAL),
                                0, 0))
          .SetIgnoreDefaultMainAxisMargins(true);

  // In a handful of (mostly South-Asian) languages, button text can exceed the
  // available width in the bubble if buttons are aligned horizontally. In those
  // cases - and only those cases - the bubble can switch to a vertical button
  // alignment.
  if (button_container->GetMinimumSize().width() >
      kBubbleMaxWidthDip - kBubbleContentsInsets.width()) {
    button_layout.SetOrientation(views::LayoutOrientation::kVertical)
        .SetCrossAxisAlignment(views::LayoutAlignment::kEnd)
        .SetDefault(views::kMarginsKey, gfx::Insets::VH(default_spacing, 0))
        .SetIgnoreDefaultMainAxisMargins(true);
  }

  button_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(button_layout.GetDefaultFlexRule()));

  // Want a consistent initial focused view if one is available.
  if (!button_container->children().empty()) {
    SetInitiallyFocusedView(button_container->children()[0]);
  } else if (close_button_) {
    SetInitiallyFocusedView(close_button_);
  }

  SetModalType(
      user_education_util::GetHelpBubbleModalType(params.extended_properties));
  SetProperty(views::kElementIdentifierKey, kHelpBubbleElementIdForTesting);
  set_margins(gfx::Insets());
  set_title_margins(gfx::Insets());
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_close_on_deactivate(false);
  set_focus_traversable_from_anchor_view(false);
  set_parent_window(
      anchor_widget()->GetNativeWindow()->GetRootWindow()->GetChildById(
          kShellWindowId_HelpBubbleContainer));

  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);

  // This gets reset to the platform default when we call `CreateBubble()`, so
  // we have to change it afterwards. Note that rounded corners are updated
  // *after* adjusting bounds since they are dependent on the help bubble's
  // position relative to its anchor.
  set_adjust_if_offscreen(true);
  SizeToContents();
  UpdateRoundedCorners();

  // Use a custom `aura::WindowTargeter` to avoid swallowing events from the
  // area outside the contents.
  // TODO(http://b/307780200): Possibly remove this after fixing the root issue
  // in the default `aura::WindowTargeter`.
  widget->GetNativeWindow()->SetEventTargeter(
      std::make_unique<HelpBubbleWindowTargeter>(this));

  if (widget->IsModal()) {
    // If the help bubble widget is a system modal widget, then it should be the
    // only interactive widget on the screen. Therefore, activate `widget`.
    widget->Show();
  } else {
    widget->ShowInactive();
  }

  auto* const anchor_bubble =
      anchor.view->GetWidget()->widget_delegate()->AsBubbleDialogDelegate();
  if (anchor_bubble) {
    anchor_pin_ = anchor_bubble->PreventCloseOnDeactivate();
  }
  MaybeStartAutoCloseTimer();

  // NOTE: `controller` may be `nullptr` in testing.
  if (auto* controller = UserEducationHelpBubbleController::Get()) {
    controller->NotifyHelpBubbleShown(base::PassKey<HelpBubbleViewAsh>(),
                                      /*help_bubble_view=*/this);
  }
}

HelpBubbleViewAsh::~HelpBubbleViewAsh() {
  // NOTE: `controller` may be `nullptr` in testing.
  if (auto* controller = UserEducationHelpBubbleController::Get()) {
    controller->NotifyHelpBubbleClosed(base::PassKey<HelpBubbleViewAsh>(),
                                       /*help_bubble_view=*/this);
  }
}

void HelpBubbleViewAsh::MaybeStartAutoCloseTimer() {
  if (timeout_.is_zero()) {
    return;
  }

  auto_close_timer_.Start(FROM_HERE, timeout_, this,
                          &HelpBubbleViewAsh::OnTimeout);
}

void HelpBubbleViewAsh::OnTimeout() {
  std::move(timeout_callback_).Run();
  GetWidget()->Close();
}

std::unique_ptr<views::NonClientFrameView>
HelpBubbleViewAsh::CreateNonClientFrameView(views::Widget* widget) {
  auto frame = BubbleDialogDelegateView::CreateNonClientFrameView(widget);
  auto* frame_ptr = static_cast<views::BubbleFrameView*>(frame.get());
  frame_ptr->bubble_border()->set_md_shadow_elevation(kBubbleShadowElevation);
  frame_ptr->set_use_anchor_window_bounds(false);
  return frame;
}

void HelpBubbleViewAsh::OnAnchorBoundsChanged() {
  views::BubbleDialogDelegateView::OnAnchorBoundsChanged();
  UpdateRoundedCorners();

  // NOTE: `controller` may be `nullptr` in testing.
  if (auto* controller = UserEducationHelpBubbleController::Get()) {
    controller->NotifyHelpBubbleAnchorBoundsChanged(
        base::PassKey<HelpBubbleViewAsh>(), /*help_bubble_view=*/this);
  }
}

std::u16string HelpBubbleViewAsh::GetAccessibleWindowTitle() const {
  std::u16string result = accessible_name_;

  // If there's a keyboard navigation hint, append it after a full stop.
  if (!screenreader_hint_text_.empty() && activate_count_ <= 1) {
    result += u". " + screenreader_hint_text_;
  }

  return result;
}

void HelpBubbleViewAsh::OnWidgetActivationChanged(views::Widget* widget,
                                                  bool active) {
  if (widget == GetWidget()) {
    if (active) {
      ++activate_count_;
      auto_close_timer_.AbandonAndStop();
    } else {
      MaybeStartAutoCloseTimer();
    }
  }
}

void HelpBubbleViewAsh::OnWidgetBoundsChanged(views::Widget* widget,
                                              const gfx::Rect& bounds) {
  views::BubbleDialogDelegateView::OnWidgetBoundsChanged(widget, bounds);
  UpdateRoundedCorners();
}

void HelpBubbleViewAsh::OnThemeChanged() {
  views::BubbleDialogDelegateView::OnThemeChanged();

  const auto* color_provider = GetColorProvider();
  const SkColor background_color =
      color_provider->GetColor(cros_tokens::kCrosSysDialogContainer);
  set_color(background_color);

  const SkColor foreground_color =
      color_provider->GetColor(cros_tokens::kCrosSysOnSurface);
  if (icon_view_) {
    icon_view_->SetBackground(views::CreateRoundedRectBackground(
        foreground_color, icon_view_->GetPreferredSize().height() / 2));
  }

  for (views::Label* label : labels_) {
    label->SetBackgroundColor(background_color);
    label->SetEnabledColor(foreground_color);
  }
}

gfx::Size HelpBubbleViewAsh::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const gfx::Size layout_manager_preferred_size =
      View::CalculatePreferredSize(available_size);

  // Wrap if the width is larger than |kBubbleMaxWidthDip|.
  if (layout_manager_preferred_size.width() > kBubbleMaxWidthDip) {
    return gfx::Size(kBubbleMaxWidthDip,
                     GetLayoutManager()->GetPreferredHeightForWidth(
                         this, kBubbleMaxWidthDip));
  }

  if (layout_manager_preferred_size.width() < kBubbleMinWidthDip) {
    return gfx::Size(kBubbleMinWidthDip,
                     layout_manager_preferred_size.height());
  }

  return layout_manager_preferred_size;
}

gfx::Rect HelpBubbleViewAsh::GetAnchorRect() const {
  // Update `anchor_rect` to respect margins.
  gfx::Rect anchor_rect = BubbleDialogDelegateView::GetAnchorRect();
  anchor_rect.Outset(kBubbleMargins);

  // Update `anchor_rect` so that the anchor view and help bubble view are
  // corner-aligned instead of edge-aligned, as would be the default.
  switch (GetBubbleFrameView()->bubble_border()->arrow()) {
    case views::BubbleBorder::LEFT_TOP:
    case views::BubbleBorder::TOP_LEFT:
      anchor_rect = gfx::Rect(anchor_rect.bottom_right(), gfx::Size());
      break;
    case views::BubbleBorder::RIGHT_TOP:
    case views::BubbleBorder::TOP_RIGHT:
      anchor_rect = gfx::Rect(anchor_rect.bottom_left(), gfx::Size());
      break;
    case views::BubbleBorder::BOTTOM_LEFT:
    case views::BubbleBorder::LEFT_BOTTOM:
      anchor_rect = gfx::Rect(anchor_rect.top_right(), gfx::Size());
      break;
    case views::BubbleBorder::BOTTOM_RIGHT:
    case views::BubbleBorder::RIGHT_BOTTOM:
      anchor_rect = gfx::Rect(anchor_rect.origin(), gfx::Size());
      break;
    case views::BubbleBorder::BOTTOM_CENTER:
    case views::BubbleBorder::LEFT_CENTER:
    case views::BubbleBorder::RIGHT_CENTER:
    case views::BubbleBorder::TOP_CENTER:
    case views::BubbleBorder::NONE:
    case views::BubbleBorder::FLOAT:
      break;
  }

  return anchor_rect;
}

void HelpBubbleViewAsh::GetWidgetHitTestMask(SkPath* mask) const {
  mask->addRect(gfx::RectToSkRect(GetHitRect()));
}

bool HelpBubbleViewAsh::WidgetHasHitTestMask() const {
  return true;
}

// static
bool HelpBubbleViewAsh::IsHelpBubble(views::DialogDelegate* dialog) {
  auto* const contents = dialog->GetContentsView();
  return contents && views::IsViewClass<HelpBubbleViewAsh>(contents);
}

bool HelpBubbleViewAsh::IsFocusInHelpBubble() const {
#if BUILDFLAG(IS_MAC)
  if (close_button_ && close_button_->HasFocus()) {
    return true;
  }
  if (default_button_ && default_button_->HasFocus()) {
    return true;
  }
  for (auto* button : non_default_buttons_) {
    if (button->HasFocus()) {
      return true;
    }
  }
  return false;
#else
  return GetWidget()->IsActive();
#endif
}

views::LabelButton* HelpBubbleViewAsh::GetDefaultButtonForTesting() const {
  return default_button_;
}

views::LabelButton* HelpBubbleViewAsh::GetNonDefaultButtonForTesting(
    int index) const {
  return non_default_buttons_[index];
}

gfx::Rect HelpBubbleViewAsh::GetHitRect() const {
  // NOTE: Mask to bubble frame view contents bounds to exclude shadows.
  return GetBubbleFrameView()->GetContentsBounds();
}

void HelpBubbleViewAsh::UpdateRoundedCorners() {
  if (!GetWidget()) {
    return;
  }

  // Alias constants to avoid line wrapping below.
  constexpr float kDefault = kBubbleCornerRadiusDefault;
  constexpr float kSmall = kBubbleCornerRadiusSmall;

  // Cache anchor and help bubble bounds in screen coordinates.
  const gfx::Rect anchor_rect = GetAnchorRect();
  const gfx::Point anchor_center = anchor_rect.CenterPoint();
  const gfx::Rect bounds_rect = GetBoundsInScreen();
  const gfx::Point bounds_center = bounds_rect.CenterPoint();

  // When the help bubble is not center aligned with its anchor, the corner
  // closest to the anchor has a smaller radius.
  const int dx = anchor_center.x() - bounds_center.x();
  const int dy = anchor_center.y() - bounds_center.y();
  const float upper_left = dx < 0 && dy < 0 ? kSmall : kDefault;
  const float upper_right = dx > 0 && dy < 0 ? kSmall : kDefault;
  const float lower_right = dx > 0 && dy > 0 ? kSmall : kDefault;
  const float lower_left = dx < 0 && dy > 0 ? kSmall : kDefault;

  // Update rounded corners.
  GetBubbleFrameView()->bubble_border()->set_rounded_corners(
      gfx::RoundedCornersF(upper_left, upper_right, lower_right, lower_left));
}

BEGIN_METADATA(HelpBubbleViewAsh)
END_METADATA

}  // namespace ash

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/ui/suggestion_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/input_method/ui/colors.h"
#include "chrome/browser/ash/input_method/ui/completion_suggestion_label_view.h"
#include "chrome/browser/ash/input_method/ui/suggestion_details.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ui {
namespace ime {

namespace {

const int kAnnotationLabelChildSpacing = 4;
const int kArrowIconSize = 14;
const int kDownIconHorizontalPadding = 2;
const int kDownIconSize = 16;
const int kEnterKeyHorizontalPadding = 2;

// Creates the index label, and returns it (never returns nullptr).
// The label text is not set in this function.
std::unique_ptr<views::Label> CreateIndexLabel() {
  auto index_label = std::make_unique<views::Label>();
  index_label->SetFontList(gfx::FontList({kFontStyle}, gfx::Font::NORMAL,
                                         kIndexFontSize,
                                         gfx::Font::Weight::MEDIUM));
  index_label->SetEnabledColor(
      ResolveSemanticColor(cros_styles::ColorName::kTextColorSecondary));
  index_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  index_label->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::VH(kPadding / 2, 0)));
  return index_label;
}

std::unique_ptr<views::ImageView> CreateDownIcon() {
  auto icon = std::make_unique<views::ImageView>();
  icon->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      0, kDownIconHorizontalPadding, 0, kDownIconHorizontalPadding)));
  return icon;
}

std::unique_ptr<views::Label> CreateEnterLabel() {
  auto label = std::make_unique<views::Label>();
  label->SetEnabledColor(
      ResolveSemanticColor(cros_styles::ColorName::kTextColorSecondary));
  label->SetText(l10n_util::GetStringUTF16(IDS_SUGGESTION_ENTER_KEY));
  label->SetFontList(gfx::FontList({kFontStyle}, gfx::Font::NORMAL,
                                   kAnnotationFontSize,
                                   gfx::Font::Weight::MEDIUM));
  label->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::VH(0, kEnterKeyHorizontalPadding)));
  return label;
}

std::unique_ptr<views::Label> CreateTabLabel() {
  auto label = std::make_unique<views::Label>();
  label->SetEnabledColor(
      ResolveSemanticColor(cros_styles::ColorName::kTextColorPrimary));
  label->SetText(l10n_util::GetStringUTF16(IDS_SUGGESTION_TAB_KEY));
  label->SetFontList(gfx::FontList({kFontStyle}, gfx::Font::NORMAL,
                                   kAnnotationFontSize,
                                   gfx::Font::Weight::MEDIUM));
  return label;
}

std::unique_ptr<views::View> CreateKeyContainer() {
  auto container = std::make_unique<views::View>();
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  // TODO(crbug/1099044): Use color from ash_color_provider and move SetBorder
  // to OnThemeChanged
  const SkColor kKeyContainerBorderColor =
      SkColorSetA(SK_ColorBLACK, 0x24);  // 14%
  container->SetBorder(views::CreateRoundedRectBorder(
      kAnnotationBorderThickness, kAnnotationCornerRadius, gfx::Insets(),
      kKeyContainerBorderColor));
  return container;
}

}  // namespace

SuggestionView::SuggestionView(PressedCallback callback)
    : views::Button(std::move(callback)) {
  index_label_ = AddChildView(CreateIndexLabel());
  index_label_->SetVisible(false);
  suggestion_label_ =
      AddChildView(std::make_unique<CompletionSuggestionLabelView>());
  suggestion_label_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::VH(kPadding / 2, 0)));

  annotation_container_ = AddChildView(CreateAnnotationContainer());
  down_and_enter_annotation_label_ =
      annotation_container_->AddChildView(CreateDownAndEnterAnnotationLabel());
  tab_annotation_label_ =
      annotation_container_->AddChildView(CreateTabAnnotationLabel());

  annotation_container_->SetVisible(false);
  down_and_enter_annotation_label_->SetVisible(false);
  tab_annotation_label_->SetVisible(false);

  SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  // TODO(crbug.com/1218186): Remove this, this is in place temporarily to be
  // able to submit accessibility checks, but this focusable View needs to
  // add a name so that the screen reader knows what to announce.
  SetProperty(views::kSkipAccessibilityPaintChecks, true);
}

SuggestionView::~SuggestionView() = default;

std::unique_ptr<views::View> SuggestionView::CreateAnnotationContainer() {
  auto label = std::make_unique<views::View>();
  label->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  // AnnotationLabel's ChildViews eat events simmilar to StyledLabel.
  // Explicitly sets can_process_events_within_subtree to false for
  // AnnotationLabel's hover to work correctly.
  label->SetCanProcessEventsWithinSubtree(false);
  return label;
}

std::unique_ptr<views::View>
SuggestionView::CreateDownAndEnterAnnotationLabel() {
  auto label = std::make_unique<views::View>();
  label->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, kAnnotationPaddingLeft, 0, 0)));
  label
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal))
      ->set_between_child_spacing(kAnnotationLabelChildSpacing);
  down_icon_ =
      label->AddChildView(CreateKeyContainer())->AddChildView(CreateDownIcon());
  arrow_icon_ = label->AddChildView(std::make_unique<views::ImageView>());
  label->AddChildView(CreateKeyContainer())->AddChildView(CreateEnterLabel());
  return label;
}

std::unique_ptr<views::View> SuggestionView::CreateTabAnnotationLabel() {
  auto label = std::make_unique<views::View>();
  label->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, kAnnotationPaddingLeft, 0, 0)));
  label->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  label->AddChildView(CreateTabLabel());
  return label;
}

void SuggestionView::SetView(const SuggestionDetails& details) {
  SetSuggestionText(details.text, details.confirmed_length);
  suggestion_width_ = suggestion_label_->GetPreferredSize().width();
  down_and_enter_annotation_label_->SetVisible(details.show_accept_annotation);
  tab_annotation_label_->SetVisible(details.show_quick_accept_annotation);
  annotation_container_->SetVisible(details.show_accept_annotation ||
                                    details.show_quick_accept_annotation);
}

void SuggestionView::SetViewWithIndex(const std::u16string& index,
                                      const std::u16string& text) {
  index_label_->SetText(index);
  index_label_->SetVisible(true);
  index_width_ = index_label_->GetPreferredSize().width();
  suggestion_label_->SetPrefixAndPrediction(u"", text);
  suggestion_width_ = suggestion_label_->GetPreferredSize().width();
}

void SuggestionView::SetSuggestionText(const std::u16string& text,
                                       const size_t confirmed_length) {
  suggestion_label_->SetPrefixAndPrediction(text.substr(0, confirmed_length),
                                            text.substr(confirmed_length));
}

void SuggestionView::SetHighlighted(bool highlighted) {
  if (highlighted_ == highlighted)
    return;

  highlighted_ = highlighted;
  if (highlighted) {
    NotifyAccessibilityEvent(ax::mojom::Event::kSelection, false);
    // TODO(crbug/1099044): Use System Color for button highlight.
    SetBackground(views::CreateSolidBackground(
        ResolveSemanticColor(kButtonHighlightColor)));
  } else {
    SetBackground(nullptr);
  }
  SchedulePaint();
}

void SuggestionView::OnThemeChanged() {
  const auto* color_provider = GetColorProvider();
  down_icon_->SetImage(
      gfx::CreateVectorIcon(kKeyboardArrowDownIcon, kDownIconSize,
                            color_provider->GetColor(ui::kColorIcon)));
  arrow_icon_->SetImage(
      gfx::CreateVectorIcon(kKeyboardArrowRightIcon, kArrowIconSize,
                            color_provider->GetColor(ui::kColorIcon)));
  views::View::OnThemeChanged();
}

void SuggestionView::Layout() {
  int left = kPadding;
  if (index_label_->GetVisible()) {
    index_label_->SetBounds(left, 0, index_width_, height());
    left += index_width_ + kPadding;
  }

  suggestion_label_->SetBounds(left, 0, suggestion_width_, height());

  if (annotation_container_->GetVisible()) {
    int annotation_left = left + suggestion_width_;
    int container_right = bounds().right();
    int annotation_width = container_right - annotation_left - kPadding;
    annotation_container_->SetBounds(annotation_left, kAnnotationPaddingTop,
                                     annotation_width,
                                     kAnnotationPaddingBottom);
  }
}

gfx::Size SuggestionView::CalculatePreferredSize() const {
  gfx::Size size;
  if (index_label_->GetVisible()) {
    size = index_label_->GetPreferredSize();
    size.SetToMax(gfx::Size(index_width_, 0));
    size.Enlarge(kPadding, 0);
  }
  gfx::Size suggestion_size = suggestion_label_->GetPreferredSize();
  suggestion_size.SetToMax(gfx::Size(suggestion_width_, 0));
  size.Enlarge(suggestion_size.width() + 2 * kPadding, 0);
  size.SetToMax(suggestion_size);
  if (annotation_container_->GetVisible()) {
    gfx::Size annotation_size = annotation_container_->GetPreferredSize();
    size.Enlarge(annotation_size.width(), 0);
  }
  if (min_width_ > size.width())
    size.Enlarge(min_width_ - size.width(), 0);
  return size;
}

void SuggestionView::SetMinWidth(int min_width) {
  min_width_ = min_width;
}

gfx::Point SuggestionView::GetAnchorOrigin() const {
  return gfx::Point(suggestion_label_->GetPrefixWidthPx() + kPadding, 0);
}

std::u16string SuggestionView::GetSuggestionForTesting() {
  return suggestion_label_->GetText();
}

CompletionSuggestionLabelView* SuggestionView::suggestion_label_for_testing()
    const {
  return suggestion_label_;
}

BEGIN_METADATA(SuggestionView, views::Button)
END_METADATA

}  // namespace ime
}  // namespace ui

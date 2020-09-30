// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ui/suggestion_view.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/input_method/ui/suggestion_details.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
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
  index_label->SetEnabledColor(kSuggestionColor);
  index_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  index_label->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kPadding / 2, 0)));

  return index_label;
}

// Creates the suggestion label, and returns it (never returns nullptr).
// The label text is not set in this function.
std::unique_ptr<views::StyledLabel> CreateSuggestionLabel() {
  auto suggestion_label = std::make_unique<views::StyledLabel>();
  suggestion_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  suggestion_label->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kPadding / 2, 0)));
  suggestion_label->SetAutoColorReadabilityEnabled(false);
  // StyledLabel eats event, probably because it has to handle links.
  // Explicitly sets can_process_events_within_subtree to false for
  // SuggestionView's hover to work correctly.
  suggestion_label->SetCanProcessEventsWithinSubtree(false);

  return suggestion_label;
}

std::unique_ptr<views::ImageView> CreateDownIcon() {
  auto icon = std::make_unique<views::ImageView>();
  icon->SetBorder(views::CreateEmptyBorder(gfx::Insets(
      0, kDownIconHorizontalPadding, 0, kDownIconHorizontalPadding)));
  return icon;
}

std::unique_ptr<views::Label> CreateEnterLabel() {
  auto label = std::make_unique<views::Label>();
  label->SetEnabledColor(kSuggestionColor);
  label->SetText(l10n_util::GetStringUTF16(IDS_SUGGESTION_ENTER_KEY));
  label->SetFontList(gfx::FontList({kFontStyle}, gfx::Font::NORMAL,
                                   kAnnotationFontSize,
                                   gfx::Font::Weight::MEDIUM));
  label->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(0, kEnterKeyHorizontalPadding)));
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

SuggestionView::SuggestionView(views::ButtonListener* listener)
    : views::Button(listener) {
  index_label_ = AddChildView(CreateIndexLabel());
  index_label_->SetVisible(false);
  suggestion_label_ = AddChildView(CreateSuggestionLabel());
  annotation_label_ = AddChildView(CreateAnnotationLabel());
  annotation_label_->SetVisible(false);
}

SuggestionView::~SuggestionView() = default;

std::unique_ptr<views::View> SuggestionView::CreateAnnotationLabel() {
  auto label = std::make_unique<views::View>();
  label->SetBorder(views::CreateEmptyBorder(gfx::Insets(0, kPadding, 0, 0)));
  label
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal))
      ->set_between_child_spacing(kAnnotationLabelChildSpacing);
  down_icon_ =
      label->AddChildView(CreateKeyContainer())->AddChildView(CreateDownIcon());
  arrow_icon_ = label->AddChildView(std::make_unique<views::ImageView>());
  label->AddChildView(CreateKeyContainer())->AddChildView(CreateEnterLabel());
  // AnnotationLabel's ChildViews eat events simmilar to StyledLabel.
  // Explicitly sets can_process_events_within_subtree to false for
  // AnnotationLabel's hover to work correctly.
  label->SetCanProcessEventsWithinSubtree(false);
  return label;
}

void SuggestionView::SetView(const SuggestionDetails& details) {
  SetSuggestionText(details.text, details.confirmed_length);
  suggestion_width_ = suggestion_label_->GetPreferredSize().width();
  annotation_label_->SetVisible(details.show_annotation);
}

void SuggestionView::SetViewWithIndex(const base::string16& index,
                                      const base::string16& text) {
  index_label_->SetText(index);
  index_label_->SetVisible(true);
  index_width_ = index_label_->GetPreferredSize().width();
  suggestion_label_->SetText(text);
  suggestion_width_ = suggestion_label_->GetPreferredSize().width();
}

void SuggestionView::SetSuggestionText(const base::string16& text,
                                       const size_t confirmed_length) {
  // SetText clears the existing style only if the text to set is different from
  // the previous one.
  suggestion_label_->SetText(base::EmptyString16());
  suggestion_label_->SetText(text);
  gfx::FontList kSuggestionFont({kFontStyle}, gfx::Font::NORMAL,
                                kSuggestionFontSize, gfx::Font::Weight::NORMAL);
  if (confirmed_length != 0) {
    views::StyledLabel::RangeStyleInfo confirmed_style;
    confirmed_style.custom_font = kSuggestionFont;
    confirmed_style.override_color = kConfirmedTextColor;
    suggestion_label_->AddStyleRange(gfx::Range(0, confirmed_length),
                                     confirmed_style);
  }

  views::StyledLabel::RangeStyleInfo suggestion_style;
  suggestion_style.custom_font = kSuggestionFont;
  suggestion_style.override_color = kSuggestionColor;
  suggestion_label_->AddStyleRange(gfx::Range(confirmed_length, text.length()),
                                   suggestion_style);

  // TODO(crbug/1099146): Add tests to check view's height and width with
  // confirmed length.
  // Maximum width for suggestion.
  suggestion_label_->SizeToFit(448);
}

void SuggestionView::SetHighlighted(bool highlighted) {
  if (highlighted_ == highlighted)
    return;

  highlighted_ = highlighted;
  if (highlighted) {
    NotifyAccessibilityEvent(ax::mojom::Event::kSelection, false);
    // TODO(crbug/1099044): Use System Color for button highlight.
    SetBackground(views::CreateSolidBackground(kButtonHighlightColor));
  } else {
    SetBackground(nullptr);
  }
  SchedulePaint();
}

void SuggestionView::OnThemeChanged() {
  down_icon_->SetImage(
      gfx::CreateVectorIcon(kKeyboardArrowDownIcon, kDownIconSize,
                            GetNativeTheme()->GetSystemColor(
                                ui::NativeTheme::kColorId_DefaultIconColor)));
  arrow_icon_->SetImage(
      gfx::CreateVectorIcon(kKeyboardArrowRightIcon, kArrowIconSize,
                            GetNativeTheme()->GetSystemColor(
                                ui::NativeTheme::kColorId_DefaultIconColor)));
  views::View::OnThemeChanged();
}

const char* SuggestionView::GetClassName() const {
  return "SuggestionView";
}

void SuggestionView::Layout() {
  int left = kPadding;
  if (index_label_->GetVisible()) {
    index_label_->SetBounds(left, 0, index_width_, height());
    left += index_width_ + kPadding;
  }

  suggestion_label_->SetBounds(left, 0, suggestion_width_, height());

  if (annotation_label_->GetVisible()) {
    int annotation_left = left + suggestion_width_ + kPadding;
    int right = bounds().right();
    annotation_label_->SetBounds(annotation_left, kAnnotationPaddingHeight,
                                 right - annotation_left - kPadding / 2, 16);
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
  if (annotation_label_->GetVisible()) {
    gfx::Size annotation_size = annotation_label_->GetPreferredSize();
    size.Enlarge(annotation_size.width() + kPadding, 0);
  }
  if (min_width_ > size.width())
    size.Enlarge(min_width_ - size.width(), 0);
  return size;
}

void SuggestionView::SetMinWidth(int min_width) {
  min_width_ = min_width;
}

}  // namespace ime
}  // namespace ui

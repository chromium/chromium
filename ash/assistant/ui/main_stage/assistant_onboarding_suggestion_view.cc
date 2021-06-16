// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_onboarding_suggestion_view.h"

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/util/resource_util.h"
#include "base/bind.h"
#include "base/cxx17_backports.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/libassistant/public/cpp/assistant_suggestion.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

using assistant::util::ResourceLinkType;

// Appearance.
constexpr int kCornerRadiusDip = 12;
constexpr int kIconSizeDip = 24;
constexpr int kLabelLineHeight = 20;
constexpr int kLabelSizeDelta = 2;
constexpr int kPreferredHeightDip = 72;

// Ink Drop.
constexpr float kInkDropVisibleOpacity = 0.06f;
constexpr float kInkDropHighlightOpacity = 0.08f;

// Helpers ---------------------------------------------------------------------

SkColor GetBackgroundColor(int index) {
  constexpr SkColor kBackgroundColors[] = {gfx::kGoogleBlue050,
                                           gfx::kGoogleRed050,
                                           gfx::kGoogleYellow050,
                                           gfx::kGoogleGreen050,
                                           SkColorSetRGB(0xF6, 0xE9, 0xF8),
                                           gfx::kGoogleBlue050};
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(base::size(kBackgroundColors)));
  return kBackgroundColors[index];
}

SkColor GetForegroundColor(int index) {
  constexpr SkColor kForegroundColors[] = {gfx::kGoogleBlue800,
                                           gfx::kGoogleRed800,
                                           SkColorSetRGB(0xBF, 0x50, 0x00),
                                           gfx::kGoogleGreen800,
                                           SkColorSetRGB(0x8A, 0x0E, 0x9E),
                                           gfx::kGoogleBlue800};
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(base::size(kForegroundColors)));
  return kForegroundColors[index];
}

}  // namespace

// AssistantOnboardingSuggestionView -------------------------------------------

AssistantOnboardingSuggestionView::AssistantOnboardingSuggestionView(
    AssistantViewDelegate* delegate,
    const chromeos::assistant::AssistantSuggestion& suggestion,
    int index)
    : views::Button(base::BindRepeating(
          &AssistantOnboardingSuggestionView::OnButtonPressed,
          base::Unretained(this))),
      delegate_(delegate),
      suggestion_id_(suggestion.id),
      index_(index) {
  InitLayout(suggestion);
}

AssistantOnboardingSuggestionView::~AssistantOnboardingSuggestionView() {
  // TODO(pbos): Revisit explicit removal of InkDrop for classes that override
  // Add/RemoveLayerBeneathView(). This is done so that the InkDrop doesn't
  // access the non-override versions in ~View.
  views::InkDrop::Remove(this);
}

int AssistantOnboardingSuggestionView::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void AssistantOnboardingSuggestionView::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

void AssistantOnboardingSuggestionView::AddLayerBeneathView(ui::Layer* layer) {
  // This routes background layers to `ink_drop_container_` instead of `this` to
  // avoid painting effects underneath our background.
  ink_drop_container_->AddLayerBeneathView(layer);
}

void AssistantOnboardingSuggestionView::RemoveLayerBeneathView(
    ui::Layer* layer) {
  // This routes background layers to `ink_drop_container_` instead of `this` to
  // avoid painting effects underneath our background.
  ink_drop_container_->RemoveLayerBeneathView(layer);
}

gfx::ImageSkia AssistantOnboardingSuggestionView::GetIcon() const {
  return icon_->GetImage();
}

const std::u16string& AssistantOnboardingSuggestionView::GetText() const {
  return label_->GetText();
}

void AssistantOnboardingSuggestionView::InitLayout(
    const chromeos::assistant::AssistantSuggestion& suggestion) {
  // A11y.
  SetAccessibleName(base::UTF8ToUTF16(suggestion.text));

  // Background.
  SetBackground(views::CreateRoundedRectBackground(GetBackgroundColor(index_),
                                                   kCornerRadiusDip));

  // Focus.
  SetFocusBehavior(FocusBehavior::ALWAYS);
  views::FocusRing::Get(this)->SetColor(gfx::kGoogleBlue300);

  // Ink Drop.
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  SetHasInkDropActionOnClick(true);
  views::InkDrop::Get(this)->SetBaseColor(GetForegroundColor(index_));
  views::InkDrop::Get(this)->SetVisibleOpacity(kInkDropVisibleOpacity);
  views::InkDrop::Get(this)->SetHighlightOpacity(kInkDropHighlightOpacity);

  // Installing this highlight path generator will set the desired shape for
  // both ink drop effects as well as our focus ring.
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kCornerRadiusDip);

  // This is used as a parent for ink-drop effects to prevent painting them
  // below the background for `this`.
  ink_drop_container_ =
      AddChildView(std::make_unique<views::InkDropContainerView>());

  // Layout.
  auto& layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>())
          ->SetCollapseMargins(true)
          .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
          .SetDefault(views::kFlexBehaviorKey, views::FlexSpecification())
          .SetDefault(views::kMarginsKey, gfx::Insets(0, 2 * kSpacingDip))
          .SetInteriorMargin(gfx::Insets(0, 2 * kMarginDip))
          .SetOrientation(views::LayoutOrientation::kHorizontal);

  // NOTE: Our |layout| ignores the view for drawing focus as it is a special
  // view which lays out itself. Removing this would cause it *not* to paint.
  layout.SetChildViewIgnoredByLayout(views::FocusRing::Get(this), true);

  // NOTE: Our |ink_drop_container_| serves only to hold reference to ink drop
  // layers for painting purposes. It can be completely ignored by our |layout|.
  layout.SetChildViewIgnoredByLayout(ink_drop_container_, true);

  // Icon.
  icon_ = AddChildView(std::make_unique<views::ImageView>());
  icon_->SetImageSize({kIconSizeDip, kIconSizeDip});
  icon_->SetPreferredSize({kIconSizeDip, kIconSizeDip});

  const GURL& url = suggestion.icon_url;
  if (assistant::util::IsResourceLinkType(url, ResourceLinkType::kIcon)) {
    // Handle local images.
    icon_->SetImage(assistant::util::CreateVectorIcon(
        assistant::util::AppendOrReplaceColorParam(url,
                                                   GetForegroundColor(index_)),
        kIconSizeDip));
  } else if (url.is_valid()) {
    // Handle remote images.
    delegate_->DownloadImage(
        url, base::BindOnce(&AssistantOnboardingSuggestionView::UpdateIcon,
                            weak_factory_.GetWeakPtr()));
  }

  // Label.
  label_ = AddChildView(std::make_unique<views::Label>());
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetEnabledColor(GetForegroundColor(index_));
  label_->SetFontList(assistant::ui::GetDefaultFontList()
                          .DeriveWithSizeDelta(kLabelSizeDelta)
                          .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label_->SetLineHeight(kLabelLineHeight);
  label_->SetMaxLines(2);
  label_->SetMultiLine(true);
  label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  label_->SetText(base::UTF8ToUTF16(suggestion.text));

  // Workaround issue where multiline label is not allocated enough height.
  label_->SetPreferredSize(
      gfx::Size(label_->GetPreferredSize().width(), 2 * kLabelLineHeight));
}

void AssistantOnboardingSuggestionView::UpdateIcon(const gfx::ImageSkia& icon) {
  if (!icon.isNull())
    icon_->SetImage(icon);
}

void AssistantOnboardingSuggestionView::OnButtonPressed() {
  delegate_->OnSuggestionPressed(suggestion_id_);
}

BEGIN_METADATA(AssistantOnboardingSuggestionView, views::Button)
END_METADATA

}  // namespace ash

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/assistant/ui/main_stage/assistant_onboarding_suggestion_view.h"

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/util/resource_util.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_suggestion.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/accessibility/view_accessibility.h"
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

struct ColorPalette {
  SkColor flag_off;
  SkColor dark;
  SkColor light;
};

SkColor GetBackgroundColor(int index) {
  // Opacity values:
  // 0x19: 10%
  // 0x4c: 30%
  constexpr ColorPalette kBackgroundColors[] = {
      {gfx::kGoogleBlue050, SkColorSetA(gfx::kGoogleBlue300, 0x4c),
       SkColorSetA(gfx::kGoogleBlue600, 0x19)},
      {gfx::kGoogleRed050, SkColorSetA(gfx::kGoogleRed300, 0x4c),
       SkColorSetA(gfx::kGoogleRed600, 0x19)},
      {gfx::kGoogleYellow050, SkColorSetA(gfx::kGoogleYellow300, 0x4c),
       SkColorSetA(gfx::kGoogleYellow600, 0x19)},
      {gfx::kGoogleGreen050, SkColorSetA(gfx::kGoogleGreen300, 0x4c),
       SkColorSetA(gfx::kGoogleGreen600, 0x19)},
      {SkColorSetRGB(0xF6, 0xE9, 0xF8), SkColorSetARGB(0x4c, 0xf8, 0x82, 0xff),
       SkColorSetARGB(0x19, 0xc6, 0x1a, 0xd9)},
      {gfx::kGoogleBlue050, SkColorSetA(gfx::kGoogleBlue300, 0x4c),
       SkColorSetA(gfx::kGoogleBlue600, 0x19)}};

  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(std::size(kBackgroundColors)));

  return DarkLightModeControllerImpl::Get()->IsDarkModeEnabled()
             ? kBackgroundColors[index].dark
             : kBackgroundColors[index].light;
}

SkColor GetForegroundColor(int index) {
  constexpr ColorPalette kForegroundColors[] = {
      {gfx::kGoogleBlue800, gfx::kGoogleBlue200, gfx::kGoogleBlue800},
      {gfx::kGoogleRed800, gfx::kGoogleRed200, gfx::kGoogleRed800},
      {SkColorSetRGB(0xBF, 0x50, 0x00), gfx::kGoogleYellow200,
       SkColorSetRGB(0xBF, 0x50, 0x00)},
      {gfx::kGoogleGreen800, gfx::kGoogleGreen200, gfx::kGoogleGreen800},
      {SkColorSetRGB(0x8A, 0x0E, 0x9E), SkColorSetRGB(0xf8, 0x82, 0xff),
       SkColorSetRGB(0xaa, 0x00, 0xb8)},
      {gfx::kGoogleBlue800, gfx::kGoogleBlue200, gfx::kGoogleBlue800}};

  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(std::size(kForegroundColors)));

  return DarkLightModeControllerImpl::Get()->IsDarkModeEnabled()
             ? kForegroundColors[index].dark
             : kForegroundColors[index].light;
}

}  // namespace

// AssistantOnboardingSuggestionView -------------------------------------------

AssistantOnboardingSuggestionView::AssistantOnboardingSuggestionView(
    AssistantViewDelegate* delegate,
    const assistant::AssistantSuggestion& suggestion,
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
  // AddLayerToRegion/RemoveLayerFromRegions(). This is done so that the InkDrop
  // doesn't access the non-override versions in ~View.
  views::InkDrop::Remove(this);
}

gfx::Size AssistantOnboardingSuggestionView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int preferred_width =
      views::Button::CalculatePreferredSize(available_size).width();
  return gfx::Size(preferred_width, kPreferredHeightDip);
}

void AssistantOnboardingSuggestionView::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

void AssistantOnboardingSuggestionView::AddLayerToRegion(
    ui::Layer* layer,
    views::LayerRegion region) {
  // This routes background layers to `ink_drop_container_` instead of `this` to
  // avoid painting effects underneath our background.
  ink_drop_container_->AddLayerToRegion(layer, region);
}

void AssistantOnboardingSuggestionView::RemoveLayerFromRegions(
    ui::Layer* layer) {
  // This routes background layers to `ink_drop_container_` instead of `this` to
  // avoid painting effects underneath our background.
  ink_drop_container_->RemoveLayerFromRegions(layer);
}

void AssistantOnboardingSuggestionView::OnThemeChanged() {
  views::View::OnThemeChanged();

  GetBackground()->SetNativeControlColor(GetBackgroundColor(index_));

  // SetNativeControlColor does not trigger a repaint.
  SchedulePaint();

  label_->SetEnabledColor(GetForegroundColor(index_));

  if (assistant::util::IsResourceLinkType(url_, ResourceLinkType::kIcon)) {
    icon_->SetImage(assistant::util::CreateVectorIcon(
        assistant::util::AppendOrReplaceColorParam(url_,
                                                   GetForegroundColor(index_)),
        kIconSizeDip));
  }
}

gfx::ImageSkia AssistantOnboardingSuggestionView::GetIcon() const {
  return icon_->GetImage();
}

const std::u16string& AssistantOnboardingSuggestionView::GetText() const {
  return label_->GetText();
}

void AssistantOnboardingSuggestionView::InitLayout(
    const assistant::AssistantSuggestion& suggestion) {
  // A11y.
  GetViewAccessibility().SetName(base::UTF8ToUTF16(suggestion.text));

  // Background.
  SetBackground(views::CreateRoundedRectBackground(GetBackgroundColor(index_),
                                                   kCornerRadiusDip));

  // Focus.
  SetFocusBehavior(FocusBehavior::ALWAYS);
  views::FocusRing::Get(this)->SetColorId(ui::kColorAshOnboardingFocusRing);

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
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetCollapseMargins(true)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetDefault(views::kFlexBehaviorKey, views::FlexSpecification())
      .SetDefault(views::kMarginsKey, gfx::Insets::VH(0, 2 * kSpacingDip))
      .SetInteriorMargin(gfx::Insets::VH(0, 2 * kMarginDip))
      .SetOrientation(views::LayoutOrientation::kHorizontal);

  // Ignore the focus ring, which lays out itself.
  views::FocusRing::Get(this)->SetProperty(views::kViewIgnoredByLayoutKey,
                                           true);

  // Ignore the `ink_drop_container_`, which serves only to hold reference to
  // ink drop layers for painting purposes.
  ink_drop_container_->SetProperty(views::kViewIgnoredByLayoutKey, true);

  // Icon.
  icon_ = AddChildView(std::make_unique<views::ImageView>());
  icon_->SetImageSize({kIconSizeDip, kIconSizeDip});
  icon_->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));

  url_ = suggestion.icon_url;
  if (!assistant::util::IsResourceLinkType(url_, ResourceLinkType::kIcon) &&
      url_.is_valid()) {
    // Handle remote images. Local resource link type image will be handled in
    // AssistantOnboardingSuggestionView::OnThemeChanged.
    delegate_->DownloadImage(
        url_, base::BindOnce(&AssistantOnboardingSuggestionView::UpdateIcon,
                             weak_factory_.GetWeakPtr()));
  }

  // Label.
  label_ = AddChildView(std::make_unique<views::Label>());
  label_->SetID(kAssistantOnboardingSuggestionViewLabel);
  label_->SetAutoColorReadabilityEnabled(false);
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

BEGIN_METADATA(AssistantOnboardingSuggestionView)
END_METADATA

}  // namespace ash

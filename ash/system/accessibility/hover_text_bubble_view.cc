// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/hover_text_bubble_view.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "base/strings/string_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr double kBaseFontSizePx = 16.0;
constexpr int kMinFontSizePx = 16;
constexpr int kMaxFontSizePx = 64;
constexpr int kMaxBubbleWidthDip = 480;
constexpr size_t kMaxBubbleLines = 4;
constexpr int kVerticalMarginDip = 12;
constexpr int kHorizontalMarginDip = 16;
constexpr int kCornerRadiusDip = 16;

constexpr std::string_view kRobotoFont = "Roboto";
constexpr std::string_view kDejaVuSansFont = "DejaVu Sans";
constexpr std::string_view kSansSerifFamily = "sans-serif";
constexpr std::string_view kNotoSerifFont = "Noto Serif";
constexpr std::string_view kDejaVuSerifFont = "DejaVu Serif";
constexpr std::string_view kSerifFamily = "serif";
constexpr std::string_view kRobotoMonoFont = "Roboto Mono";
constexpr std::string_view kDejaVuSansMonoFont = "DejaVu Sans Mono";
constexpr std::string_view kMonospaceFamily = "monospace";

// Returns the prioritized compile-time font family fallback list for `style`.
// The primary ChromeOS font (`Roboto`, `Noto Serif`, `Roboto Mono`) is listed
// first, followed by standard Linux/Fontconfig fallbacks (`DejaVu Sans`,
// `DejaVu Serif`, `DejaVu Sans Mono`, and generic family aliases) so each style
// renders in a visibly distinct typeface on both ChromeOS devices and
// Linux-CrOS workstations.
constexpr std::array<std::string_view, 3> GetCandidateFamiliesForStyle(
    HoverTextFontStyle style) {
  switch (style) {
    case HoverTextFontStyle::kSystemSans:
      return {kRobotoFont, kDejaVuSansFont, kSansSerifFamily};
    case HoverTextFontStyle::kSerif:
      return {kNotoSerifFont, kDejaVuSerifFont, kSerifFamily};
    case HoverTextFontStyle::kMonospace:
      return {kRobotoMonoFont, kDejaVuSansMonoFont, kMonospaceFamily};
  }
}

}  // namespace

HoverTextBubbleView::HoverTextBubbleView()
    : views::BubbleDialogDelegate(/*anchor_view=*/nullptr,
                                  views::BubbleBorder::TOP_LEFT) {
  resolved_candidate_index_by_style_.fill(-1);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetCanActivate(false);
  set_accept_events(false);
  set_close_on_deactivate(false);
  SetFocusBehavior(views::View::FocusBehavior::NEVER);
  SetCanProcessEventsWithinSubtree(false);
  SetBackgroundColor(cros_tokens::kCrosSysSystemBaseElevated);
  set_corner_radius(kCornerRadiusDip);
  set_margins(gfx::Insets());
  set_highlight_button_when_shown(false);
  set_parent_window(
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          kShellWindowId_AccessibilityBubbleContainer));

  // Override `BubbleDialogDelegate::GetAccessibleWindowRole()` (which defaults
  // to `ax::mojom::Role::kAlertDialog` when `GetCanActivate()` is false) and
  // expose zero accessibility properties so `ShowInactive()` never sets an
  // alert role on the widget root view or emits `ax::mojom::Event::kAlert`, and
  // assistive technologies (ChromeVox, Select-to-Speak, Switch Access, and
  // Hover Text itself) never target, focus, or recursively read the bubble.
  SetAccessibleWindowRole(ax::mojom::Role::kNone);
  GetViewAccessibility().SetIsIgnored(true);
  GetViewAccessibility().SetIsLeaf(true);
}

HoverTextBubbleView::~HoverTextBubbleView() {
  SetLayoutManager(nullptr);
  SetAnchorView(nullptr);
}

void HoverTextBubbleView::Update(const std::u16string& text,
                                 double font_scale,
                                 HoverTextFontStyle font_style) {
  const int font_size_px =
      std::clamp(static_cast<int>(std::round(kBaseFontSizePx * font_scale)),
                 kMinFontSizePx, kMaxFontSizePx);
  if (font_size_px != last_font_size_px_ || font_style != last_font_style_) {
    const auto candidate_families = GetCandidateFamiliesForStyle(font_style);
    const size_t style_index = std::to_underlying(font_style);

    // `gfx::FontList` shapes all Latin glyphs with `fonts_[0]`, and
    // `PlatformFontSkia` falls back an uninstalled family name to `"sans"`.
    // Probe and cache the first candidate that resolves to its own typeface
    // once per `HoverTextFontStyle` so `kSerif` and `kMonospace` render in
    // genuine serif/monospace typefaces without constructing probe fonts on
    // every hover.
    int promoted_index = resolved_candidate_index_by_style_[style_index];
    if (promoted_index < 0) {
      promoted_index = 0;
      for (size_t i = 0; i < candidate_families.size(); ++i) {
        const std::string candidate(candidate_families[i]);
        const gfx::Font candidate_font(candidate, font_size_px);
        if (base::EqualsCaseInsensitiveASCII(candidate_font.GetFontName(),
                                             candidate) &&
            (base::EqualsCaseInsensitiveASCII(
                 candidate_font.GetActualFontName(), candidate) ||
             i + 1 == candidate_families.size())) {
          promoted_index = static_cast<int>(i);
          break;
        }
      }
      resolved_candidate_index_by_style_[style_index] = promoted_index;
    }

    std::vector<std::string> resolved_families;
    resolved_families.reserve(candidate_families.size());
    resolved_families.emplace_back(
        candidate_families[static_cast<size_t>(promoted_index)]);
    for (size_t i = 0; i < candidate_families.size(); ++i) {
      if (static_cast<int>(i) != promoted_index) {
        resolved_families.emplace_back(candidate_families[i]);
      }
    }
    label_->SetFontList(gfx::FontList(resolved_families, gfx::Font::NORMAL,
                                      font_size_px, gfx::Font::Weight::MEDIUM));
    last_font_size_px_ = font_size_px;
    last_font_style_ = font_style;
  }

  // Measure single-line preferred width first so short labels shrink tightly to
  // their content and only wrap when exceeding `kMaxBubbleWidthDip`.
  label_->SetMultiLine(false);
  label_->SetMaximumWidthSingleLine(0);
  label_->SetText(text);

  if (label_->GetPreferredSize().width() > kMaxBubbleWidthDip) {
    label_->SetMultiLine(true);
    label_->SetMaximumWidth(kMaxBubbleWidthDip);
    label_->SetMaxLines(kMaxBubbleLines);
    label_->SetElideBehavior(gfx::ELIDE_TAIL);
  }
}

views::View* HoverTextBubbleView::GetContentsView() {
  return this;
}

views::Widget* HoverTextBubbleView::GetWidget() {
  return View::GetWidget();
}

const views::Widget* HoverTextBubbleView::GetWidget() const {
  return View::GetWidget();
}

void HoverTextBubbleView::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kVerticalMarginDip, kHorizontalMarginDip)));

  label_ = AddChildView(views::Builder<views::Label>()
                            .SetText(std::u16string())
                            .SetEnabledColor(cros_tokens::kCrosSysOnSurface)
                            .SetAutoColorReadabilityEnabled(false)
                            .SetHorizontalAlignment(gfx::ALIGN_TO_HEAD)
                            .SetElideBehavior(gfx::ELIDE_TAIL)
                            .SetSelectable(false)
                            .SetFocusBehavior(views::View::FocusBehavior::NEVER)
                            .Build());

  // Ignore the internal label in the accessibility tree and set a non-matching
  // cached accessible name so `views::Label::SetText()` does not invoke
  // `ViewAccessibility::SetName()` or emit `ax::mojom::Event::kTextChanged` on
  // rapid hover updates, and assistive technologies never target, focus, or
  // read the bubble's own text node.
  label_->GetViewAccessibility().SetName(u"\xfffc",
                                         ax::mojom::NameFrom::kAttribute);
  label_->GetViewAccessibility().SetIsIgnored(true);
  label_->GetViewAccessibility().SetIsLeaf(true);
}

std::u16string HoverTextBubbleView::GetTextForTesting() const {
  return std::u16string(label_->GetText());
}

const gfx::FontList& HoverTextBubbleView::GetFontListForTesting() const {
  return label_->font_list();
}

int HoverTextBubbleView::GetFontSizePixelsForTesting() const {
  return label_->font_list().GetFontSize();
}

std::string HoverTextBubbleView::GetFontFamilyForTesting() const {
  return label_->font_list().GetPrimaryFont().GetFontName();
}

BEGIN_METADATA(HoverTextBubbleView)
END_METADATA

}  // namespace ash

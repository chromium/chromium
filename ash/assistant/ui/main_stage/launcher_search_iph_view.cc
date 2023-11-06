// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/launcher_search_iph_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/assistant/ui/main_stage/chip_view.h"
#include "ash/public/cpp/app_list/app_list_client.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/rand_util.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "url/gurl.h"

namespace ash {
namespace {
constexpr int kMainLayoutBetweenChildSpacing = 16;
constexpr int kActionContainerBetweenChildSpacing = 8;

constexpr int kNumberOfQueryChipsWithAssistantChip = 3;
constexpr int kNumberOfQueryChipsWithoutAssistantChip = 4;

constexpr char16_t kTitleTextPlaceholder[] = u"Title text";
constexpr char16_t kDescriptionTextPlaceholder[] = u"Description text";

constexpr char16_t kChipWeatherQueryPlaceholder[] = u"Weather";
constexpr char16_t kChipShortcutQueryPlaceholder[] = u"Screen Saver";
constexpr char16_t kChipCalculationQueryPlaceholder[] = u"50+96/5";
constexpr char16_t kChipUnitConversionQueryPlaceholder[] = u"1 inch in cm";
constexpr char16_t kChipTranslationQueryPlaceholder[] = u"Hello in Spanish";
constexpr char16_t kChipDefinitionQueryPlaceholder[] = u"impromptu definition";
constexpr char16_t kChipStockQueryPlaceholder[] = u"S&P 500";

constexpr char16_t kAssistantButtonPlaceholder[] = u"Go to Assistant";

constexpr gfx::RoundedCornersF kBackgroundRadiiClamshellLTR = {16, 4, 16, 16};

constexpr gfx::RoundedCornersF kBackgroundRadiiClamshellRTL = {4, 16, 16, 16};

// There are 4px margins for the top and the bottom (and for the left in LTR
// Clamshell mode) provided by SearchBoxViewBase's root level container, i.e.
// left=10px in `kOuterBackgroundInsetsClamshell` means 14px in prod.
constexpr gfx::Insets kOuterBackgroundInsetsClamshell =
    gfx::Insets::TLBR(0, 10, 17, 10);
constexpr gfx::Insets kOuterBackgroundInsetsTablet =
    gfx::Insets::TLBR(10, 16, 12, 16);

constexpr gfx::Insets kInnerBackgroundInsetsClamshell = gfx::Insets::VH(20, 24);
constexpr gfx::Insets kInnerBackgroundInsetsTablet = gfx::Insets::VH(16, 16);

constexpr int kBackgroundRadiusTablet = 16;

std::vector<std::u16string> GetQueryChips(int num_of_chips) {
  std::vector<std::u16string> chips = {
      kChipWeatherQueryPlaceholder,     kChipShortcutQueryPlaceholder,
      kChipCalculationQueryPlaceholder, kChipUnitConversionQueryPlaceholder,
      kChipTranslationQueryPlaceholder, kChipDefinitionQueryPlaceholder,
      kChipStockQueryPlaceholder};
  CHECK_GE(static_cast<int>(chips.size()), num_of_chips);
  base::RandomShuffle(chips.begin(), chips.end());
  chips.resize(num_of_chips);
  return chips;
}

}  // namespace

LauncherSearchIphView::LauncherSearchIphView(
    Delegate* delegate,
    bool is_in_tablet_mode,
    std::unique_ptr<ScopedIphSession> scoped_iph_session,
    bool show_assistant_chip)
    : delegate_(delegate),
      scoped_iph_session_(std::move(scoped_iph_session)),
      show_assistant_chip_(show_assistant_chip) {
  SetID(ViewId::kSelf);

  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Add a root `box_layout_view` as we can set margins (i.e. borders) outside
  // the background.
  views::BoxLayoutView* box_layout_view =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  box_layout_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  box_layout_view->SetInsideBorderInsets(is_in_tablet_mode
                                             ? kInnerBackgroundInsetsTablet
                                             : kInnerBackgroundInsetsClamshell);
  box_layout_view->SetBetweenChildSpacing(kMainLayoutBetweenChildSpacing);
  // Use `kStretch` for `actions_container` to get stretched.
  box_layout_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  SetBorder(views::CreateEmptyBorder(is_in_tablet_mode
                                         ? kOuterBackgroundInsetsTablet
                                         : kOuterBackgroundInsetsClamshell));

  // Add texts into a container to avoid stretching `views::Label`s.
  views::BoxLayoutView* text_container =
      box_layout_view->AddChildView(std::make_unique<views::BoxLayoutView>());
  text_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  text_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  text_container->SetBetweenChildSpacing(kMainLayoutBetweenChildSpacing);

  views::Label* title_label = text_container->AddChildView(
      std::make_unique<views::Label>(kTitleTextPlaceholder));
  title_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  title_label->SetEnabledColorId(kColorAshTextColorPrimary);

  views::Label* description_label = text_container->AddChildView(
      std::make_unique<views::Label>(kDescriptionTextPlaceholder));
  description_label->SetEnabledColorId(kColorAshTextColorPrimary);

  const TypographyProvider* typography_provider = TypographyProvider::Get();
  DCHECK(typography_provider) << "TypographyProvider must not be null";
  if (typography_provider) {
    typography_provider->StyleLabel(TypographyToken::kCrosTitle1, *title_label);
    typography_provider->StyleLabel(TypographyToken::kCrosBody2,
                                    *description_label);
  }

  views::BoxLayoutView* actions_container =
      box_layout_view->AddChildView(std::make_unique<views::BoxLayoutView>());
  actions_container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  actions_container->SetBetweenChildSpacing(
      kActionContainerBetweenChildSpacing);

  // In Launcher zero state, we show 3 query chips + Assistant chip.
  // In Assistant zero state, we show 4 query chips.
  int num_of_chips = show_assistant_chip_
                         ? kNumberOfQueryChipsWithAssistantChip
                         : kNumberOfQueryChipsWithoutAssistantChip;
  int query_chip_view_id = ViewId::kChipStart;
  for (const std::u16string& query : GetQueryChips(num_of_chips)) {
    ChipView* chip = actions_container->AddChildView(
        std::make_unique<ChipView>(ChipView::Type::kLarge));
    chip->SetText(query);
    chip->SetCallback(
        base::BindRepeating(&LauncherSearchIphView::RunLauncherSearchQuery,
                            weak_ptr_factory_.GetWeakPtr(), query));
    chip->SetID(query_chip_view_id);
    query_chip_view_id++;
  }

  if (show_assistant_chip_) {
    views::View* spacer =
        actions_container->AddChildView(std::make_unique<views::View>());
    actions_container->SetFlexForView(spacer, 1);

    ash::PillButton* assistant_button =
        actions_container->AddChildView(std::make_unique<ash::PillButton>(
            base::BindRepeating(&LauncherSearchIphView::OpenAssistantPage,
                                weak_ptr_factory_.GetWeakPtr()),
            kAssistantButtonPlaceholder));
    assistant_button->SetID(ViewId::kAssistant);
    assistant_button->SetPillButtonType(
        PillButton::Type::kDefaultLargeWithoutIcon);
  }

  if (is_in_tablet_mode) {
    box_layout_view->SetBackground(views::CreateThemedRoundedRectBackground(
        kColorAshControlBackgroundColorInactive, kBackgroundRadiusTablet));
  } else {
    box_layout_view->SetBackground(views::CreateThemedRoundedRectBackground(
        kColorAshControlBackgroundColorInactive,
        base::i18n::IsRTL() ? kBackgroundRadiiClamshellRTL
                            : kBackgroundRadiiClamshellLTR,
        /*for_border_thickness=*/0));
  }
}

LauncherSearchIphView::~LauncherSearchIphView() = default;

void LauncherSearchIphView::RunLauncherSearchQuery(
    const std::u16string& query) {
  if (scoped_iph_session_) {
    scoped_iph_session_->NotifyEvent(kIphEventNameChipClick);
  }
  delegate_->RunLauncherSearchQuery(query);
}

void LauncherSearchIphView::OpenAssistantPage() {
  if (scoped_iph_session_) {
    scoped_iph_session_->NotifyEvent(kIphEventNameAssistantClick);
  }
  delegate_->OpenAssistantPage();
}

}  // namespace ash

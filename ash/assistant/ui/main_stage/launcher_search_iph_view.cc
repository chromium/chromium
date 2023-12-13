// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/launcher_search_iph_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/assistant/ui/main_stage/chip_view.h"
#include "ash/public/cpp/app_list/app_list_client.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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

using QueryType = assistant::LauncherSearchIphQueryType;

constexpr char kLauncherSearchIphQueryTypeHistogramPrefix[] =
    "Assistant.LauncherSearchIphQueryType.";
constexpr char kLauncherSearchIphQueryFromSearchBox[] = "SearchBox";
constexpr char kLauncherSearchIphQueryFromAssistantPage[] = "AssistantPage";

constexpr int kMainLayoutBetweenChildSpacing = 16;
constexpr int kActionContainerBetweenChildSpacing = 8;

constexpr int kNumberOfQueryChips = 3;

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

std::vector<QueryType> GetQueryChips() {
  std::vector<QueryType> chips = {
      QueryType::kWeather,         QueryType::kUnitConversion1,
      QueryType::kUnitConversion2, QueryType::kTranslation,
      QueryType::kDefinition,      QueryType::kCalculation};
  CHECK_GE(static_cast<int>(chips.size()), kNumberOfQueryChips);
  base::RandomShuffle(chips.begin(), chips.end());
  chips.resize(kNumberOfQueryChips);
  return chips;
}

int GetQueryTextId(QueryType type) {
  switch (type) {
    case QueryType::kWeather:
      return IDS_ASH_ASSISTANT_LAUNCHER_SEARCH_IPH_CHIP_WEATHER;
    case QueryType::kUnitConversion1:
      return IDS_ASH_ASSISTANT_LAUNCHER_SEARCH_IPH_CHIP_UNIT_CONVERSION1;
    case QueryType::kUnitConversion2:
      return IDS_ASH_ASSISTANT_LAUNCHER_SEARCH_IPH_CHIP_UNIT_CONVERSION2;
    case QueryType::kTranslation:
      return IDS_ASH_ASSISTANT_LAUNCHER_SEARCH_IPH_CHIP_TRANSLATION;
    case QueryType::kDefinition:
      return IDS_ASH_ASSISTANT_LAUNCHER_SEARCH_IPH_CHIP_DEFINITION;
    case QueryType::kCalculation:
      return IDS_ASH_ASSISTANT_LAUNCHER_SEARCH_IPH_CHIP_CALCULATION;
  }
}

std::u16string GetQueryText(QueryType type) {
  int id = GetQueryTextId(type);
  return l10n_util::GetStringUTF16(id);
}

}  // namespace

LauncherSearchIphView::LauncherSearchIphView(
    Delegate* delegate,
    bool is_in_tablet_mode,
    std::unique_ptr<ScopedIphSession> scoped_iph_session,
    UiLocation location)
    : delegate_(delegate),
      scoped_iph_session_(std::move(scoped_iph_session)),
      location_(location) {
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
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_ASSISTANT_LAUNCHER_SEARCH_IPH_TITLE)));
  title_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  title_label->SetEnabledColorId(kColorAshTextColorPrimary);

  views::Label* description_label = text_container->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_ASSISTANT_LAUNCHER_SEARCH_IPH_DESCRIPTION)));
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

  CreateQueryChips(actions_container);

  if (location_ == UiLocation::kSearchBox) {
    views::View* spacer =
        actions_container->AddChildView(std::make_unique<views::View>());
    actions_container->SetFlexForView(spacer, 1);

    ash::PillButton* assistant_button =
        actions_container->AddChildView(std::make_unique<ash::PillButton>(
            base::BindRepeating(&LauncherSearchIphView::OpenAssistantPage,
                                weak_ptr_factory_.GetWeakPtr()),
            l10n_util::GetStringUTF16(
                IDS_ASH_ASSISTANT_LAUNCHER_SEARCH_IPH_CHIP_ASSISTANT)));
    assistant_button->SetID(ViewId::kAssistant);
    assistant_button->SetPillButtonType(
        PillButton::Type::kDefaultLargeWithoutIcon);
  }

  if (is_in_tablet_mode || location_ == UiLocation::kAssistantPage) {
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

void LauncherSearchIphView::VisibilityChanged(views::View* starting_from,
                                              bool is_visible) {
  if (is_visible) {
    ShuffleChipsQuery();

    // Label size should be changed. The `PreferredSizeChanged()` in label is
    // not bubbled up to this view, so we need to explicitly call it here.
    PreferredSizeChanged();
  }
}

void LauncherSearchIphView::NotifyAssistantButtonPressedEvent() {
  if (scoped_iph_session_) {
    scoped_iph_session_->NotifyEvent(kIphEventNameAssistantClick);
  }
}

std::vector<raw_ptr<ChipView>> LauncherSearchIphView::GetChipsForTesting() {
  return chips_;
}

void LauncherSearchIphView::RunLauncherSearchQuery(QueryType query_type) {
  const std::string& location = location_ == UiLocation::kSearchBox
                                    ? kLauncherSearchIphQueryFromSearchBox
                                    : kLauncherSearchIphQueryFromAssistantPage;
  base::UmaHistogramEnumeration(
      kLauncherSearchIphQueryTypeHistogramPrefix + location, query_type);

  if (scoped_iph_session_) {
    scoped_iph_session_->NotifyEvent(kIphEventNameChipClick);
  }

  delegate_->RunLauncherSearchQuery(GetQueryText(query_type));
}

void LauncherSearchIphView::OpenAssistantPage() {
  NotifyAssistantButtonPressedEvent();
  delegate_->OpenAssistantPage();
}

void LauncherSearchIphView::CreateQueryChips(views::View* actions_container) {
  int query_chip_view_id = ViewId::kChipStart;
  for (auto query_type : GetQueryChips()) {
    ChipView* chip = actions_container->AddChildView(
        std::make_unique<ChipView>(ChipView::Type::kLarge));
    chip->SetText(GetQueryText(query_type));
    chip->SetCallback(
        base::BindRepeating(&LauncherSearchIphView::RunLauncherSearchQuery,
                            weak_ptr_factory_.GetWeakPtr(), query_type));
    chip->SetID(query_chip_view_id);
    query_chip_view_id++;
    chips_.emplace_back(chip);
  }
}

void LauncherSearchIphView::ShuffleChipsQuery() {
  size_t chip_index = 0;
  for (auto query_type : GetQueryChips()) {
    CHECK_LT(chip_index, chips_.size());
    auto chip = chips_[chip_index++];
    chip->SetText(GetQueryText(query_type));
    chip->SetCallback(
        base::BindRepeating(&LauncherSearchIphView::RunLauncherSearchQuery,
                            weak_ptr_factory_.GetWeakPtr(), query_type));
  }
}

BEGIN_METADATA(LauncherSearchIphView)
END_METADATA

}  // namespace ash

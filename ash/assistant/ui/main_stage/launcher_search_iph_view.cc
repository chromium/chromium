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
#include "base/memory/raw_ptr.h"
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
#include "ui/views/accessibility/view_accessibility.h"
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

std::u16string g_chip_text_for_testing;

constexpr char kLauncherSearchIphQueryTypeHistogramPrefix[] =
    "Assistant.LauncherSearchIphQueryType.";
constexpr char kLauncherSearchIphQueryFromSearchBox[] = "SearchBox";
constexpr char kLauncherSearchIphQueryFromAssistantPage[] = "AssistantPage";

constexpr int kMainLayoutBetweenChildSpacing = 16;
constexpr int kActionContainerBetweenChildSpacing = 8;

constexpr int kNumberOfQueryChipsSearchBox = 3;
constexpr int kNumberOfQueryChipsAssistantPage = 4;

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

std::vector<QueryType> GetRandomizedQueryChips(
    LauncherSearchIphView::UiLocation location) {
  std::vector<QueryType> chips = {
      QueryType::kWeather,         QueryType::kUnitConversion1,
      QueryType::kUnitConversion2, QueryType::kTranslation,
      QueryType::kDefinition,      QueryType::kCalculation};

  int num_of_chips = location == LauncherSearchIphView::UiLocation::kSearchBox
                         ? kNumberOfQueryChipsSearchBox
                         : kNumberOfQueryChipsAssistantPage;
  CHECK_GE(static_cast<int>(chips.size()), num_of_chips);
  base::RandomShuffle(chips.begin(), chips.end());
  chips.resize(num_of_chips);
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
  if (!g_chip_text_for_testing.empty()) {
    return g_chip_text_for_testing;
  }

  int id = GetQueryTextId(type);
  return l10n_util::GetStringUTF16(id);
}

std::u16string GetQueryTextAccessibleName(QueryType type) {
  std::u16string text = GetQueryText(type);
  return l10n_util::GetStringFUTF16(
      IDS_ASH_ASSISTANT_LAUNCHER_SEARCH_IPH_CHIP_ACCNAME_PREFIX, text);
}

}  // namespace

// static
void LauncherSearchIphView::SetChipTextForTesting(const std::u16string& text) {
  g_chip_text_for_testing = text;
}

LauncherSearchIphView::LauncherSearchIphView(
    Delegate* delegate,
    bool is_in_tablet_mode,
    std::unique_ptr<ScopedIphSession> scoped_iph_session,
    UiLocation location)
    : delegate_(delegate),
      is_in_tablet_mode_(is_in_tablet_mode),
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

  title_label_ = text_container->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_LAUNCHER_SEARCH_IPH_TITLE)));
  title_label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  title_label_->SetEnabledColorId(kColorAshTextColorPrimary);
  title_label_->GetViewAccessibility().SetRole(ax::mojom::Role::kHeading);

  views::Label* description_label = text_container->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_ASSISTANT_LAUNCHER_SEARCH_IPH_DESCRIPTION)));
  description_label->SetEnabledColorId(kColorAshTextColorPrimary);

  const TypographyProvider* typography_provider = TypographyProvider::Get();
  DCHECK(typography_provider) << "TypographyProvider must not be null";
  if (typography_provider) {
    typography_provider->StyleLabel(TypographyToken::kCrosTitle1,
                                    *title_label_);
    typography_provider->StyleLabel(TypographyToken::kCrosBody2,
                                    *description_label);
  }

  views::BoxLayoutView* actions_container =
      box_layout_view->AddChildView(std::make_unique<views::BoxLayoutView>());
  actions_container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  actions_container->SetBetweenChildSpacing(
      kActionContainerBetweenChildSpacing);

  CreateChips(actions_container);

  if (is_in_tablet_mode || location_ == UiLocation::kAssistantPage) {
    box_layout_view->SetBackground(views::CreateThemedRoundedRectBackground(
        kColorAshControlBackgroundColorInactive, kBackgroundRadiusTablet));
  } else {
    box_layout_view->SetBackground(views::CreateThemedRoundedRectBackground(
        kColorAshControlBackgroundColorInactive,
        base::i18n::IsRTL() ? kBackgroundRadiiClamshellRTL
                            : kBackgroundRadiiClamshellLTR));
  }
}

LauncherSearchIphView::~LauncherSearchIphView() = default;

void LauncherSearchIphView::VisibilityChanged(views::View* starting_from,
                                              bool is_visible) {
  if (is_visible && location_ == UiLocation::kAssistantPage) {
    // Only shuffle when the IPH is in AssistantPage.
    // When the IPH is in SearchBox, the chips will be recreated every time.
    ShuffleChipsQuery();

    SetChipsVisibility();

    // Label size should be changed. The `PreferredSizeChanged()` in label is
    // not bubbled up to this view, so we need to explicitly call it here.
    PreferredSizeChanged();
  }
}

void LauncherSearchIphView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (location_ == UiLocation::kAssistantPage) {
    // Will set visibility of chips in VisibilityChanged().
    return;
  }

  SetChipsVisibility();
}

void LauncherSearchIphView::NotifyAssistantButtonPressedEvent() {
  if (scoped_iph_session_) {
    scoped_iph_session_->NotifyEvent(kIphEventNameAssistantClick);
  }
}

std::u16string LauncherSearchIphView::GetTitleText() const {
  return title_label_->GetText();
}

std::vector<raw_ptr<ChipView>> LauncherSearchIphView::GetChipsForTesting() {
  return chips_;
}

views::View* LauncherSearchIphView::GetAssistantButtonForTesting() {
  return assistant_button_;
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

void LauncherSearchIphView::CreateChips(
    views::BoxLayoutView* actions_container) {
  CHECK(chips_.empty());

  int query_chip_view_id = ViewId::kChipStart;
  for (auto query_type : GetRandomizedQueryChips(location_)) {
    ChipView* chip = actions_container->AddChildView(
        std::make_unique<ChipView>(ChipView::Type::kLarge));
    chip->SetText(GetQueryText(query_type));
    chip->GetViewAccessibility().SetName(
        GetQueryTextAccessibleName(query_type));
    chip->SetCallback(
        base::BindRepeating(&LauncherSearchIphView::RunLauncherSearchQuery,
                            weak_ptr_factory_.GetWeakPtr(), query_type));
    chip->SetID(query_chip_view_id);
    query_chip_view_id++;
    chips_.emplace_back(chip);
  }

  // If the IPH is in the search box, will add an assistant button.
  if (location_ == UiLocation::kSearchBox) {
    views::View* spacer =
        actions_container->AddChildView(std::make_unique<views::View>());
    actions_container->SetFlexForView(spacer, 1);

    assistant_button_ =
        actions_container->AddChildView(std::make_unique<ash::PillButton>(
            base::BindRepeating(&LauncherSearchIphView::OpenAssistantPage,
                                weak_ptr_factory_.GetWeakPtr()),
            l10n_util::GetStringUTF16(
                IDS_ASH_ASSISTANT_LAUNCHER_SEARCH_IPH_CHIP_ASSISTANT)));
    assistant_button_->SetID(ViewId::kAssistant);
    assistant_button_->SetPillButtonType(
        PillButton::Type::kDefaultLargeWithoutIcon);
  }
}

void LauncherSearchIphView::ShuffleChipsQuery() {
  size_t chip_index = 0;
  for (auto query_type : GetRandomizedQueryChips(location_)) {
    CHECK_LT(chip_index, chips_.size());
    auto chip = chips_[chip_index++];
    chip->SetText(GetQueryText(query_type));
    chip->GetViewAccessibility().SetName(
        GetQueryTextAccessibleName(query_type));
    chip->SetCallback(
        base::BindRepeating(&LauncherSearchIphView::RunLauncherSearchQuery,
                            weak_ptr_factory_.GetWeakPtr(), query_type));
  }
}

void LauncherSearchIphView::SetChipsVisibility() {
  const int iph_width = GetContentsBounds().width();
  if (iph_width == 0) {
    return;
  }

  // Check the PreferredSize of all chips. If the width is wider than the
  // available width, do not show the last a few query chips but at least show
  // one chip.
  int running_width = 0;
  for (auto chip : chips_) {
    running_width += chip->GetPreferredSize().width();
    running_width += kActionContainerBetweenChildSpacing;
  }

  const auto iph_insets = is_in_tablet_mode_ ? kInnerBackgroundInsetsTablet
                                             : kInnerBackgroundInsetsClamshell;
  const int available_width = iph_width - iph_insets.width();

  int assistant_button_width = 0;
  if (location_ == UiLocation::kSearchBox) {
    assistant_button_width = assistant_button_->GetPreferredSize().width();

    // Add additional spacing before the `assistant_button_`.
    // The multiplier `2` is an arbitrary number.
    running_width += 2 * kActionContainerBetweenChildSpacing;
    running_width += assistant_button_width;
  } else {
    // Subtract the last spacing.
    running_width -= kActionContainerBetweenChildSpacing;
  }

  // At least show one chip.
  chips_[0]->SetVisible(true);

  // Show remaining chips if they fit.
  for (size_t index = chips_.size() - 1; index > 0; index--) {
    chips_[index]->SetVisible(running_width <= available_width);
    running_width -= chips_[index]->GetPreferredSize().width();
  }
}

BEGIN_METADATA(LauncherSearchIphView)
END_METADATA

}  // namespace ash

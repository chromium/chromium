// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/launcher_search_iph_view.h"

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
#include "ui/color/color_id.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {
namespace {
constexpr int kVerticalInset = 20;
constexpr int kHorizontalInset = 24;

constexpr int kMainLayoutBetweenChildSpacing = 16;
constexpr int kActionContainerBetweenChildSpacing = 8;

constexpr char16_t kTitleTextPlaceholder[] = u"Title text";
constexpr char16_t kDescriptionTextPlaceholder[] = u"Description text";

constexpr char16_t kChipOneQueryPlaceholder[] = u"Weather";
constexpr char16_t kChipTwoQueryPlaceholder[] = u"1+1";
constexpr char16_t kChipThreeQueryPlaceholder[] = u"5 cm in inches";

constexpr char16_t kAssistantButtonPlaceholder[] = u"Assistant";

constexpr views::Radii kBackgroundRadiiLTR = {.top_left = 16.0f,
                                              .top_right = 4.0f,
                                              .bottom_right = 16.0f,
                                              .bottom_left = 16.0f};

constexpr views::Radii kBackgroundRadiiRTL = {.top_left = 4.0f,
                                              .top_right = 16.0f,
                                              .bottom_right = 16.0f,
                                              .bottom_left = 16.0f};

}  // namespace

LauncherSearchIphView::LauncherSearchIphView(
    std::unique_ptr<ScopedIphSession> scoped_iph_session,
    raw_ptr<Delegate> delegate)
    : scoped_iph_session_(std::move(scoped_iph_session)), delegate_(delegate) {
  SetID(ViewId::kSelf);

  raw_ptr<views::BoxLayout> box_layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets::VH(kVerticalInset, kHorizontalInset)));
  box_layout->set_between_child_spacing(kMainLayoutBetweenChildSpacing);
  // Use `kStretch` for `actions_container` to get stretched.
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  // Add texts into a container to avoid stretching `views::Label`s.
  raw_ptr<views::BoxLayoutView> text_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  text_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  text_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  text_container->SetBetweenChildSpacing(kMainLayoutBetweenChildSpacing);

  raw_ptr<views::Label> title_label = text_container->AddChildView(
      std::make_unique<views::Label>(kTitleTextPlaceholder));
  title_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  title_label->SetEnabledColorId(kColorAshTextColorPrimary);

  raw_ptr<views::Label> description_label = text_container->AddChildView(
      std::make_unique<views::Label>(kDescriptionTextPlaceholder));
  description_label->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  description_label->SetEnabledColorId(kColorAshTextColorPrimary);

  raw_ptr<const TypographyProvider> typography_provider =
      TypographyProvider::Get();
  DCHECK(typography_provider) << "TypographyProvider must not be null";
  if (typography_provider) {
    typography_provider->StyleLabel(TypographyToken::kCrosTitle1, *title_label);
    typography_provider->StyleLabel(TypographyToken::kCrosBody2,
                                    *description_label);
  }

  raw_ptr<views::BoxLayoutView> actions_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  actions_container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  actions_container->SetBetweenChildSpacing(
      kActionContainerBetweenChildSpacing);

  int query_chip_view_id = ViewId::kChipStart;
  for (const std::u16string& query :
       {kChipOneQueryPlaceholder, kChipTwoQueryPlaceholder,
        kChipThreeQueryPlaceholder}) {
    raw_ptr<ChipView> chip = actions_container->AddChildView(
        std::make_unique<ChipView>(ChipView::Type::kLarge));
    chip->SetText(query);
    chip->SetCallback(
        base::BindRepeating(&LauncherSearchIphView::RunLauncherSearchQuery,
                            weak_ptr_factory_.GetWeakPtr(), query));
    chip->SetID(query_chip_view_id);
    query_chip_view_id++;
  }

  raw_ptr<views::View> spacer =
      actions_container->AddChildView(std::make_unique<views::View>());
  actions_container->SetFlexForView(spacer, 1);

  raw_ptr<ash::PillButton> assistant_button =
      actions_container->AddChildView(std::make_unique<ash::PillButton>(
          base::BindRepeating(&LauncherSearchIphView::OpenAssistantPage,
                              weak_ptr_factory_.GetWeakPtr()),
          kAssistantButtonPlaceholder));
  assistant_button->SetID(ViewId::kAssistant);
  assistant_button->SetPillButtonType(
      PillButton::Type::kDefaultLargeWithoutIcon);

  SetBackground(views::CreateThemedRoundedRectBackground(
      kColorAshControlBackgroundColorInactive,
      base::i18n::IsRTL() ? kBackgroundRadiiRTL : kBackgroundRadiiLTR,
      /*for_border_thickness=*/0));
}

LauncherSearchIphView::~LauncherSearchIphView() = default;

void LauncherSearchIphView::RunLauncherSearchQuery(
    const std::u16string& query) {
  delegate_->RunLauncherSearchQuery(query);
}

void LauncherSearchIphView::OpenAssistantPage() {
  delegate_->OpenAssistantPage();
}

}  // namespace ash

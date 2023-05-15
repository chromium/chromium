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
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "url/gurl.h"

namespace ash {
namespace {
constexpr int kMainLayoutBetweenChildSpacing = 16;
constexpr int kActionContainerBetweenChildSpacing = 8;

constexpr char16_t kTitleTextPlaceholder[] = u"Title text";
constexpr char16_t kDescriptionTextPlaceholder[] = u"Description text";
constexpr char16_t kLinkTextPlaceholder[] = u"Link text";

constexpr char16_t kChipOneQueryPlaceholder[] = u"Weather";
constexpr char16_t kChipTwoQueryPlaceholder[] = u"1+1";
constexpr char16_t kChipThreeQueryPlaceholder[] = u"5 cm in inches";

constexpr char16_t kAssistantButtonPlaceholder[] = u"Assistant";

constexpr views::Radii kBackgroundRadiiClamshellLTR = {.top_left = 16.0f,
                                                       .top_right = 4.0f,
                                                       .bottom_right = 16.0f,
                                                       .bottom_left = 16.0f};

constexpr views::Radii kBackgroundRadiiClamshellRTL = {.top_left = 4.0f,
                                                       .top_right = 16.0f,
                                                       .bottom_right = 16.0f,
                                                       .bottom_left = 16.0f};

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

}  // namespace

LauncherSearchIphView::LauncherSearchIphView(
    std::unique_ptr<ScopedIphSession> scoped_iph_session,
    Delegate* delegate,
    bool is_in_tablet_mode)
    : scoped_iph_session_(std::move(scoped_iph_session)), delegate_(delegate) {
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

  // Add a description text and a link into another container to have a
  // different between-child-spacing.
  views::BoxLayoutView* description_container =
      text_container->AddChildView(std::make_unique<views::BoxLayoutView>());
  description_container->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  description_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  views::Label* description_label = description_container->AddChildView(
      std::make_unique<views::Label>(kDescriptionTextPlaceholder));
  description_label->SetEnabledColorId(kColorAshTextColorPrimary);

  link_label_ = description_container->AddChildView(
      std::make_unique<views::Link>(kLinkTextPlaceholder));
  link_label_->SetID(ViewId::kDescriptionLinkLabel);
  link_label_->SetCallback(base::BindRepeating(
      &LauncherSearchIphView::OnLinkClicked, weak_ptr_factory_.GetWeakPtr()));

  const TypographyProvider* typography_provider = TypographyProvider::Get();
  DCHECK(typography_provider) << "TypographyProvider must not be null";
  if (typography_provider) {
    typography_provider->StyleLabel(TypographyToken::kCrosTitle1, *title_label);
    typography_provider->StyleLabel(TypographyToken::kCrosBody2,
                                    *description_label);
    typography_provider->StyleLabel(TypographyToken::kCrosBody2, *link_label_);
  }

  views::BoxLayoutView* actions_container =
      box_layout_view->AddChildView(std::make_unique<views::BoxLayoutView>());
  actions_container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  actions_container->SetBetweenChildSpacing(
      kActionContainerBetweenChildSpacing);

  int query_chip_view_id = ViewId::kChipStart;
  for (const std::u16string& query :
       {kChipOneQueryPlaceholder, kChipTwoQueryPlaceholder,
        kChipThreeQueryPlaceholder}) {
    ChipView* chip = actions_container->AddChildView(
        std::make_unique<ChipView>(ChipView::Type::kLarge));
    chip->SetText(query);
    chip->SetCallback(
        base::BindRepeating(&LauncherSearchIphView::RunLauncherSearchQuery,
                            weak_ptr_factory_.GetWeakPtr(), query));
    chip->SetID(query_chip_view_id);
    query_chip_view_id++;
  }

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

void LauncherSearchIphView::OnThemeChanged() {
  views::View::OnThemeChanged();

  ui::ColorProvider* color_provider = link_label_->GetColorProvider();
  DCHECK(color_provider) << "ColorProvider must not be null";
  if (color_provider) {
    // TODO(b/277380563): `views::Link::SetEnabledColorId` does not work.
    link_label_->SetEnabledColor(
        color_provider->GetColor(ui::ColorIds::kColorLabelForeground));
  }
}

void LauncherSearchIphView::RunLauncherSearchQuery(
    const std::u16string& query) {
  scoped_iph_session_->NotifyEvent(kIphEventNameChipClick);
  delegate_->RunLauncherSearchQuery(query);
}

void LauncherSearchIphView::OnLinkClicked(const ui::Event& event) {
  delegate_->OpenSearchBoxIphUrl();
}

void LauncherSearchIphView::OpenAssistantPage() {
  scoped_iph_session_->NotifyEvent(kIphEventNameAssistantClick);
  delegate_->OpenAssistantPage();
}

}  // namespace ash

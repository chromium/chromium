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
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "url/gurl.h"

namespace ash {
namespace {
constexpr int kVerticalInset = 20;
constexpr int kHorizontalInset = 24;

constexpr int kMainLayoutBetweenChildSpacing = 16;
constexpr int kActionContainerBetweenChildSpacing = 8;

constexpr char16_t kTitleTextPlaceholder[] = u"Title text";
constexpr char16_t kDescriptionTextPlaceholder[] = u"Description text";
constexpr char16_t kSeparator[] = u" ";
constexpr char16_t kLinkTextPlaceholder[] = u"Link text";

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

  std::u16string text(kDescriptionTextPlaceholder);
  text.append(kSeparator);
  text_range_ = gfx::Range(0, text.size());

  std::u16string link(kLinkTextPlaceholder);
  link_range_ = gfx::Range(text.size(), text.size() + link.size());

  description_label_ =
      text_container->AddChildView(std::make_unique<views::StyledLabel>());
  description_label_->SetID(kDescriptionLabel);
  description_label_->SetDefaultTextStyle(
      views::style::TextStyle::STYLE_PRIMARY);
  description_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  description_label_->SetText(text + link);

  raw_ptr<const TypographyProvider> typography_provider =
      TypographyProvider::Get();
  DCHECK(typography_provider) << "TypographyProvider must not be null";
  if (typography_provider) {
    typography_provider->StyleLabel(TypographyToken::kCrosTitle1, *title_label);
    description_label_->SetLineHeight(
        typography_provider->ResolveLineHeight(TypographyToken::kCrosBody2));
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

void LauncherSearchIphView::OnThemeChanged() {
  description_label_->ClearStyleRanges();

  // Apply no style if ranges are not initialized for a fail-safe behavior.
  DCHECK(!text_range_.is_empty());
  DCHECK(!link_range_.is_empty());
  if (text_range_.is_empty() || link_range_.is_empty()) {
    return;
  }

  views::StyledLabel::RangeStyleInfo text_range_style_info;
  text_range_style_info.text_style = views::style::TextStyle::STYLE_PRIMARY;

  views::StyledLabel::RangeStyleInfo link_range_style_info;
  link_range_style_info.text_style = views::style::TextStyle::STYLE_LINK;
  link_range_style_info.callback = base::BindRepeating(
      &LauncherSearchIphView::OnLinkClicked, weak_ptr_factory_.GetWeakPtr());
  raw_ptr<ui::ColorProvider> color_provider =
      description_label_->GetColorProvider();
  DCHECK(color_provider) << "ColorProvider must not be null";
  if (color_provider) {
    // `TextStyle::STYLE_LINK` has a different color from
    // `ui::ColorIds::kColorLabelForeground`.
    link_range_style_info.override_color =
        description_label_->GetColorProvider()->GetColor(
            ui::ColorIds::kColorLabelForeground);
  }

  raw_ptr<const TypographyProvider> typography_provider =
      TypographyProvider::Get();
  DCHECK(typography_provider) << "TypographyProvider must not be null";
  if (typography_provider) {
    // `TextStyle::STYLE_PRIMARY` is different from
    // `TypographyToken::kCrosBody2`.
    text_range_style_info.custom_font =
        typography_provider->ResolveTypographyToken(
            TypographyToken::kCrosBody2);
    link_range_style_info.custom_font =
        typography_provider->ResolveTypographyToken(
            TypographyToken::kCrosBody2);
  }

  description_label_->AddStyleRange(text_range_, text_range_style_info);
  description_label_->AddStyleRange(link_range_, link_range_style_info);
}

void LauncherSearchIphView::RunLauncherSearchQuery(
    const std::u16string& query) {
  delegate_->RunLauncherSearchQuery(query);
}

void LauncherSearchIphView::OnLinkClicked(const ui::Event& event) {
  delegate_->OpenSearchBoxIphUrl();
}

void LauncherSearchIphView::OpenAssistantPage() {
  delegate_->OpenAssistantPage();
}

}  // namespace ash

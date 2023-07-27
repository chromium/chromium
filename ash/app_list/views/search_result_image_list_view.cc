// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_image_list_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/views/search_result_image_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/table_layout_view.h"

namespace ash {

namespace {

// Border spacing for title_label_
constexpr int kPreferredTitleHorizontalMargins = 16;
constexpr int kPreferredTitleTopMargins = 12;
constexpr int kPreferredTitleBottomMargins = 4;

// Layout constants for `image_view_container_`.
constexpr int kImageContainerHorizontalMargins =
    kPreferredTitleHorizontalMargins;
constexpr int kImageContainerVerticalMargins = 8;
constexpr int kSpaceBetweenImages = 8;

// Layout constants for `image_info_container_`.
constexpr auto kInfoContainerMargins = gfx::Insets::TLBR(0, 8, 0, 0);
constexpr int kSpaceBetweenInfoTitleAndContent = 16;

}  // namespace

using views::LayoutAlignment;
using views::TableLayout;

SearchResultImageListView::SearchResultImageListView(
    AppListViewDelegate* view_delegate)
    : SearchResultContainerView(view_delegate) {
  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);

  title_label_ = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_IMAGES)));
  title_label_->SetBackgroundColor(SK_ColorTRANSPARENT);
  title_label_->SetAutoColorReadabilityEnabled(false);
  title_label_->SetEnabledColorId(kColorAshTextColorSecondary);
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kPreferredTitleTopMargins, kPreferredTitleHorizontalMargins,
      kPreferredTitleBottomMargins, kPreferredTitleHorizontalMargins)));
  title_label_->SetPaintToLayer();
  title_label_->layer()->SetFillsBoundsOpaquely(false);

  SetAccessibleRole(ax::mojom::Role::kListBox);
  SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_ACCESSIBLE_NAME,
      title_label_->GetText()));

  image_view_container_ =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  image_view_container_->SetPaintToLayer();
  image_view_container_->layer()->SetFillsBoundsOpaquely(false);
  image_view_container_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred));

  // `image_view_container_` flex layout settings.
  image_view_container_->SetInteriorMargin(gfx::Insets::TLBR(
      kImageContainerVerticalMargins, kImageContainerHorizontalMargins,
      kImageContainerVerticalMargins, kImageContainerHorizontalMargins));
  image_view_container_->SetCollapseMargins(true);
  image_view_container_->SetDefault(views::kMarginsKey,
                                    gfx::Insets::VH(0, kSpaceBetweenImages));

  for (size_t i = 0;
       i < SharedAppListConfig::instance().image_search_max_results(); ++i) {
    auto* image_view = image_view_container_->AddChildView(
        std::make_unique<SearchResultImageView>(/*index=*/1, this));
    image_view->SetPaintToLayer();
    image_view->layer()->SetFillsBoundsOpaquely(false);
    image_views_.push_back(image_view);
  }

  // TODO(crbug.com/1352636): replace mock results with real results.
  std::vector<std::u16string> info_strings = {
      u"3.46MB", u"Today 13:28", u"image/png", u"My files/Downloads/abc.png"};
  std::vector<int> title_string_ids = {
      IDS_ASH_SEARCH_RESULT_IMAGE_FILE_SIZE,
      IDS_ASH_SEARCH_RESULT_IMAGE_DATE_MODIFIED,
      IDS_ASH_SEARCH_RESULT_IMAGE_FILE_TYPE,
      IDS_ASH_SEARCH_RESULT_IMAGE_FILE_LOCATION};

  auto append_image_info = [&](int idx) {
    auto title_label = std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(title_string_ids[idx]));
    auto content_label = std::make_unique<views::Label>(info_strings[idx]);
    if (chromeos::features::IsJellyEnabled()) {
      TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                            *title_label);
      title_label->SetEnabledColorId(cros_tokens::kColorPrimary);
      TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosAnnotation1,
                                            *content_label);
      content_label->SetEnabledColorId(cros_tokens::kCrosSysSecondary);
    } else {
      title_label->SetFontList(
          views::Label::GetDefaultFontList().DeriveWithWeight(
              gfx::Font::Weight::MEDIUM));
    }

    // Elide the file path if needed.
    if (title_string_ids[idx] == IDS_ASH_SEARCH_RESULT_IMAGE_FILE_LOCATION) {
      content_label->SetElideBehavior(gfx::ElideBehavior::ELIDE_MIDDLE);
    }

    image_info_container_->AddChildView(std::move(title_label));
    image_info_container_->AddChildView(std::move(content_label));
  };

  image_info_container_ = image_view_container_->AddChildView(
      std::make_unique<views::TableLayoutView>());
  image_info_container_->SetVisible(false);
  image_info_container_->SetBorder(
      views::CreateEmptyBorder(kInfoContainerMargins));
  image_info_container_->AddColumn(
      LayoutAlignment::kStart, LayoutAlignment::kStretch,
      TableLayout::kFixedSize, TableLayout::ColumnSize::kUsePreferred, 0, 0);
  image_info_container_->AddPaddingColumn(TableLayout::kFixedSize,
                                          kSpaceBetweenInfoTitleAndContent);
  image_info_container_->AddColumn(
      LayoutAlignment::kStart, LayoutAlignment::kStretch, 1.0f,
      TableLayout::ColumnSize::kUsePreferred, 0, 0);
  image_info_container_->AddRows(title_string_ids.size(), 1.0f);
  for (size_t i = 0; i < title_string_ids.size(); ++i) {
    append_image_info(i);
  }
}

SearchResultImageListView::~SearchResultImageListView() = default;

void SearchResultImageListView::SearchResultActivated(
    SearchResultImageView* view,
    int event_flags,
    bool by_button_press) {
  if (!view_delegate() || !view || !view->result()) {
    return;
  }

  view_delegate()->OpenSearchResult(
      view->result()->id(), event_flags,
      AppListLaunchedFrom::kLaunchedFromSearchBox,
      AppListLaunchType::kSearchResult, -1 /* suggestion_index */,
      !by_button_press && view->is_default_result() /* launch_as_default */);
}

SearchResultImageView* SearchResultImageListView::GetResultViewAt(
    size_t index) {
  DCHECK(index < image_views_.size());
  return image_views_[index];
}

void SearchResultImageListView::AppendShownResultMetadata(
    std::vector<SearchResultAimationMetadata>* result_metadata_) {
  // TODO(crbug.com/1352636) Update once animations are defined by UX.
  return;
}

std::vector<SearchResultImageView*>
SearchResultImageListView::GetSearchResultImageViews() {
  return image_views_;
}

void SearchResultImageListView::ConfigureLayoutForAvailableWidth(int width) {
  const int image_count = image_views_.size();
  const int margin_space = kImageContainerHorizontalMargins * 2 +
                           kSpaceBetweenImages * (image_count - 1);
  const int image_width = std::max(0, (width - margin_space) / image_count);

  for (auto* image_view : image_views_) {
    image_view->ConfigureLayoutForAvailableWidth(image_width);
  }
}

void SearchResultImageListView::OnSelectedResultChanged() {
  // TODO(crbug.com/1352636) once result selection spec is available.
  return;
}

int SearchResultImageListView::DoUpdate() {
  std::vector<SearchResult*> display_results =
      SearchModel::FilterSearchResultsByFunction(
          results(), base::BindRepeating([](const SearchResult& result) {
            return result.display_type() == SearchResultDisplayType::kImage;
          }),
          SharedAppListConfig::instance().image_search_max_results());

  const size_t num_results = display_results.size();

  for (size_t i = 0; i < image_views_.size(); ++i) {
    SearchResultImageView* result_view = GetResultViewAt(i);
    if (i < num_results) {
      result_view->SetResult(display_results[i]);
      result_view->SizeToPreferredSize();
    } else {
      result_view->SetResult(nullptr);
    }
  }

  NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged, false);
  return num_results;
}

void SearchResultImageListView::UpdateResultsVisibility(bool force_hide) {
  SetVisible(!force_hide);
  for (size_t i = 0; i < image_views_.size(); ++i) {
    SearchResultImageView* result_view = GetResultViewAt(i);
    result_view->SetVisible(i < num_results() && !force_hide);
  }
  image_info_container_->SetVisible(num_results() == 1 && !force_hide);
}

views::View* SearchResultImageListView::GetTitleLabel() {
  return title_label_.get();
}

std::vector<views::View*> SearchResultImageListView::GetViewsToAnimate() {
  return {image_view_container_};
}

BEGIN_METADATA(SearchResultImageListView, SearchResultContainerView)
END_METADATA

}  // namespace ash

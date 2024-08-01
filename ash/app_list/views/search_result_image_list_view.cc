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
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"

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

// The number of labels shown in `image_info_container_`, which should also be
// the size of `metadata_content_labels_`.
constexpr size_t kNumOfContentLabels = 3;

// Returns a displayable time for the last modified date in
// `image_info_container_`.
std::u16string GetFormattedTime(base::Time time) {
  std::u16string date_time_of_day = base::TimeFormatTimeOfDay(time);
  std::u16string relative_date = ui::TimeFormat::RelativeDate(time, nullptr);
  std::u16string formatted_time;
  if (!relative_date.empty()) {
    relative_date = base::ToLowerASCII(relative_date);
    formatted_time = l10n_util::GetStringFUTF16(
        IDS_ASH_SEARCH_RESULT_IMAGE_LAST_MODIFIED_RELATIVE_DATE_AND_TIME,
        relative_date, date_time_of_day);
  } else {
    formatted_time = l10n_util::GetStringFUTF16(
        IDS_ASH_SEARCH_RESULT_IMAGE_LAST_MODIFIED_DATE_AND_TIME,
        base::TimeFormatShortDate(time), date_time_of_day);
  }

  return l10n_util::GetStringFUTF16(
      IDS_ASH_SEARCH_RESULT_IMAGE_LAST_MODIFIED_STRING, formatted_time);
}

}  // namespace

using views::LayoutAlignment;

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

  GetViewAccessibility().SetRole(ax::mojom::Role::kListBox);
  GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
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
        std::make_unique<SearchResultImageView>(/*index=*/1, this, &delegate_));
    image_view->SetPaintToLayer();
    image_view->layer()->SetFillsBoundsOpaquely(false);
    image_views_.push_back(image_view);
  }

  image_info_container_ = image_view_container_->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  image_info_container_->SetBorder(
      views::CreateEmptyBorder(kInfoContainerMargins));

  // Initialize the vertical container in `image_info_container_`.
  image_info_container_->SetOrientation(views::LayoutOrientation::kVertical);
  image_info_container_->SetCollapseMargins(true);
  image_info_container_->SetMainAxisAlignment(LayoutAlignment::kCenter);
  image_info_container_->SetCrossAxisAlignment(LayoutAlignment::kStart);

  // Set the flex to restrict the sizes of the child labels.
  image_info_container_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kScaleToMaximum)
          .WithWeight(1));

  // Initialize the labels in the info container.
  for (size_t i = 0; i < kNumOfContentLabels; ++i) {
    auto content_label = std::make_unique<views::Label>();
    content_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

    if (i == 0) {
      // Make the image result file name, which is the first metadata label,
      // more prominent.
      content_label->SetProperty(views::kMarginsKey,
                                 gfx::Insets::TLBR(8, 0, 12, 0));
      content_label->SetMultiLine(true);
      content_label->SetAllowCharacterBreak(true);
      TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton1,
                                            *content_label);
      content_label->SetEnabledColorId(cros_tokens::kColorPrimary);
    } else {
      content_label->SetElideBehavior(gfx::ElideBehavior::ELIDE_MIDDLE);
      TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2,
                                            *content_label);
      content_label->SetEnabledColorId(cros_tokens::kTextColorSecondary);
    }

    metadata_content_labels_.push_back(content_label.get());
    image_info_container_->AddChildView(std::move(content_label));
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

std::vector<raw_ptr<SearchResultImageView, VectorExperimental>>
SearchResultImageListView::GetSearchResultImageViews() {
  return image_views_;
}

void SearchResultImageListView::ConfigureLayoutForAvailableWidth(int width) {
  const int image_count = image_views_.size();
  const int margin_space = kImageContainerHorizontalMargins * 2 +
                           kSpaceBetweenImages * (image_count - 1);
  const int image_width = std::max(0, (width - margin_space) / image_count);

  for (ash::SearchResultImageView* image_view : image_views_) {
    image_view->ConfigureLayoutForAvailableWidth(image_width);
  }
}

void SearchResultImageListView::OnImageMetadataLoaded(base::File::Info info) {
  if (num_results() != 1) {
    return;
  }

  // Check that there are 3 labels in `metadata_content_labels_`.
  CHECK_EQ(metadata_content_labels_.size(), kNumOfContentLabels);
  metadata_content_labels_[2]->SetText(GetFormattedTime(info.last_modified));
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

  if (num_results == 1) {
    SearchResult* display_result = display_results[0];
    const base::FilePath& displayable_file_path =
        display_result->displayable_file_path();
    CHECK_EQ(metadata_content_labels_.size(), kNumOfContentLabels);
    metadata_content_labels_[0]->SetText(
        base::UTF8ToUTF16(displayable_file_path.BaseName().value()));
    metadata_content_labels_[1]->SetText(
        base::UTF8ToUTF16(displayable_file_path.DirName().value()));

    CHECK(display_result->file_metadata_loader());
    display_result->file_metadata_loader()->RequestFileInfo(
        base::BindRepeating(&SearchResultImageListView::OnImageMetadataLoaded,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  auto* notifier = view_delegate()->GetNotifier();
  if (notifier) {
    std::vector<AppListNotifier::Result> notifier_results;
    for (const auto* result : display_results) {
      notifier_results.emplace_back(result->id(), result->metrics_type(),
                                    result->continue_file_suggestion_type());
    }
    notifier->NotifyResultsUpdated(SearchResultDisplayType::kImage,
                                   notifier_results);
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

BEGIN_METADATA(SearchResultImageListView)
END_METADATA

}  // namespace ash

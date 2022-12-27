// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_image_list_view.h"

#include <memory>
#include <vector>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/views/search_result_image_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"

namespace ash {

namespace {

// Border spacing for title_label_
constexpr int kPreferredTitleHorizontalMargins = 16;
constexpr int kPreferredTitleTopMargins = 12;
constexpr int kPreferredTitleBottomMargins = 4;

constexpr size_t kMaxImageResults = 4;

}  // namespace

SearchResultImageListView::SearchResultImageListView(
    AppListViewDelegate* view_delegate)
    : SearchResultContainerView(view_delegate) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
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

  image_view_container_ =
      AddChildView(std::make_unique<views::BoxLayoutView>());

  // TODO(crbug.com/1352636) replace mock results with real results.
  int dummy_search_result_id = 0;
  for (size_t i = 0; i < kMaxImageResults; ++i) {
    image_views_.emplace_back(new SearchResultImageView(
        "dummy id" + base::NumberToString(++dummy_search_result_id)));
    image_views_.back()->SetPaintToLayer();
    image_views_.back()->layer()->SetFillsBoundsOpaquely(false);
    image_views_.back()->SetVisible(true);
    image_view_container_->AddChildView(image_views_.back());
  }
}

SearchResultImageListView::~SearchResultImageListView() = default;

SearchResultImageView* SearchResultImageListView::GetResultViewAt(
    size_t index) {
  DCHECK(index < image_views_.size());
  return image_views_[index];
}

bool SearchResultImageListView::HasAnimatingChildView() {
  // TODO(crbug.com/1352636) Update once animations are defined by UX.
  return false;
}

void SearchResultImageListView::AppendShownResultMetadata(
    std::vector<SearchResultAimationMetadata>* result_metadata_) {
  // TODO(crbug.com/1352636) Update once animations are defined by UX.
  return;
}

absl::optional<SearchResultContainerView::ResultsAnimationInfo>
SearchResultImageListView::ScheduleResultAnimations(
    const ResultsAnimationInfo& aggregate_animation_info) {
  SetVisible(true);
  // TODO(crbug.com/1352636) Update once animations are defined by UX. There is
  // no animation information to be returned for this container.
  return absl::nullopt;
}

std::vector<SearchResultImageView*>
SearchResultImageListView::GetSearchResultImageViews() {
  return image_views_;
}

void SearchResultImageListView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kListBox;
}

void SearchResultImageListView::OnSelectedResultChanged() {
  // TODO(crbug.com/1352636) once result selection spec is available.
  return;
}
int SearchResultImageListView::DoUpdate() {
  // TODO(crbug.com/1352636) once backend results are available.
  return -1;
}

BEGIN_METADATA(SearchResultImageListView, SearchResultContainerView)
END_METADATA

}  // namespace ash

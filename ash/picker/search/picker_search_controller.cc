// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_search_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/picker/search/picker_search_aggregator.h"
#include "ash/picker/search/picker_search_request.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_client.h"
#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/time/time.h"

namespace ash {

PickerSearchController::PickerSearchController(PickerClient* client,
                                               base::TimeDelta burn_in_period)
    : client_(CHECK_DEREF(client)), burn_in_period_(burn_in_period) {}

PickerSearchController::~PickerSearchController() = default;

void PickerSearchController::StartSearch(
    const std::u16string& query,
    std::optional<PickerCategory> category,
    base::span<const PickerCategory> available_categories,
    PickerViewDelegate::SearchResultsCallback callback) {
  search_request_.reset();
  aggregator_.reset();
  aggregator_ = std::make_unique<PickerSearchAggregator>(burn_in_period_,
                                                         std::move(callback));
  search_request_ = std::make_unique<PickerSearchRequest>(
      query, std::move(category),
      base::BindRepeating(&PickerSearchAggregator::HandleSearchSourceResults,
                          aggregator_->GetWeakPtr()),
      &client_.get(), &emoji_search_, available_categories);
}

}  // namespace ash

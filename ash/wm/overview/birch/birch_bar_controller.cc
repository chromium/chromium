// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_bar_controller.h"

#include "ash/birch/birch_model.h"
#include "ash/shell.h"
#include "ash/wm/overview/birch/birch_bar_view.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"

namespace ash {

BirchBarController::BirchBarController() {
  // Fetching data from model.
  Shell::Get()->birch_model()->RequestBirchDataFetch(
      base::BindOnce(&BirchBarController::OnItemsFecthedFromModel,
                     weak_ptr_factory_.GetWeakPtr()));
}

BirchBarController::~BirchBarController() = default;

// static.
BirchBarController* BirchBarController::Get() {
  if (auto* overview_session = GetOverviewSession()) {
    return overview_session->birch_bar_controller();
  }
  return nullptr;
}

void BirchBarController::RegisterBar(
    BirchBarView* bar_view,
    base::OnceClosure bar_initialized_callback) {
  // Register the bar view and its initialized callback.
  bar_map_[bar_view] = std::move(bar_initialized_callback);

  // Directly initialize the bar view if data fetching is done.
  if (data_fetch_complete_) {
    InitBar(bar_view);
  }
}

void BirchBarController::OnBarDestroying(BirchBarView* bar_view) {
  // Clear the initialized callback.
  auto callback_iter = bar_map_.find(bar_view);
  if (callback_iter != bar_map_.end()) {
    bar_map_.erase(callback_iter);
  }
}

void BirchBarController::OnItemsFecthedFromModel() {
  // When data fetching completes, use the fetched items to initialize all the
  // bar views.
  data_fetch_complete_ = true;
  items_ = Shell::Get()->birch_model()->GetItemsForDisplay();

  for (auto& bar_and_callback : bar_map_) {
    InitBar(bar_and_callback.first);
  }
}

void BirchBarController::InitBar(BirchBarView* bar_view) {
  CHECK(data_fetch_complete_);

  for (auto& item : items_) {
    if (bar_view->GetChipsNum() == BirchBarView::kMaxChipsNum) {
      break;
    }
    bar_view->AddChip(item.get());
  }

  // Only run bar initialized callback if there are fetched items.
  if (items_.size() && !bar_map_[bar_view].is_null()) {
    std::move(bar_map_[bar_view]).Run();
  }
}

}  // namespace ash

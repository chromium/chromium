// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/search/search_box_model.h"

#include <string>
#include <utility>

#include "ash/app_list/model/search/search_box_model_observer.h"
#include "ash/public/cpp/app_list/app_list_client.h"
#include "base/check_is_test.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image.h"

namespace ash {

SearchBoxModel::SearchBoxModel() = default;

SearchBoxModel::~SearchBoxModel() = default;

void SearchBoxModel::SetGeminiButtonVisibility(
    std::optional<SearchBoxIconButton> search_box_icon_button) {
  gemini_search_box_icon_button_ = search_box_icon_button;

  if (gemini_search_box_icon_button_) {
    CHECK(!gemini_search_box_icon_button_.value().display_name.empty());
    CHECK(!gemini_search_box_icon_button_.value().icon.IsEmpty());
  }

  for (SearchBoxModelObserver& observer : observers_) {
    observer.ShowGeminiButtonChanged();
  }
}

void SearchBoxModel::SetSunfishButtonVisibility(
    SearchBoxModel::SunfishButtonVisibility show) {
  if (sunfish_button_visibility_ == show) {
    return;
  }
  sunfish_button_visibility_ = show;
  for (SearchBoxModelObserver& observer : observers_) {
    observer.SunfishButtonVisibilityChanged();
  }
}

void SearchBoxModel::SetWouldTriggerIph(bool would_trigger_iph) {
  if (would_trigger_iph_ == would_trigger_iph) {
    return;
  }

  would_trigger_iph_ = would_trigger_iph;
}

void SearchBoxModel::SetSearchEngineIsGoogle(bool is_google) {
  if (is_google == search_engine_is_google_)
    return;
  search_engine_is_google_ = is_google;
  for (auto& observer : observers_)
    observer.SearchEngineChanged();
}

void SearchBoxModel::AddObserver(SearchBoxModelObserver* observer) {
  observers_.AddObserver(observer);
}

void SearchBoxModel::RemoveObserver(SearchBoxModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash

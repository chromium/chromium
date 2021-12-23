// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/search/search_box_model.h"

#include <utility>

#include "ash/app_list/model/search/search_box_model_observer.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"

namespace ash {

SearchBoxModel::SearchBoxModel() = default;

SearchBoxModel::~SearchBoxModel() = default;

void SearchBoxModel::SetShowAssistantButton(bool show) {
  if (show_assistant_button_ == show)
    return;
  show_assistant_button_ = show;
  for (auto& observer : observers_)
    observer.ShowAssistantChanged();
}

void SearchBoxModel::SetSearchEngineIsGoogle(bool is_google) {
  if (is_google == search_engine_is_google_)
    return;
  search_engine_is_google_ = is_google;
  for (auto& observer : observers_)
    observer.SearchEngineChanged();
}

void SearchBoxModel::Update(const std::u16string& text,
                            bool initiated_by_user) {
  if (text_ == text)
    return;

  if (initiated_by_user) {
    const base::TimeTicks current_time = base::TimeTicks::Now();

    if (text_.empty() && !text.empty()) {
      UMA_HISTOGRAM_ENUMERATION("Apps.AppListSearchCommenced", 1, 2);
      base::RecordAction(base::UserMetricsAction("AppList_EnterSearch"));
      // The user started a new search. Record search start time.
      user_initiated_model_update_time_ = current_time;
    } else if (!text_.empty() && text.empty()) {
      // The user ended a search interaction. Reset search start time.
      base::RecordAction(base::UserMetricsAction("AppList_LeaveSearch"));
      user_initiated_model_update_time_ = base::TimeTicks();
    } else {
      DCHECK(!user_initiated_model_update_time_.is_null());
      UMA_HISTOGRAM_TIMES("Ash.SearchModelUpdateInterval",
                          current_time - user_initiated_model_update_time_);
      // The user updated an existing query. Record metrics and update
      // user_initiated_model_update_time_.
      user_initiated_model_update_time_ = current_time;
    }
  }

  text_ = text;
  for (auto& observer : observers_)
    observer.Update();
}

void SearchBoxModel::AddObserver(SearchBoxModelObserver* observer) {
  observers_.AddObserver(observer);
}

void SearchBoxModel::RemoveObserver(SearchBoxModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash

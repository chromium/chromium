// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/tabs_counter.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/browsing_data/core/pref_names.h"

TabsCounter::TabsCounter(Profile* profile) : profile_(profile) {}

TabsCounter::~TabsCounter() = default;

const char* TabsCounter::GetPrefName() const {
  return browsing_data::prefs::kCloseTabs;
}

void TabsCounter::Count() {
  base::Time begin_time = GetPeriodStart();
  base::Time end_time = GetPeriodEnd();

  int total_tab_count = 0;
  int total_window_count = 0;

  for (TabModel* tab_model : TabModelList::models()) {
    if (tab_model->GetProfile() != profile_ || tab_model->IsOffTheRecord()) {
      continue;
    }

    int tab_count =
        tab_model->GetTabCountNavigatedInTimeWindow(begin_time, end_time);

    if (tab_count > 0) {
      total_window_count++;
      total_tab_count += tab_count;
    }
  }

  TabModel* archived_tab_model = TabModelList::GetArchivedTabModel();
  if (archived_tab_model) {
    total_tab_count += archived_tab_model->GetTabCountNavigatedInTimeWindow(
        begin_time, end_time);
  }

  auto result =
      std::make_unique<TabsResult>(this, total_tab_count, total_window_count);
  ReportResult(std::move(result));
}

// TabsCounter::TabsResult -----------------------------------------

TabsCounter::TabsResult::TabsResult(const TabsCounter* source,
                                    ResultInt tab_count,
                                    ResultInt window_count)
    : FinishedResult(source, tab_count), window_count_(window_count) {}

TabsCounter::TabsResult::~TabsResult() = default;

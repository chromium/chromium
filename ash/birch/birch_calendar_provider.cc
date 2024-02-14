// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_calendar_provider.h"

#include <string>
#include <vector>

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "base/task/single_thread_task_runner.h"
#include "url/gurl.h"

namespace ash {

BirchCalendarProvider::BirchCalendarProvider(BirchModel* birch_model)
    : birch_model_(birch_model) {}

BirchCalendarProvider::~BirchCalendarProvider() = default;

void BirchCalendarProvider::RequestBirchDataFetch() {
  // TODO(jamescook): Perform a real calendar data fetch.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BirchCalendarProvider::OnCalendarInfoFetched,
                                weak_factory_.GetWeakPtr()));
}

void BirchCalendarProvider::OnCalendarInfoFetched() {
  std::vector<BirchCalendarItem> items;
  items.emplace_back(u"Placeholder Event", GURL(), base::Time::Now(),
                     base::Time::Now() + base::Hours(1));
  birch_model_->SetCalendarItems(std::move(items));
}

}  // namespace ash

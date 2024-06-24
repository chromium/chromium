// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/artemis/test_data_aggregator_service.h"

namespace ash::cfm {

namespace {

// Local convenience aliases
using mojom::DataFilter::FilterType::CHANGE;
using mojom::DataFilter::FilterType::REGEX;

}  // namespace

DataAggregatorServiceForTesting::DataAggregatorServiceForTesting()
    : DataAggregatorService() {}

DataAggregatorServiceForTesting::~DataAggregatorServiceForTesting() = default;

void DataAggregatorServiceForTesting::InitializeLocalSources() {
  DataAggregatorService::InitializeLocalSources();
  AddLocalWatchDogs();
}

void DataAggregatorServiceForTesting::AddLocalWatchDogs() {
  // Target source and filters were selected at random. Note that the
  // source MUST match one of the local sources created in the parent.
  // See kLocalCommandSources[] and kLocalLogSources[].
  std::string target_source = "ip -brief address";
  std::vector<mojom::DataFilterPtr> test_filters;
  test_filters.push_back(mojom::DataFilter::New(CHANGE, ""));
  test_filters.push_back(mojom::DataFilter::New(REGEX, "100.115.81.*"));

  // Use delay here if desired. Helps to test out the case where
  // a watchdog is added after everything else is set up.
  auto delay = base::Seconds(30);

  for (auto& filter : test_filters) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DataAggregatorServiceForTesting::AddLocalWatchDog,
                       base::Unretained(this), target_source,
                       std::move(filter)),
        delay);
  }
}

void DataAggregatorServiceForTesting::AddLocalWatchDog(
    const std::string& source,
    mojom::DataFilterPtr filter) {
  mojo::PendingReceiver<mojom::DataWatchDog> receiver;
  auto remote = receiver.InitWithNewPipeAndPassRemote();
  auto watchdog =
      std::make_unique<TestWatchDog>(std::move(receiver), std::move(filter));

  // We're expecting the caller to have provided a source that was created
  // in the parent (see kLocalCommandSources[] and kLocalLogSources[]), so
  // fail fast if this is not the case. It is likely a user error if so.
  CHECK(data_source_map_.count(source) > 0);

  data_source_map_[source]->AddWatchDog(watchdog->GetFilter(),
                                        std::move(remote), base::DoNothing());

  local_watchdogs_.push_back(std::move(watchdog));
}

}  // namespace ash::cfm

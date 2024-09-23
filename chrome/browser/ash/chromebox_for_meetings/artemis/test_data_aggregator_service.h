// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_TEST_DATA_AGGREGATOR_SERVICE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_TEST_DATA_AGGREGATOR_SERVICE_H_

#include "chrome/browser/ash/chromebox_for_meetings/artemis/data_aggregator_service.h"
#include "chrome/browser/ash/chromebox_for_meetings/artemis/test_watchdog.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_data_aggregator.mojom.h"

namespace ash::cfm {

class DataAggregatorServiceForTesting : public DataAggregatorService {
 public:
  DataAggregatorServiceForTesting();
  ~DataAggregatorServiceForTesting() override;
  DataAggregatorServiceForTesting(const DataAggregatorServiceForTesting&) =
      delete;
  DataAggregatorServiceForTesting& operator=(
      const DataAggregatorServiceForTesting&) = delete;

 protected:
  void InitializeLocalSources() override;

 private:
  void AddLocalWatchDogs();
  void AddLocalWatchDog(const std::string& source, mojom::DataFilterPtr filter);

  // Used to test watchdog functionality.
  std::vector<std::unique_ptr<TestWatchDog>> local_watchdogs_;
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_TEST_DATA_AGGREGATOR_SERVICE_H_

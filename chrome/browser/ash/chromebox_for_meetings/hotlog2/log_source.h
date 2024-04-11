// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_LOG_SOURCE_H_

#include <base/memory/weak_ptr.h>

#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/local_data_source.h"
#include "chromeos/ash/services/chromebox_for_meetings/public/mojom/meet_devices_data_aggregator.mojom.h"

namespace ash::cfm {

// This class tracks data from a single log file.
class LogSource : public LocalDataSource {
 public:
  LogSource(const std::string& source_name, base::TimeDelta poll_rate);
  LogSource(const LogSource&) = delete;
  LogSource& operator=(const LogSource&) = delete;
  ~LogSource() override;

 private:
  // LocalDataSource:
  const std::string& GetDisplayName() override;
  std::vector<std::string> GetNextData() override;

  std::string filepath_;

  // Must be the last class member.
  base::WeakPtrFactory<LogSource> weak_ptr_factory_{this};
};

}  // namespace ash::cfm
#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_LOG_SOURCE_H_

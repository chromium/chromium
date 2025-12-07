// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_RESOURCE_MONITOR_H_
#define CHROME_BROWSER_SCREEN_AI_RESOURCE_MONITOR_H_

#include <string_view>

#include "components/performance_manager/public/resource_attribution/process_context.h"
#include "components/performance_manager/public/resource_attribution/queries.h"

namespace screen_ai {

class ResourceMonitor : public resource_attribution::QueryResultObserver {
 public:
  static std::unique_ptr<ResourceMonitor> CreateForProcess(
      std::string_view process_name);

  ~ResourceMonitor() override;

  ResourceMonitor(const ResourceMonitor&) = delete;
  ResourceMonitor& operator=(const ResourceMonitor&) = delete;

  // QueryResultObserver:
  void OnResourceUsageUpdated(
      const resource_attribution::QueryResultMap& results) override;

  base::ByteCount get_max_resident_memory() const {
    return max_resident_memory_;
  }

 private:
  explicit ResourceMonitor(
      resource_attribution::ProcessContext& process_context);

  resource_attribution::ScopedResourceUsageQuery scoped_query_;
  resource_attribution::ScopedQueryObservation query_observation_{this};

  base::ByteCount max_resident_memory_;
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_SCREEN_AI_RESOURCE_MONITOR_H_

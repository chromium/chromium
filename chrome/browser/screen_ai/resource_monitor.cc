// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/resource_monitor.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/process_type.h"

namespace {
// LINT.IfChange(kSampleInterval)
constexpr base::TimeDelta kSampleInterval = base::Seconds(1);
// LINT.ThenChange(//chrome/browser/screen_ai/optical_character_recognizer_browsertest.cc:kResourceMeasurementInterval)
}  // namespace

namespace screen_ai {

// static
std::unique_ptr<ResourceMonitor> ResourceMonitor::CreateForProcess(
    std::string_view process_name) {
  std::u16string u16name = base::UTF8ToUTF16(process_name);
  content::BrowserChildProcessHost* process_host = nullptr;
  content::BrowserChildProcessHostIterator iter(content::PROCESS_TYPE_UTILITY);
  while (!iter.Done()) {
    if (iter.GetData().name == u16name) {
      process_host =
          content::BrowserChildProcessHost::FromID(iter.GetData().id);
      break;
    }
    ++iter;
  }
  if (!process_host) {
    return nullptr;
  }

  auto process_context =
      resource_attribution::ProcessContext::FromBrowserChildProcessHost(
          process_host);
  return process_context
             ? base::WrapUnique(new ResourceMonitor(process_context.value()))
             : nullptr;
}

ResourceMonitor::ResourceMonitor(
    resource_attribution::ProcessContext& process_context)
    : scoped_query_(resource_attribution::QueryBuilder()
                        .AddResourceType(
                            resource_attribution::ResourceType::kMemorySummary)
                        .AddResourceContext(process_context)
                        .CreateScopedQuery()) {
  query_observation_.Observe(&scoped_query_);
  scoped_query_.Start(kSampleInterval);
}

ResourceMonitor::~ResourceMonitor() = default;

void ResourceMonitor::OnResourceUsageUpdated(
    const resource_attribution::QueryResultMap& results) {
  if (results.size() == 0) {
    // Service process couldn't be measured.
    return;
  }

  CHECK_EQ(results.size(), 1ul);
  const resource_attribution::QueryResults& result = results.begin()->second;
  const std::optional<resource_attribution::MemorySummaryResult>& memory =
      result.memory_summary_result;

  if (memory.has_value()) {
    max_resident_memory_ =
        std::max(max_resident_memory_, memory->resident_set_size);
  }
}

}  // namespace screen_ai

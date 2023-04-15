// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/data_source.h"

#include <algorithm>

#include "ash/hud_display/memory_status.h"
#include "base/functional/bind.h"
#include "base/threading/thread_restrictions.h"

namespace ash {
namespace hud_display {
namespace {

// Returns number of bytes rounded up to next Gigabyte.
int64_t EstimatePhysicalRAMSize(int64_t total_ram) {
  // Round up to nearest Gigabyte.
  constexpr int64_t one_gig = 1024 * 1024 * 1024;
  if (total_ram % one_gig) {
    return ((total_ram / one_gig) + 1) * one_gig;
  }
  return total_ram;
}

// Calculates counter difference with respect to overflow.
CpuStats Delta(const CpuStats& newer, const CpuStats& older) {
  static_assert(sizeof(CpuStats) == sizeof(uint64_t) * 10,
                "This method should be updated when CpuStats is changed.");

  // Calculates (left - right) assuming |left| and |right| are increasing
  // unsigned counters with respect to possible counter overflow.
  auto minus = [](const uint64_t& left, const uint64_t right) {
    return left > right
               ? (left - right)
               : (left + (std::numeric_limits<uint64_t>::max() - right));
  };

  CpuStats result;
  result.user = minus(newer.user, older.user);
  result.nice = minus(newer.nice, older.nice);
  result.system = minus(newer.system, older.system);
  result.idle = minus(newer.idle, older.idle);
  result.iowait = minus(newer.iowait, older.iowait);
  result.irq = minus(newer.irq, older.irq);
  result.softirq = minus(newer.softirq, older.softirq);
  result.steal = minus(newer.steal, older.steal);
  result.guest = minus(newer.guest, older.guest);
  result.guest_nice = minus(newer.guest_nice, older.guest_nice);
  return result;
}

// Returns sum of all entries. This is useful for deltas to calculate
// percentage.
uint64_t Sum(const CpuStats& stats) {
  static_assert(sizeof(CpuStats) == sizeof(uint64_t) * 10,
                "This method should be updated when CpuStats is changed.");

  return stats.user + stats.nice + stats.system + stats.idle + stats.iowait +
         stats.irq + stats.softirq + stats.steal + stats.guest +
         stats.guest_nice;
}

}  // anonymous namespace

// --------------------------------

////////////////////////////////////////////////////////////////////////////////
// DataSource, public:

DataSource::Snapshot::Snapshot() = default;
DataSource::Snapshot::Snapshot(const Snapshot&) = default;
DataSource::Snapshot& DataSource::Snapshot::operator=(const Snapshot&) =
    default;

DataSource::DataSource() {
  cpu_stats_base_ = {0};
  cpu_stats_latest_ = {0};
}

DataSource::~DataSource() = default;

DataSource::Snapshot DataSource::GetSnapshotAndReset() {
  // Refresh data synchronously.
  Refresh();

  Snapshot snapshot = GetSnapshot();

  if (cpu_stats_base_.user > 0) {
    // Calculate CPU graph values for the last interval.
    CpuStats cpu_stats_delta = Delta(cpu_stats_latest_, cpu_stats_base_);
    const double cpu_ticks_total = Sum(cpu_stats_delta);

    // Makes sure that the given value is between 0 and 1 and converts to
    // float.
    auto to_0_1 = [](const double& value) -> float {
      return std::clamp(static_cast<float>(value), 0.0f, 1.0f);
    };

    snapshot.cpu_idle_part = cpu_stats_delta.idle / cpu_ticks_total;
    snapshot.cpu_user_part =
        (cpu_stats_delta.user + cpu_stats_delta.nice) / cpu_ticks_total;
    snapshot.cpu_system_part = cpu_stats_delta.system / cpu_ticks_total;
    // The remaining part is "other".
    snapshot.cpu_other_part =
        to_0_1(1 - snapshot.cpu_idle_part - snapshot.cpu_user_part -
               snapshot.cpu_system_part);
  }
  ResetCounters();
  return snapshot;
}

DataSource::Snapshot DataSource::GetSnapshot() const {
  return snapshot_;
}

void DataSource::ResetCounters() {
  snapshot_ = Snapshot();

  cpu_stats_base_ = cpu_stats_latest_;
  cpu_stats_latest_ = {0};
}

////////////////////////////////////////////////////////////////////////////////
// DataSource, private:

void DataSource::Refresh() {
  const MemoryStatus memory_status;

  snapshot_.physical_ram =
      std::max(snapshot_.physical_ram,
               EstimatePhysicalRAMSize(memory_status.total_ram_size()));
  snapshot_.total_ram =
      std::max(snapshot_.total_ram, memory_status.total_ram_size());
  snapshot_.free_ram = std::min(snapshot_.free_ram, memory_status.total_free());
  snapshot_.arc_rss = std::max(snapshot_.arc_rss, memory_status.arc_rss());
  snapshot_.arc_rss_shared =
      std::max(snapshot_.arc_rss_shared, memory_status.arc_rss_shared());
  snapshot_.browser_rss =
      std::max(snapshot_.browser_rss, memory_status.browser_rss());
  snapshot_.browser_rss_shared = std::max(snapshot_.browser_rss_shared,
                                          memory_status.browser_rss_shared());
  snapshot_.renderers_rss =
      std::max(snapshot_.renderers_rss, memory_status.renderers_rss());
  snapshot_.renderers_rss_shared = std::max(
      snapshot_.renderers_rss_shared, memory_status.renderers_rss_shared());
  snapshot_.gpu_rss_shared =
      std::max(snapshot_.gpu_rss_shared, memory_status.gpu_rss_shared());
  snapshot_.gpu_rss = std::max(snapshot_.gpu_rss, memory_status.gpu_rss());
  snapshot_.gpu_kernel =
      std::max(snapshot_.gpu_kernel, memory_status.gpu_kernel());

  cpu_stats_latest_ = GetProcStatCPU();
}

}  // namespace hud_display
}  // namespace ash

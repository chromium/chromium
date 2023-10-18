// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_TRACING_ARC_SYSTEM_STAT_COLLECTOR_H_
#define CHROME_BROWSER_ASH_ARC_TRACING_ARC_SYSTEM_STAT_COLLECTOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"

namespace base {
class SequencedTaskRunner;
class TimeDelta;
class Value;
}  // namespace base

namespace arc {

class ArcSystemModel;

// Collects various system statistics and appends results to the
// |ArcSystemModel|.
class ArcSystemStatCollector {
 public:
  // Indices of fields to parse zram info, see
  // https://www.kernel.org/doc/Documentation/block/stat.txt
  static constexpr int kZramStatColumns[] = {
      2,   // number of sectors read
      6,   // number of sectors written
      10,  // total wait time for all requests (milliseconds)
      -1,  // End of sequence
  };

  // Indices of fields to parse /proc/meminfo
  // As an example:
  // MemTotal:        8058940 kB
  // MemFree:          314184 kB
  // MemAvailable:    2714260 kB
  // ...
  static constexpr int kMemInfoColumns[] = {
      1,   // MemTotal in kb.
      7,   // MemAvailable in kb.
      -1,  // End of sequence
  };

  // Indices of fields to parse /run/debugfs_gpu/i915_gem_objects
  // As an example:
  // 656 objects, 354971648 bytes
  // 113 unbound objects, 17240064 bytes
  // ...
  static constexpr int kGemInfoColumns[] = {
      0,   // Number of objects.
      2,   // Used memory in bytes.
      -1,  // End of sequence
  };

  // Indices of fields to parse as one value.
  // For example: /sys/class/hwmon/hwmon*/temp*_input
  // 30000
  static constexpr int kOneValueColumns[] = {
      0,
      -1,  // End of sequence
  };

  ArcSystemStatCollector();

  ArcSystemStatCollector(const ArcSystemStatCollector&) = delete;
  ArcSystemStatCollector& operator=(const ArcSystemStatCollector&) = delete;

  ~ArcSystemStatCollector();

  // Starts sample collection, |max_interval| defines the maximum interval and
  // it is used for circle buffer size calculation.
  void Start(const base::TimeDelta& max_interval);
  // Stops sample collection.
  void Stop();
  // Appends collected samples to |system_model|.|min_timestamp| and
  // |max_timestamp| specify the minimum and maximum timestamps respectively to
  // add to |system_model|.
  void Flush(const base::TimeTicks& min_timestamp,
             const base::TimeTicks& max_timestamp,
             ArcSystemModel* system_model);

  // Serializes the model to |base::Value|.
  std::unique_ptr<base::Value> Serialize() const;
  // Serializes the model to Json string.
  std::string SerializeToJson() const;
  // Loads the model from |base::Value|.
  bool LoadFromValue(const base::Value& root);
  // Loads the model from Json string.
  bool LoadFromJson(const std::string& json_data);

  base::TimeDelta max_interval() const { return max_interval_; }

 private:
  struct Sample;
  struct SystemReadersContext;

  struct RuntimeFrame {
    RuntimeFrame();

    base::TimeTicks timestamp;
    // read, written sectors and total time in milliseconds.
    int64_t zram_stat[std::size(kZramStatColumns) - 1] = {0};
    // total, available.
    int64_t mem_info[std::size(kMemInfoColumns) - 1] = {0};
    // objects, used bytes.
    int64_t gem_info[std::size(kGemInfoColumns) - 1] = {0};
    // Temperature of CPU, either the package or Core 0.
    int64_t cpu_temperature = std::numeric_limits<int>::min();
    // CPU Frequency.
    int64_t cpu_frequency = 0;
    // CPU energy in micro-joules for Intel platforms.
    int64_t cpu_energy = 0;
    // GPU energy in micro-joules for some Intel platforms.
    int64_t gpu_energy = 0;
    // Memory energy in micro-joules for some Intel platforms.
    int64_t memory_energy = 0;
    // Power constraint for CPU package.
    int64_t package_power_constraint = 0;
  };

  // Schedules reading System stat files in |ReadSystemStatOnBackgroundThread|
  // on background thread. Once ready result is passed to
  // |UpdateSystemStatOnUiThread|
  void ScheduleSystemStatUpdate();

  // Frees |context_| if it exists.
  void FreeSystemReadersContext();

  // Called when |SystemReadersContext| is initialized.
  void OnInitOnUiThread(std::unique_ptr<SystemReadersContext> context);

  // Reads system stat files on background thread using |context|.
  static std::unique_ptr<SystemReadersContext> ReadSystemStatOnBackgroundThread(
      std::unique_ptr<SystemReadersContext> context);
  // Processes filled |current_frame_| on UI thread.
  void UpdateSystemStatOnUiThread(
      std::unique_ptr<SystemReadersContext> context);

  // To schedule updates of system stat.
  base::RepeatingTimer timer_;
  // Performs reading kernel stat files on backgrond thread.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  // Used to limit the number of warnings printed in case System stat update is
  // dropped due to previous update is in progress.
  int missed_update_warning_left_ = 0;

  // Samples are implemented as a circle buffer.
  std::vector<Sample> samples_;
  size_t write_index_ = 0;

  // Used to calculate delta.
  RuntimeFrame previous_frame_;

  // Defines the maximum interval and it is used for circle buffer size
  // calculation.
  base::TimeDelta max_interval_;

  std::unique_ptr<SystemReadersContext> context_;

  base::WeakPtrFactory<ArcSystemStatCollector> weak_ptr_factory_{this};
};

// Helper that reads and parses stat file containing decimal number separated by
// whitespace and text fields. It does not have any dynamic memory allocation.
// |fd| specifies the file descriptor to read and parse. |columns| contains
// index of column to parse, end of sequence is specified by terminator -1.
// |output| receives parsed value. Must be the size as |columns| size - 1.
bool ParseStatFile(int fd, const int* columns, int64_t* output);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_TRACING_ARC_SYSTEM_STAT_COLLECTOR_H_

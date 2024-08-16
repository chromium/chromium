// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/arc/tracing/arc_system_stat_collector.h"

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <utility>

#include "base/cpu.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ash/arc/tracing/arc_system_model.h"
#include "chrome/browser/ash/arc/tracing/arc_value_event_trimmer.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc {

namespace {

// Interval to update system stats.
constexpr base::TimeDelta kSystemStatUpdateInterval = base::Milliseconds(10);

const base::FilePath::CharType kZramPath[] =
    FILE_PATH_LITERAL("/sys/block/zram0/stat");
const base::FilePath::CharType kMemoryInfoPath[] =
    FILE_PATH_LITERAL("/proc/meminfo");
#if defined(ARCH_CPU_ARM_FAMILY)
const base::FilePath::CharType kGemInfoPath[] =
    FILE_PATH_LITERAL("/run/debugfs_gpu/exynos_gem_objects");
#else
const base::FilePath::CharType kGemInfoPath[] =
    FILE_PATH_LITERAL("/run/debugfs_gpu/i915_gem_objects");
#endif
const base::FilePath::CharType kCpuFrequencyPath[] =
    FILE_PATH_LITERAL("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");

const base::FilePath::CharType kPowercapPath[] =
    FILE_PATH_LITERAL("/sys/class/powercap");
const base::FilePath::CharType kIntelRaplQuery[] =
    FILE_PATH_LITERAL("intel-rapl:*");
const base::FilePath::CharType kEnergyPath[] = FILE_PATH_LITERAL("energy_uj");
const base::FilePath::CharType kLongTermConstraintPath[] =
    FILE_PATH_LITERAL("constraint_0_power_limit_uw");
const base::FilePath::CharType kNamePath[] = FILE_PATH_LITERAL("name");

constexpr char kPackagePowerDomainName[] = "package-0";
constexpr char kCpuPowerDomainName[] = "core";
constexpr char kGpuPowerDomainName[] = "uncore";
constexpr char kMemoryPowerDomainName[] = "dram";

bool IsWhitespace(char c) {
  return c == ' ' || c == '\t' || c == '\n';
}

bool IsDigit(char c) {
  return c >= '0' && c <= '9';
}

bool IsEnd(char c) {
  return IsWhitespace(c) || c == 0;
}

// Detects path to stat file that contains temperature for CPU package that is
// used as temperature for CPU.
// Prefer package temperature if available. Otherwise, fall back on CPU
// core 0. Not all cores may be covered by CPU core 0.
// Package temperature is the weighted average of the different cores according
// to:
//   www.intel.com/content/www/us/en/support/articles/000058845/processors.html
class CpuTemperaturePathDetector {
 public:
  // Detected path is stored |path_|
  CpuTemperaturePathDetector() {
    base::FileEnumerator hwmon_enumerator(
        base::FilePath(FILE_PATH_LITERAL("/sys/class/hwmon/")),
        false /* recursive */, base::FileEnumerator::DIRECTORIES,
        FILE_PATH_LITERAL("hwmon*"));
    for (base::FilePath hwmon_path = hwmon_enumerator.Next();
         !hwmon_path.empty(); hwmon_path = hwmon_enumerator.Next()) {
      base::FileEnumerator enumerator(
          hwmon_path, false, base::FileEnumerator::FILES, "temp*_label");
      for (base::FilePath temperature_label_path = enumerator.Next();
           !temperature_label_path.empty();
           temperature_label_path = enumerator.Next()) {
        std::string label;
        if (!base::ReadFileToString(temperature_label_path, &label)) {
          continue;
        }
        base::TrimWhitespaceASCII(label, base::TRIM_TRAILING, &label);
        bool package_temp = label == "Package id 0";
        if (label != "Core 0" && label != "Physical id 0" && !package_temp) {
          continue;
        }
        std::string temperature_input_path_string =
            temperature_label_path.value();
        base::ReplaceSubstringsAfterOffset(&temperature_input_path_string, 0,
                                           "label", "input");
        const base::FilePath temperature_input_path =
            base::FilePath(temperature_input_path_string);
        if (!base::PathExists(temperature_input_path)) {
          continue;
        }
        path_ = temperature_input_path;
        VLOG(1) << "Detected path to read CPU temperature (" << label
                << "): " << temperature_input_path;

        // If we already found the ideal temperature source, no need to continue
        // iterating. Using Core 0 would require running all iterations of this
        // loop.
        if (package_temp) {
          return;
        }
      }

      if (!path_.empty()) {
        return;
      }
    }
    LOG(WARNING) << "Not detected path to read CPU temperature.";
  }

  CpuTemperaturePathDetector(const CpuTemperaturePathDetector&) = delete;
  CpuTemperaturePathDetector& operator=(const CpuTemperaturePathDetector&) =
      delete;

  const base::FilePath& path() const { return path_; }

 private:
  base::FilePath path_;
};

const base::FilePath& GetCpuTemperaturePathOnFileThread() {
  static base::NoDestructor<CpuTemperaturePathDetector> instance;
  return instance->path();
}

bool ReadNonNegativeInt(const base::Value::Dict& root,
                        const std::string& key,
                        int* out) {
  std::optional<int> value = root.FindInt(key);
  if (!value || *value < 0) {
    return false;
  }
  *out = *value;
  return true;
}

enum SystemReader {
  kZram = 0,
  kMemoryInfo,
  kGemInfo,
  kCpuTemperature,
  kCpuFrequency,
  kCpuEnergy,
  kGpuEnergy,
  kMemoryEnergy,
  kPackagePowerConstraint,
  kTotal
};

constexpr char kKeyCpuFrequency[] = "cpu_frequency";
constexpr char kKeyCpuPower[] = "cpu_power";
constexpr char kKeyCpuTemperature[] = "cpu_temperature";
constexpr char kKeyGemObjects[] = "gem_objects";
constexpr char kKeyGemSizeKb[] = "gem_size_kb";
constexpr char kKeyGpuPower[] = "gpu_power";
constexpr char kKeyMaxInterval[] = "max_interval";
constexpr char kKeyMemoryPower[] = "memory_power";
constexpr char kKeyMemTotalKb[] = "mem_total_kb";
constexpr char kKeyMemUsedKb[] = "mem_used_kb";
constexpr char kKeyPackagePowerConstraint[] = "package_power_constraint";
constexpr char kKeySamples[] = "samples";
constexpr char kKeySwapSectorsRead[] = "swap_sectors_read";
constexpr char kKeySwapSectorsWrite[] = "swap_sectors_write";
constexpr char kKeySwapWaitingTimeMs[] = "swap_waiting_time_ms";
constexpr char kKeyTimestamp[] = "timestamp";

}  // namespace

struct ArcSystemStatCollector::Sample {
  base::TimeTicks timestamp;
  int swap_sectors_read = 0;
  int swap_sectors_write = 0;
  int swap_waiting_time_ms = 0;
  int mem_total_kb = 0;
  int mem_used_kb = 0;
  int gem_objects = 0;
  int gem_size_kb = 0;
  int cpu_temperature = std::numeric_limits<int>::min();
  int cpu_frequency = 0;
  // Power in milli-watts.
  int cpu_power = 0;
  int gpu_power = 0;
  int memory_power = 0;
  // Constraint in milli-watts.
  int package_power_constraint = 0;
};

struct OneValueReaderInfo {
  SystemReader reader = SystemReader::kTotal;
  raw_ptr<int64_t> value = nullptr;
  int64_t default_value = 0;
};

struct ArcSystemStatCollector::SystemReadersContext {
  // Initializes |SystemReadersContext| for Intel power counters. Must be called
  // on background thread.
  static void InitIntelPowerOnBackgroundThread(SystemReadersContext* context) {
    // Power counters for Intel platforms.
    const base::FilePath powercap_path(kPowercapPath);
    if (!base::PathExists(powercap_path)) {
      LOG(WARNING) << "There are no power counters for this board";
      return;
    }

    base::FileEnumerator dirs(powercap_path, false /* recursive */,
                              base::FileEnumerator::DIRECTORIES,
                              kIntelRaplQuery);

    for (base::FilePath dir = dirs.Next(); !dir.empty(); dir = dirs.Next()) {
      const base::FilePath domain_file_path = dir.Append(kNamePath);

      std::string domain_name;
      if (!base::PathExists(domain_file_path) ||
          !base::ReadFileToString(domain_file_path, &domain_name)) {
        LOG(ERROR) << "Unable to get power counter name in "
                   << domain_file_path.value();
        continue;
      }

      SystemReader reader;
      base::FilePath component;
      base::TrimWhitespaceASCII(domain_name, base::TRIM_ALL, &domain_name);
      if (domain_name == kPackagePowerDomainName) {
        reader = kPackagePowerConstraint;
        component = base::FilePath(kLongTermConstraintPath);
      } else if (domain_name == kCpuPowerDomainName) {
        reader = kCpuEnergy;
        component = base::FilePath(kEnergyPath);
      } else if (domain_name == kGpuPowerDomainName) {
        reader = kGpuEnergy;
        component = base::FilePath(kEnergyPath);
      } else if (domain_name == kMemoryPowerDomainName) {
        reader = kMemoryEnergy;
        component = base::FilePath(kEnergyPath);
      } else {
        LOG(WARNING) << "Ignore power counter " << domain_name << " in "
                     << domain_file_path.value();
        continue;
      }

      if (context->system_readers[reader].is_valid()) {
        LOG(ERROR) << "Found duplicate power counter " << domain_name << " in "
                   << domain_file_path.value();
        continue;
      }

      const base::FilePath counter_file_path = dir.Append(component);
      context->system_readers[reader].reset(
          open(counter_file_path.value().c_str(), O_RDONLY));
      if (!context->system_readers[reader].is_valid()) {
        // TODO(b/182801299): Some intel-rapl files may not be opened from user
        // process by design. Add support to access through debugd as root.
        LOG(ERROR) << "Failed to open power counter: " << domain_name << " as "
                   << counter_file_path.value();
      }
    }
  }
  // Creates and initializes |SystemReadersContext|. Must be called on
  // background thread.
  static std::unique_ptr<SystemReadersContext> InitOnBackgroundThread() {
    std::unique_ptr<SystemReadersContext> context =
        std::make_unique<SystemReadersContext>();

    context->system_readers[SystemReader::kZram].reset(
        open(kZramPath, O_RDONLY));
    if (!context->system_readers[SystemReader::kZram].is_valid()) {
      LOG(ERROR) << "Failed to open zram stat file: " << kZramPath;
    }

    context->system_readers[SystemReader::kMemoryInfo].reset(
        open(kMemoryInfoPath, O_RDONLY));
    if (!context->system_readers[SystemReader::kMemoryInfo].is_valid()) {
      LOG(ERROR) << "Failed to open mem info file: " << kMemoryInfoPath;
    }

    // Reading i915_gem_objects on Intel platform with kernel 5.4 is slow and is
    // prohibited. Also it changes reporting format.
    // TODO(b/170397975): Update if i915_gem_objects reading time is improved.
    const bool is_newer_kernel =
        base::StartsWith(base::SysInfo::KernelVersion(), "5.");
    const bool is_intel_cpu = base::CPU().vendor_name() == "GenuineIntel";

    if (!is_newer_kernel || !is_intel_cpu) {
      context->system_readers[SystemReader::kGemInfo].reset(
          open(kGemInfoPath, O_RDONLY));
      if (!context->system_readers[SystemReader::kGemInfo].is_valid()) {
        LOG(ERROR) << "Failed to open gem info file: " << kGemInfoPath;
      }
    } else {
      LOG(ERROR) << "Reading gem info from: " << kGemInfoPath
                 << " is disabled.";
    }

    const base::FilePath& cpu_temp_path = GetCpuTemperaturePathOnFileThread();
    context->system_readers[SystemReader::kCpuTemperature].reset(
        open(cpu_temp_path.value().c_str(), O_RDONLY));
    if (!context->system_readers[SystemReader::kCpuTemperature].is_valid()) {
      LOG(ERROR) << "Failed to open cpu temperature file: "
                 << cpu_temp_path.value();
    }

    context->system_readers[SystemReader::kCpuFrequency].reset(
        open(kCpuFrequencyPath, O_RDONLY));
    if (!context->system_readers[SystemReader::kCpuFrequency].is_valid()) {
      LOG(ERROR) << "Failed to open cpu frequency file: " << kCpuFrequencyPath;
    }

    InitIntelPowerOnBackgroundThread(context.get());

    return context;
  }

  // Releases |context|. Must be called on background thread.
  static void FreeOnBackgroundThread(
      std::unique_ptr<ArcSystemStatCollector::SystemReadersContext> context) {
    DCHECK(context);
    context.reset();
  }

  base::ScopedFD system_readers[SystemReader::kTotal];
  RuntimeFrame current_frame;
};

// static
constexpr int ArcSystemStatCollector::kZramStatColumns[];

// static
constexpr int ArcSystemStatCollector::kMemInfoColumns[];

// static
constexpr int ArcSystemStatCollector::kGemInfoColumns[];

// static
constexpr int ArcSystemStatCollector::kOneValueColumns[];

ArcSystemStatCollector::ArcSystemStatCollector() {}

ArcSystemStatCollector::~ArcSystemStatCollector() {
  FreeSystemReadersContext();
}

void ArcSystemStatCollector::Start(const base::TimeDelta& max_interval) {
  max_interval_ = max_interval;
  const size_t sample_count =
      1 + max_interval.InMicroseconds() /
              kSystemStatUpdateInterval.InMicroseconds();
  samples_.resize(sample_count);
  write_index_ = 0;
  // Maximum 10 warning per session.
  missed_update_warning_left_ = 10;

  background_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&SystemReadersContext::InitOnBackgroundThread),
      base::BindOnce(&ArcSystemStatCollector::OnInitOnUiThread,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcSystemStatCollector::Stop() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  background_task_runner_.reset();
  timer_.Stop();
  FreeSystemReadersContext();
}

void ArcSystemStatCollector::Flush(const base::TimeTicks& min_timestamp,
                                   const base::TimeTicks& max_timestamp,
                                   ArcSystemModel* system_model) {
  DCHECK(!timer_.IsRunning());
  size_t sample_index =
      write_index_ >= samples_.size() ? write_index_ - samples_.size() : 0;
  ArcValueEventTrimmer mem_total(&system_model->memory_events(),
                                 ArcValueEvent::Type::kMemTotal);
  ArcValueEventTrimmer mem_used(&system_model->memory_events(),
                                ArcValueEvent::Type::kMemUsed);
  ArcValueEventTrimmer gem_objects(&system_model->memory_events(),
                                   ArcValueEvent::Type::kGemObjects);
  ArcValueEventTrimmer gem_size(&system_model->memory_events(),
                                ArcValueEvent::Type::kGemSize);
  ArcValueEventTrimmer swap_read(&system_model->memory_events(),
                                 ArcValueEvent::Type::kSwapRead);
  ArcValueEventTrimmer swap_write(&system_model->memory_events(),
                                  ArcValueEvent::Type::kSwapWrite);
  ArcValueEventTrimmer swap_wait(&system_model->memory_events(),
                                 ArcValueEvent::Type::kSwapWait);
  ArcValueEventTrimmer cpu_temperature(&system_model->memory_events(),
                                       ArcValueEvent::Type::kCpuTemperature);
  ArcValueEventTrimmer cpu_frequency(&system_model->memory_events(),
                                     ArcValueEvent::Type::kCpuFrequency);
  ArcValueEventTrimmer package_power_constraint(
      &system_model->memory_events(),
      ArcValueEvent::Type::kPackagePowerConstraint);
  ArcValueEventTrimmer cpu_power(&system_model->memory_events(),
                                 ArcValueEvent::Type::kCpuPower);
  ArcValueEventTrimmer gpu_power(&system_model->memory_events(),
                                 ArcValueEvent::Type::kGpuPower);
  ArcValueEventTrimmer memory_power(&system_model->memory_events(),
                                    ArcValueEvent::Type::kMemoryPower);

  while (sample_index < write_index_) {
    const Sample& sample = samples_[sample_index % samples_.size()];
    ++sample_index;
    if (sample.timestamp > max_timestamp) {
      break;
    }
    if (sample.timestamp < min_timestamp) {
      continue;
    }
    const int64_t timestamp =
        (sample.timestamp - base::TimeTicks()).InMicroseconds();
    mem_total.MaybeAdd(timestamp, sample.mem_total_kb);
    mem_used.MaybeAdd(timestamp, sample.mem_used_kb);
    gem_objects.MaybeAdd(timestamp, sample.gem_objects);
    gem_size.MaybeAdd(timestamp, sample.gem_size_kb);
    swap_read.MaybeAdd(timestamp, sample.swap_sectors_read);
    swap_write.MaybeAdd(timestamp, sample.swap_sectors_write);
    swap_wait.MaybeAdd(timestamp, sample.swap_waiting_time_ms);
    if (sample.cpu_temperature > std::numeric_limits<int>::min()) {
      cpu_temperature.MaybeAdd(timestamp, sample.cpu_temperature);
    }
    if (sample.cpu_frequency > 0) {
      cpu_frequency.MaybeAdd(timestamp, sample.cpu_frequency);
    }
    if (sample.package_power_constraint > 0) {
      package_power_constraint.MaybeAdd(timestamp,
                                        sample.package_power_constraint);
    }
    if (sample.cpu_power > 0) {
      cpu_power.MaybeAdd(timestamp, sample.cpu_power);
    }
    if (sample.gpu_power > 0) {
      gpu_power.MaybeAdd(timestamp, sample.gpu_power);
    }
    if (sample.memory_power > 0) {
      memory_power.MaybeAdd(timestamp, sample.memory_power);
    }
  }

  // These are optional. Keep it if non-zero value is detected.
  package_power_constraint.ResetIfConstant(0);
  cpu_power.ResetIfConstant(0);
  gpu_power.ResetIfConstant(0);
  memory_power.ResetIfConstant(0);

  // Trimmer may break time sequence for events of different types. However
  // time sequence of events of the same type should be preserved.
  std::sort(system_model->memory_events().begin(),
            system_model->memory_events().end(),
            [](const auto& lhs, const auto& rhs) {
              return lhs.timestamp < rhs.timestamp;
            });
}

// Serializes the model to |base::Value|, this can be passed to
// javascript for rendering.
std::unique_ptr<base::Value> ArcSystemStatCollector::Serialize() const {
  base::Value::Dict root;

  root.Set(kKeyMaxInterval,
           base::NumberToString(max_interval_.InMicroseconds()));

  // Samples
  base::Value::List sample_list;
  for (const auto& sample : samples_) {
    base::Value::Dict sample_value;

    sample_value.Set(
        kKeyTimestamp,
        base::NumberToString(
            (sample.timestamp - base::TimeTicks()).InMicroseconds()));
    sample_value.Set(kKeySwapSectorsRead, sample.swap_sectors_read);
    sample_value.Set(kKeySwapSectorsWrite, sample.swap_sectors_write);
    sample_value.Set(kKeySwapWaitingTimeMs, sample.swap_waiting_time_ms);
    sample_value.Set(kKeyMemTotalKb, sample.mem_total_kb);
    sample_value.Set(kKeyMemUsedKb, sample.mem_used_kb);
    sample_value.Set(kKeyGemObjects, sample.gem_objects);
    sample_value.Set(kKeyGemSizeKb, sample.gem_size_kb);
    sample_value.Set(kKeyCpuTemperature, sample.cpu_temperature);
    sample_value.Set(kKeyCpuFrequency, sample.cpu_frequency);
    sample_value.Set(kKeyCpuPower, sample.cpu_power);
    sample_value.Set(kKeyGpuPower, sample.gpu_power);
    sample_value.Set(kKeyMemoryPower, sample.memory_power);
    sample_value.Set(kKeyPackagePowerConstraint,
                     sample.package_power_constraint);

    sample_list.Append(std::move(sample_value));
  }
  root.Set(kKeySamples, std::move(sample_list));

  return std::make_unique<base::Value>(std::move(root));
}

std::string ArcSystemStatCollector::SerializeToJson() const {
  std::unique_ptr<base::Value> root = Serialize();
  DCHECK(root);
  std::string output;
  if (!base::JSONWriter::WriteWithOptions(
          *root, base::JSONWriter::OPTIONS_PRETTY_PRINT, &output)) {
    LOG(ERROR) << "Failed to serialize system collector";
  }
  return output;
}

bool ArcSystemStatCollector::LoadFromJson(const std::string& json_data) {
  const std::optional<base::Value> root = base::JSONReader::Read(json_data);
  if (!root) {
    return false;
  }
  return LoadFromValue(*root);
}

bool ArcSystemStatCollector::LoadFromValue(const base::Value& root) {
  samples_.clear();
  const base::Value::Dict& root_dict = root.GetDict();

  int64_t max_interval_mcs;
  const std::string* max_interval = root_dict.FindString(kKeyMaxInterval);
  if (!max_interval || !base::StringToInt64(*max_interval, &max_interval_mcs)) {
    return false;
  }

  max_interval_ = base::Microseconds(max_interval_mcs);

  const base::Value::List* sample_list = root_dict.FindList(kKeySamples);
  if (!sample_list) {
    return false;
  }

  for (const auto& sample_entry : *sample_list) {
    const base::Value::Dict* sample_entry_dict = sample_entry.GetIfDict();
    if (!sample_entry_dict) {
      return false;
    }

    Sample sample;
    int64_t timestamp_mcs;
    const std::string* timestamp = sample_entry_dict->FindString(kKeyTimestamp);
    if (!timestamp || !base::StringToInt64(*timestamp, &timestamp_mcs)) {
      return false;
    }

    sample.timestamp = base::TimeTicks() + base::Microseconds(timestamp_mcs);

    if (!ReadNonNegativeInt(*sample_entry_dict, kKeySwapSectorsRead,
                            &sample.swap_sectors_read) ||
        !ReadNonNegativeInt(*sample_entry_dict, kKeySwapSectorsWrite,
                            &sample.swap_sectors_write) ||
        !ReadNonNegativeInt(*sample_entry_dict, kKeySwapWaitingTimeMs,
                            &sample.swap_waiting_time_ms) ||
        !ReadNonNegativeInt(*sample_entry_dict, kKeyMemTotalKb,
                            &sample.mem_total_kb) ||
        !ReadNonNegativeInt(*sample_entry_dict, kKeyMemUsedKb,
                            &sample.mem_used_kb) ||
        !ReadNonNegativeInt(*sample_entry_dict, kKeyGemObjects,
                            &sample.gem_objects) ||
        !ReadNonNegativeInt(*sample_entry_dict, kKeyGemSizeKb,
                            &sample.gem_size_kb) ||
        !ReadNonNegativeInt(*sample_entry_dict, kKeyCpuTemperature,
                            &sample.cpu_temperature) ||
        !ReadNonNegativeInt(*sample_entry_dict, kKeyCpuFrequency,
                            &sample.cpu_frequency) ||
        !ReadNonNegativeInt(*sample_entry_dict, kKeyCpuPower,
                            &sample.cpu_power) ||
        !ReadNonNegativeInt(*sample_entry_dict, kKeyGpuPower,
                            &sample.gpu_power) ||
        !ReadNonNegativeInt(*sample_entry_dict, kKeyMemoryPower,
                            &sample.memory_power) ||
        !ReadNonNegativeInt(*sample_entry_dict, kKeyPackagePowerConstraint,
                            &sample.package_power_constraint)) {
      return false;
    }
    samples_.emplace_back(sample);
  }

  return true;
}

void ArcSystemStatCollector::ScheduleSystemStatUpdate() {
  if (!context_) {
    if (missed_update_warning_left_-- > 0) {
      LOG(WARNING) << "Dropping update, already pending";
    }
    return;
  }
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ArcSystemStatCollector::ReadSystemStatOnBackgroundThread,
                     std::move(context_)),
      base::BindOnce(&ArcSystemStatCollector::UpdateSystemStatOnUiThread,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcSystemStatCollector::FreeSystemReadersContext() {
  if (!context_) {
    return;
  }
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&SystemReadersContext::FreeOnBackgroundThread,
                     std::move(context_)));
}

void ArcSystemStatCollector::OnInitOnUiThread(
    std::unique_ptr<ArcSystemStatCollector::SystemReadersContext> context) {
  DCHECK(!context_ && context);
  context_ = std::move(context);

  timer_.Start(
      FROM_HERE, kSystemStatUpdateInterval,
      base::BindRepeating(&ArcSystemStatCollector::ScheduleSystemStatUpdate,
                          base::Unretained(this)));
}

// static
std::unique_ptr<ArcSystemStatCollector::SystemReadersContext>
ArcSystemStatCollector::ReadSystemStatOnBackgroundThread(
    std::unique_ptr<SystemReadersContext> context) {
  DCHECK(context);
  context->current_frame.timestamp = base::TimeTicks::Now();
  if (!context->system_readers[SystemReader::kZram].is_valid() ||
      !ParseStatFile(context->system_readers[SystemReader::kZram].get(),
                     kZramStatColumns, context->current_frame.zram_stat)) {
    memset(context->current_frame.zram_stat, 0,
           sizeof(context->current_frame.zram_stat));
    static bool error_reported = false;
    if (!error_reported) {
      LOG(ERROR) << "Failed to read zram stat file: " << kZramPath;
      error_reported = true;
    }
  }

  if (!context->system_readers[SystemReader::kMemoryInfo].is_valid() ||
      !ParseStatFile(context->system_readers[SystemReader::kMemoryInfo].get(),
                     kMemInfoColumns, context->current_frame.mem_info)) {
    memset(context->current_frame.mem_info, 0,
           sizeof(context->current_frame.mem_info));
    static bool error_reported = false;
    if (!error_reported) {
      LOG(ERROR) << "Failed to read mem info file: " << kMemoryInfoPath;
      error_reported = true;
    }
  }

  if (!context->system_readers[SystemReader::kGemInfo].is_valid() ||
      !ParseStatFile(context->system_readers[SystemReader::kGemInfo].get(),
                     kGemInfoColumns, context->current_frame.gem_info)) {
    memset(context->current_frame.gem_info, 0,
           sizeof(context->current_frame.gem_info));
    static bool error_reported = false;
    if (!error_reported) {
      LOG(ERROR) << "Failed to read gem info file: " << kGemInfoPath;
      error_reported = true;
    }
  }

  OneValueReaderInfo one_value_readers[] = {
      {SystemReader::kCpuTemperature, &context->current_frame.cpu_temperature,
       std::numeric_limits<int>::min()},
      {SystemReader::kCpuFrequency, &context->current_frame.cpu_frequency, 0},
      {SystemReader::kPackagePowerConstraint,
       &context->current_frame.package_power_constraint, 0},
      {SystemReader::kCpuEnergy, &context->current_frame.cpu_energy, 0},
      {SystemReader::kGpuEnergy, &context->current_frame.gpu_energy, 0},
      {SystemReader::kMemoryEnergy, &context->current_frame.memory_energy, 0},
  };

  static bool one_value_readers_error_reported[std::size(one_value_readers)] = {
      false};

  for (size_t i = 0; i < std::size(one_value_readers); ++i) {
    if (!context->system_readers[one_value_readers[i].reader].is_valid() ||
        !ParseStatFile(
            context->system_readers[one_value_readers[i].reader].get(),
            kOneValueColumns, one_value_readers[i].value)) {
      *one_value_readers[i].value = one_value_readers[i].default_value;
      if (one_value_readers_error_reported[i]) {
        continue;
      }
      LOG(ERROR) << "Failed to read one value system stat: "
                 << one_value_readers[i].reader;
      one_value_readers_error_reported[i] = true;
    }
  }

  return context;
}

void ArcSystemStatCollector::UpdateSystemStatOnUiThread(
    std::unique_ptr<SystemReadersContext> context) {
  DCHECK(!context_ && context);
  DCHECK(!samples_.empty());
  Sample& current_sample = samples_[write_index_ % samples_.size()];
  current_sample.timestamp = context->current_frame.timestamp;
  current_sample.mem_total_kb = context->current_frame.mem_info[0];
  // kTotal - available.
  current_sample.mem_used_kb =
      context->current_frame.mem_info[0] - context->current_frame.mem_info[1];
  current_sample.gem_objects = context->current_frame.gem_info[0];
  current_sample.gem_size_kb = context->current_frame.gem_info[1] / 1024;

  // We calculate delta, so ignore first update.
  if (write_index_) {
    DCHECK_GT(context->current_frame.timestamp, previous_frame_.timestamp);
    const double to_milli_watts_scale =
        0.001 / (context->current_frame.timestamp - previous_frame_.timestamp)
                    .InSecondsF();
    current_sample.swap_sectors_read =
        context->current_frame.zram_stat[0] - previous_frame_.zram_stat[0];
    current_sample.swap_sectors_write =
        context->current_frame.zram_stat[1] - previous_frame_.zram_stat[1];
    current_sample.swap_waiting_time_ms =
        context->current_frame.zram_stat[2] - previous_frame_.zram_stat[2];

    // Energy is in micro-joules, power is in milli-watts.
    current_sample.cpu_power = static_cast<int>(
        (context->current_frame.cpu_energy - previous_frame_.cpu_energy) *
        to_milli_watts_scale);
    current_sample.gpu_power = static_cast<int>(
        (context->current_frame.gpu_energy - previous_frame_.gpu_energy) *
        to_milli_watts_scale);
    current_sample.memory_power = static_cast<int>(
        (context->current_frame.memory_energy - previous_frame_.memory_energy) *
        to_milli_watts_scale);
    DCHECK_GE(current_sample.cpu_power, 0);
    DCHECK_GE(current_sample.gpu_power, 0);
    DCHECK_GE(current_sample.memory_power, 0);
  }
  current_sample.cpu_temperature = context->current_frame.cpu_temperature;
  current_sample.cpu_frequency = context->current_frame.cpu_frequency;
  current_sample.package_power_constraint =
      static_cast<int>(context->current_frame.package_power_constraint *
                       0.001 /* micro-watts to milli-watts */);
  DCHECK_GE(current_sample.package_power_constraint, 0);
  DCHECK_GE(current_sample.swap_sectors_read, 0);
  DCHECK_GE(current_sample.swap_sectors_write, 0);
  DCHECK_GE(current_sample.swap_waiting_time_ms, 0);
  DCHECK_GE(current_sample.mem_total_kb, 0);
  DCHECK_GE(current_sample.mem_used_kb, 0);
  previous_frame_ = context->current_frame;
  ++write_index_;

  context_ = std::move(context);
}

ArcSystemStatCollector::RuntimeFrame::RuntimeFrame() = default;

bool ParseStatFile(int fd, const int* columns, int64_t* output) {
  char buffer[128];
  if (lseek(fd, 0, SEEK_SET)) {
    return false;
  }
  const int read_bytes = read(fd, buffer, sizeof(buffer) - 1);
  if (read_bytes < 0) {
    return false;
  }
  buffer[read_bytes] = 0;
  int column_index = 0;
  const char* scan = buffer;
  while (true) {
    // Skip whitespace.
    while (IsWhitespace(*scan)) {
      ++scan;
    }
    if (*columns != column_index) {
      // Just skip this entry. It may be digits or text.
      while (!IsWhitespace(*scan)) {
        ++scan;
      }
    } else {
      int64_t value = 0;
      while (IsDigit(*scan)) {
        value = 10 * value + *scan - '0';
        ++scan;
      }
      *output++ = value;
      ++columns;
      if (*columns < 0) {
        return IsEnd(*scan);  // All columns are read.
      }
    }
    if (!IsWhitespace(*scan)) {
      return false;
    }
    ++column_index;
  }
}

}  // namespace arc

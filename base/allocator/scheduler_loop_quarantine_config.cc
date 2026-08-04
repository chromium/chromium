#include "base/check.h"
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string_view>
#include <vector>

#include "base/allocator/partition_alloc_features.h"
#include "base/allocator/scheduler_loop_quarantine_config.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/strcat.h"

namespace base::allocator {

namespace {
constexpr char kProcessTypeWildcardStr[] = "*";
// SchedulerLoopQuarantineBranchType string representation.
constexpr char kBranchTypeGlobalStr[] = "global";
constexpr char kBranchTypeThreadLocalDefaultStr[] = "*";
constexpr char kBranchTypeMainStr[] = "main";
constexpr char kBranchTypeIOStr[] = "io";
constexpr char kBranchTypeAdvancedMemorySafetyChecksStr[] = "amsc";
constexpr char kBranchTypeVizCompositorStr[] = "viz-compositor";
constexpr char kBranchTypeCompositorGpuStr[] = "compositor-gpu";

constexpr std::string_view GetSchedulerLoopQuarantineBranchTypeStr(
    SchedulerLoopQuarantineBranchType type) {
  switch (type) {
    case SchedulerLoopQuarantineBranchType::kGlobal:
      return kBranchTypeGlobalStr;
    case SchedulerLoopQuarantineBranchType::kThreadLocalDefault:
      return kBranchTypeThreadLocalDefaultStr;
    case SchedulerLoopQuarantineBranchType::kMain:
      return kBranchTypeMainStr;
    case SchedulerLoopQuarantineBranchType::kIO:
      return kBranchTypeIOStr;
    case SchedulerLoopQuarantineBranchType::kAdvancedMemorySafetyChecks:
      return kBranchTypeAdvancedMemorySafetyChecksStr;
    case SchedulerLoopQuarantineBranchType::kVizCompositor:
      return kBranchTypeVizCompositorStr;
    case SchedulerLoopQuarantineBranchType::kCompositorGpu:
      return kBranchTypeCompositorGpuStr;
  }
  NOTREACHED();
}

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// JSON parsing options.
constexpr int kJSONParserOptions =
    JSONParserOptions::JSON_PARSE_CHROMIUM_EXTENSIONS |
    JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS;

// JSON keys for parameters.
constexpr char kKeyEnableQuarantine[] = "enable-quarantine";
constexpr char kKeyEnableZapping[] = "enable-zapping";
constexpr char kKeyLeakOnDestruction[] = "leak-on-destruction";
constexpr char kKeyEnableTaskControlledPurge[] = "enable-task-controlled-purge";
constexpr char kKeyPauseInBetweenTasks[] = "pause-in-between-tasks";
constexpr char kKeyBranchCapacityInBytes[] = "branch-capacity-in-bytes";
constexpr char kKeyMaxQuarantineSize[] = "max-quarantine-size";

struct Match {
  const DictValue* dict;
  size_t length;
};

std::optional<DictValue> CreateConfigProcessesDict() {
  std::string config_str;

  // This is called after switches initialization.
  CHECK(base::CommandLine::InitializedForCurrentProcess());
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          switches::kPartitionAllocSchedulerLoopQuarantine)) {
    config_str = command_line->GetSwitchValueASCII(
        switches::kPartitionAllocSchedulerLoopQuarantine);
  } else {
    if (!FeatureList::IsEnabled(
            features::kPartitionAllocSchedulerLoopQuarantine)) {
      return std::nullopt;  // Feature disabled.
    }
    config_str = features::kPartitionAllocSchedulerLoopQuarantineConfig.Get();
  }

  std::optional<DictValue> config_processes =
      JSONReader::ReadDict(config_str, kJSONParserOptions);
  if (!config_processes) {
    LOG(ERROR) << "Unparseable JSON: " << config_str;
    return std::nullopt;  // Ill-formed JSON; disabled.
  }
  return config_processes;
}

std::optional<DictValue>& GetConfigProcessesDict() {
  static base::NoDestructor<std::optional<DictValue>> config_processes(
      CreateConfigProcessesDict());
  return *config_processes;
}

std::vector<Match> GetMatchingProcessConfigs(
    const DictValue& config_processes,
    std::string_view process_type_identifier) {
  std::vector<Match> matches;

  for (auto [key, value] : config_processes) {
    if (!value.is_dict()) {
      LOG(ERROR) << "Non-dict value for key: " << key;
      continue;
    }
    if (key.find(kProcessTypeWildcardStr) < key.length() - 1) {
      LOG(ERROR) << "Wildcard '*' must be at the end of the process type: "
                 << key;
      continue;
    }

    const DictValue& dict_value = value.GetDict();

    if (key == process_type_identifier) {
      matches.push_back({&dict_value, key.length()});
    } else if (key.ends_with(kProcessTypeWildcardStr)) {
      std::string_view prefix =
          std::string_view(key).substr(0, key.length() - 1);
      if (process_type_identifier.starts_with(prefix)) {
        matches.push_back({&dict_value, prefix.length()});
      }
    }
  }

  // Sort matches by length, descending.
  std::sort(matches.begin(), matches.end(),
            [](const Match& a, const Match& b) { return a.length > b.length; });
  return matches;
}
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}  // namespace

::partition_alloc::internal::SchedulerLoopQuarantineConfig
GetSchedulerLoopQuarantineConfiguration(
    std::string_view process_type_identifier,
    SchedulerLoopQuarantineBranchType branch_type) {
  ::partition_alloc::internal::SchedulerLoopQuarantineConfig config = {};

  // Should not be a special name.
  DCHECK_NE(process_type_identifier, kProcessTypeWildcardStr);

  std::string_view branch_type_str =
      GetSchedulerLoopQuarantineBranchTypeStr(branch_type);

  std::string process_name(process_type_identifier);
  constexpr size_t kMaxBranchNameLen = sizeof(config.branch_name) - 1;
  if (process_name.length() + 1 + branch_type_str.length() >
      kMaxBranchNameLen) {
    size_t max_process_name_len =
        kMaxBranchNameLen - 1 - branch_type_str.length();
    // Use ".." as a separator.
    size_t side_len = (max_process_name_len - 2) / 2;
    process_name =
        base::StrCat({process_type_identifier.substr(0, side_len), "..",
                      process_type_identifier.substr(
                          process_type_identifier.length() - side_len)});
  }

  // Set a branch name like "browser/main" or "renderer/*";
  std::string branch_name = base::StrCat({process_name, "/", branch_type_str});
  // `std::string::copy` does not append a null character.
  branch_name.copy(config.branch_name, kMaxBranchNameLen);
  config.branch_name[kMaxBranchNameLen] = '\0';

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  const std::optional<DictValue>& config_processes = GetConfigProcessesDict();
  if (!config_processes) {
    return config;
  }
  std::vector<Match> matches =
      GetMatchingProcessConfigs(*config_processes, process_type_identifier);
  const DictValue* config_entry = nullptr;
  for (const auto& match : matches) {
    const DictValue* config_process = match.dict;
    config_entry = config_process->FindDict(branch_type_str);

    // Falls back to thread-local default unless global nor AMSC.
    if (!config_entry &&
        branch_type != SchedulerLoopQuarantineBranchType::kGlobal &&
        branch_type !=
            SchedulerLoopQuarantineBranchType::kAdvancedMemorySafetyChecks) {
      config_entry = config_process->FindDict(kBranchTypeThreadLocalDefaultStr);
    }

    if (config_entry) {
      break;
    }
  }

  if (!config_entry) {
    VLOG(1) << "No entry found for " << branch_name << ".";
    return config;  // No config found; disabled.
  }

  config.enable_quarantine = config_entry->FindBool(kKeyEnableQuarantine)
                                 .value_or(config.enable_quarantine);
  config.enable_zapping =
      config_entry->FindBool(kKeyEnableZapping).value_or(config.enable_zapping);
  config.leak_on_destruction = config_entry->FindBool(kKeyLeakOnDestruction)
                                   .value_or(config.leak_on_destruction);
  config.enable_task_controlled_purge =
      config_entry->FindBool(kKeyEnableTaskControlledPurge)
          .value_or(config.enable_task_controlled_purge);
  config.pause_in_between_tasks =
      config_entry->FindBool(kKeyPauseInBetweenTasks)
          .value_or(config.pause_in_between_tasks);
  config.branch_capacity_in_bytes =
      static_cast<size_t>(config_entry->FindInt(kKeyBranchCapacityInBytes)
                              .value_or(config.branch_capacity_in_bytes));

  int max_quarantine_size =
      config_entry->FindInt(kKeyMaxQuarantineSize).value_or(-1);
  if (0 < max_quarantine_size) {
    config.max_quarantine_size = static_cast<size_t>(max_quarantine_size);
  }
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  return config;
}

bool HasMiracleObject(std::string_view process_type_identifier) {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  const std::optional<DictValue>& config_processes = GetConfigProcessesDict();
  if (!config_processes) {
    return false;
  }
  std::vector<Match> matches =
      GetMatchingProcessConfigs(*config_processes, process_type_identifier);
  for (const auto& match : matches) {
    const DictValue* config_process = match.dict;
    for (auto [branch_key, branch_val] : *config_process) {
      if (branch_key == kBranchTypeAdvancedMemorySafetyChecksStr) {
        continue;
      }
      if (!branch_val.is_dict()) {
        continue;
      }
      const DictValue& branch_dict = branch_val.GetDict();
      if (branch_dict.FindBool(kKeyEnableQuarantine).value_or(false)) {
        return true;
      }
    }
  }
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  return false;
}

bool HasSchedulerLoopQuarantineTaskControl(
    std::string_view process_type_identifier) {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  const std::optional<DictValue>& config_processes = GetConfigProcessesDict();
  if (!config_processes) {
    return false;
  }
  std::vector<Match> matches =
      GetMatchingProcessConfigs(*config_processes, process_type_identifier);
  for (const auto& match : matches) {
    const DictValue* config_process = match.dict;
    for (auto [branch_key, branch_val] : *config_process) {
      if (!branch_val.is_dict()) {
        continue;
      }
      const DictValue& branch_dict = branch_val.GetDict();
      if (branch_dict.FindBool(kKeyEnableTaskControlledPurge).value_or(false) ||
          branch_dict.FindBool(kKeyPauseInBetweenTasks).value_or(false)) {
        return true;
      }
    }
  }
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  return false;
}

void ResetSchedulerLoopQuarantineConfigForTesting() {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  GetConfigProcessesDict() = CreateConfigProcessesDict();
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

}  // namespace base::allocator

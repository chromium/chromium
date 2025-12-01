// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/scheduler_loop_quarantine_config.h"

#include <string_view>

#include "base/allocator/partition_alloc_features.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/strcat.h"

namespace base::allocator {

namespace {
// For configuration purpose use "browser" instead of "" for visibility.
constexpr char kProcessTypeBrowserStr[] = "browser";
constexpr char kProcessTypeWildcardStr[] = "*";
// SchedulerLoopQuarantineBranchType string representation.
constexpr char kBranchTypeGlobalStr[] = "global";
constexpr char kBranchTypeThreadLocalDefaultStr[] = "*";
constexpr char kBranchTypeMainStr[] = "main";
constexpr char kBranchTypeIOStr[] = "io";
constexpr char kBranchTypeAdvancedMemorySafetyChecksStr[] = "amsc";

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
constexpr char kKeyBranchCapacityInBytes[] = "branch-capacity-in-bytes";
constexpr char kKeyMaxQuarantineSize[] = "max-quarantine-size";
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}  // namespace

::partition_alloc::internal::SchedulerLoopQuarantineConfig
GetSchedulerLoopQuarantineConfiguration(
    const std::string& process_type,
    SchedulerLoopQuarantineBranchType branch_type) {
  ::partition_alloc::internal::SchedulerLoopQuarantineConfig config = {};

  std::string_view process_type_str = process_type;
  if (process_type_str.empty()) {
    process_type_str = kProcessTypeBrowserStr;
  }
  // Should not be a special name.
  DCHECK_NE(process_type_str, kProcessTypeWildcardStr);

  std::string_view branch_type_str =
      GetSchedulerLoopQuarantineBranchTypeStr(branch_type);

  // Set a branch name like "browser/main" or "renderer/*";
  std::string branch_name =
      base::StrCat({process_type_str, "/", branch_type_str});
  // `std::string::copy` does not append a null character.
  branch_name.copy(config.branch_name, sizeof(config.branch_name) - 1);
  config.branch_name[sizeof(config.branch_name) - 1] = '\0';

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  if (!FeatureList::IsEnabled(
          features::kPartitionAllocSchedulerLoopQuarantine)) {
    return config;  // Feature disabled.
  }

  // TODO(https://crbug.com/434693933): Also read from command-line switches
  // to support an enterprise policy. It is loaded after PA configuration in
  // child processes so we should pass it from the Browser process via switches.
  std::string config_str =
      features::kPartitionAllocSchedulerLoopQuarantineConfig.Get();

  std::optional<Value::Dict> config_processes =
      JSONReader::ReadDict(config_str, kJSONParserOptions);
  if (!config_processes) {
    LOG(ERROR) << "Unparseable JSON: " << config_str;
    return config;  // Ill-formed JSON; disabled.
  }

  const Value::Dict* config_entry = nullptr;

  const Value::Dict* config_current_process =
      config_processes->FindDict(process_type_str);
  if (config_current_process) {
    // First, try the exact match.
    config_entry = config_current_process->FindDict(branch_type_str);

    // Falls back to thread-local default unless global.
    if (!config_entry &&
        branch_type != SchedulerLoopQuarantineBranchType::kGlobal &&
        branch_type !=
            SchedulerLoopQuarantineBranchType::kAdvancedMemorySafetyChecks) {
      config_entry =
          config_current_process->FindDict(kBranchTypeThreadLocalDefaultStr);
    }
  }

  Value::Dict* config_wildcard_process =
      config_processes->FindDict(kProcessTypeWildcardStr);
  if (!config_entry && config_wildcard_process) {
    // Couldn't find a configuration entry with the exact process name match.
    // Look-up an entry with a process name being "*".
    config_entry = config_wildcard_process->FindDict(branch_type_str);

    // Falls back to thread-local default unless global.
    if (!config_entry &&
        branch_type != SchedulerLoopQuarantineBranchType::kGlobal) {
      config_entry =
          config_wildcard_process->FindDict(kBranchTypeThreadLocalDefaultStr);
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
}  // namespace base::allocator

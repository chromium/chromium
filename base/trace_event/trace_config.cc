// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_config.h"

#include <stddef.h>

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/trace_event.h"

#include "third_party/perfetto/protos/perfetto/config/track_event/track_event_config.gen.h"  // nogncheck

namespace base::trace_event {

namespace {

// String options that can be used to initialize TraceOptions.
const char kRecordUntilFull[] = "record-until-full";
const char kRecordContinuously[] = "record-continuously";
const char kRecordAsMuchAsPossible[] = "record-as-much-as-possible";
const char kTraceToConsole[] = "trace-to-console";
const char kEnableSystrace[] = "enable-systrace";
constexpr int kEnableSystraceLength = sizeof(kEnableSystrace) - 1;

const char kEnableArgumentFilter[] = "enable-argument-filter";

// String parameters that can be used to parse the trace config string.
const char kRecordModeParam[] = "record_mode";
const char kTraceBufferSizeInEvents[] = "trace_buffer_size_in_events";
const char kTraceBufferSizeInKb[] = "trace_buffer_size_in_kb";
const char kEnableSystraceParam[] = "enable_systrace";
const char kSystraceEventsParam[] = "enable_systrace_events";
const char kEnableArgumentFilterParam[] = "enable_argument_filter";
const char kEnableEventPackageNameFilterParam[] = "enable_package_name_filter";

// String parameters that is used to parse memory dump config in trace config
// string.
const char kMemoryDumpConfigParam[] = "memory_dump_config";
const char kAllowedDumpModesParam[] = "allowed_dump_modes";
const char kTriggersParam[] = "triggers";
const char kTriggerModeParam[] = "mode";
const char kMinTimeBetweenDumps[] = "min_time_between_dumps_ms";
const char kTriggerTypeParam[] = "type";
const char kPeriodicIntervalLegacyParam[] = "periodic_interval_ms";
const char kHeapProfilerOptions[] = "heap_profiler_options";
const char kBreakdownThresholdBytes[] = "breakdown_threshold_bytes";

// String parameters used to parse category event filters.
const char kEventFiltersParam[] = "event_filters";
const char kFilterPredicateParam[] = "filter_predicate";
const char kFilterArgsParam[] = "filter_args";

// String parameter used to parse process filter.
const char kIncludedProcessesParam[] = "included_process_ids";

const char kHistogramNamesParam[] = "histogram_names";

class ConvertableTraceConfigToTraceFormat
    : public base::trace_event::ConvertableToTraceFormat {
 public:
  explicit ConvertableTraceConfigToTraceFormat(const TraceConfig& trace_config)
      : trace_config_(trace_config) {}

  ~ConvertableTraceConfigToTraceFormat() override = default;

  void AppendAsTraceFormat(std::string* out) const override {
    out->append(trace_config_.ToString());
  }

 private:
  const TraceConfig trace_config_;
};

std::set<MemoryDumpLevelOfDetail> GetDefaultAllowedMemoryDumpModes() {
  std::set<MemoryDumpLevelOfDetail> all_modes;
  for (uint32_t mode = static_cast<uint32_t>(MemoryDumpLevelOfDetail::kFirst);
       mode <= static_cast<uint32_t>(MemoryDumpLevelOfDetail::kLast); mode++) {
    all_modes.insert(static_cast<MemoryDumpLevelOfDetail>(mode));
  }
  return all_modes;
}

}  // namespace

TraceConfig::MemoryDumpConfig::HeapProfiler::HeapProfiler()
    : breakdown_threshold_bytes(kDefaultBreakdownThresholdBytes) {}

void TraceConfig::MemoryDumpConfig::HeapProfiler::Clear() {
  breakdown_threshold_bytes = kDefaultBreakdownThresholdBytes;
}

void TraceConfig::ResetMemoryDumpConfig(
    const TraceConfig::MemoryDumpConfig& memory_dump_config) {
  memory_dump_config_.Clear();
  memory_dump_config_ = memory_dump_config;
}

TraceConfig::MemoryDumpConfig::MemoryDumpConfig() = default;

TraceConfig::MemoryDumpConfig::MemoryDumpConfig(
    const MemoryDumpConfig& other) = default;

TraceConfig::MemoryDumpConfig::~MemoryDumpConfig() = default;

void TraceConfig::MemoryDumpConfig::Clear() {
  allowed_dump_modes.clear();
  triggers.clear();
  heap_profiler_options.Clear();
}

void TraceConfig::MemoryDumpConfig::Merge(
    const TraceConfig::MemoryDumpConfig& config) {
  triggers.insert(triggers.end(), config.triggers.begin(),
                  config.triggers.end());
  allowed_dump_modes.insert(config.allowed_dump_modes.begin(),
                            config.allowed_dump_modes.end());
  heap_profiler_options.breakdown_threshold_bytes =
      std::min(heap_profiler_options.breakdown_threshold_bytes,
               config.heap_profiler_options.breakdown_threshold_bytes);
}

TraceConfig::ProcessFilterConfig::ProcessFilterConfig() = default;

TraceConfig::ProcessFilterConfig::ProcessFilterConfig(
    const ProcessFilterConfig& other) = default;

TraceConfig::ProcessFilterConfig::ProcessFilterConfig(
    const std::unordered_set<base::ProcessId>& included_process_ids)
    : included_process_ids_(included_process_ids) {}

TraceConfig::ProcessFilterConfig::~ProcessFilterConfig() = default;

void TraceConfig::ProcessFilterConfig::Clear() {
  included_process_ids_.clear();
}

void TraceConfig::ProcessFilterConfig::Merge(
    const ProcessFilterConfig& config) {
  included_process_ids_.insert(config.included_process_ids_.begin(),
                               config.included_process_ids_.end());
}

void TraceConfig::ProcessFilterConfig::InitializeFromConfigDict(
    const Value::Dict& dict) {
  included_process_ids_.clear();
  const Value::List* value = dict.FindList(kIncludedProcessesParam);
  if (!value)
    return;
  for (auto& pid_value : *value) {
    if (pid_value.is_int()) {
      included_process_ids_.insert(
          static_cast<base::ProcessId>(pid_value.GetInt()));
    }
  }
}

void TraceConfig::ProcessFilterConfig::ToDict(Value::Dict& dict) const {
  if (included_process_ids_.empty())
    return;
  base::Value::List list;
  std::set<base::ProcessId> ordered_set(included_process_ids_.begin(),
                                        included_process_ids_.end());
  for (auto process_id : ordered_set)
    list.Append(static_cast<int>(process_id));
  dict.Set(kIncludedProcessesParam, std::move(list));
}

bool TraceConfig::ProcessFilterConfig::IsEnabled(
    base::ProcessId process_id) const {
  return included_process_ids_.empty() ||
         included_process_ids_.count(process_id);
}

TraceConfig::EventFilterConfig::EventFilterConfig(
    const std::string& predicate_name)
    : predicate_name_(predicate_name) {}

TraceConfig::EventFilterConfig::~EventFilterConfig() = default;

TraceConfig::EventFilterConfig::EventFilterConfig(const EventFilterConfig& tc) {
  *this = tc;
}

TraceConfig::EventFilterConfig& TraceConfig::EventFilterConfig::operator=(
    const TraceConfig::EventFilterConfig& rhs) {
  if (this == &rhs)
    return *this;

  predicate_name_ = rhs.predicate_name_;
  category_filter_ = rhs.category_filter_;

  args_ = rhs.args_.Clone();

  return *this;
}

bool TraceConfig::EventFilterConfig::IsEquivalentTo(
    const EventFilterConfig& other) const {
  return predicate_name_ == other.predicate_name_ &&
         category_filter_.IsEquivalentTo(category_filter_) &&
         args_ == other.args_;
}

void TraceConfig::EventFilterConfig::InitializeFromConfigDict(
    const Value::Dict& event_filter) {
  category_filter_.InitializeFromConfigDict(event_filter);

  const Value::Dict* args_dict = event_filter.FindDict(kFilterArgsParam);
  if (args_dict)
    args_ = args_dict->Clone();
}

void TraceConfig::EventFilterConfig::SetCategoryFilter(
    const TraceConfigCategoryFilter& category_filter) {
  category_filter_ = category_filter;
}

void TraceConfig::EventFilterConfig::ToDict(Value::Dict& filter_dict) const {
  filter_dict.Set(kFilterPredicateParam, predicate_name());

  category_filter_.ToDict(filter_dict);

  if (!args_.empty()) {
    filter_dict.Set(kFilterArgsParam, args_.Clone());
  }
}

bool TraceConfig::EventFilterConfig::GetArgAsSet(
    const char* key,
    std::unordered_set<std::string>* out_set) const {
  const Value::List* list = args_.FindList(key);
  if (!list)
    return false;
  for (const Value& item : *list) {
    if (item.is_string())
      out_set->insert(item.GetString());
  }
  return true;
}

bool TraceConfig::EventFilterConfig::IsCategoryGroupEnabled(
    std::string_view category_group_name) const {
  return category_filter_.IsCategoryGroupEnabled(category_group_name);
}

// static
std::string TraceConfig::TraceRecordModeToStr(TraceRecordMode record_mode) {
  switch (record_mode) {
    case RECORD_UNTIL_FULL:
      return kRecordUntilFull;
    case RECORD_CONTINUOUSLY:
      return kRecordContinuously;
    case RECORD_AS_MUCH_AS_POSSIBLE:
      return kRecordAsMuchAsPossible;
    case ECHO_TO_CONSOLE:
      return kTraceToConsole;
  }
  return kRecordUntilFull;
}

TraceConfig::TraceConfig() {
  InitializeDefault();
}

TraceConfig::TraceConfig(std::string_view category_filter_string,
                         std::string_view trace_options_string) {
  InitializeFromStrings(category_filter_string, trace_options_string);
}

TraceConfig::TraceConfig(std::string_view category_filter_string,
                         TraceRecordMode record_mode) {
  InitializeFromStrings(category_filter_string,
                        TraceConfig::TraceRecordModeToStr(record_mode));
}

TraceConfig::TraceConfig(const Value::Dict& config) {
  InitializeFromConfigDict(config);
}

TraceConfig::TraceConfig(std::string_view config_string) {
  if (!config_string.empty())
    InitializeFromConfigString(config_string);
  else
    InitializeDefault();
}

TraceConfig::TraceConfig(const TraceConfig& tc) = default;

TraceConfig::~TraceConfig() = default;

TraceConfig& TraceConfig::operator=(const TraceConfig& rhs) {
  if (this == &rhs)
    return *this;

  record_mode_ = rhs.record_mode_;
  trace_buffer_size_in_events_ = rhs.trace_buffer_size_in_events_;
  trace_buffer_size_in_kb_ = rhs.trace_buffer_size_in_kb_;
  enable_systrace_ = rhs.enable_systrace_;
  enable_argument_filter_ = rhs.enable_argument_filter_;
  category_filter_ = rhs.category_filter_;
  process_filter_config_ = rhs.process_filter_config_;
  enable_event_package_name_filter_ = rhs.enable_event_package_name_filter_;
  memory_dump_config_ = rhs.memory_dump_config_;
  event_filters_ = rhs.event_filters_;
  histogram_names_ = rhs.histogram_names_;
  systrace_events_ = rhs.systrace_events_;
  return *this;
}

bool TraceConfig::IsEquivalentTo(const TraceConfig& other) const {
  if (enable_systrace_ != other.enable_systrace_ ||
      enable_argument_filter_ != other.enable_argument_filter_ ||
      enable_event_package_name_filter_ !=
          other.enable_event_package_name_filter_ ||
      histogram_names_ != other.histogram_names_ ||
      systrace_events_ != other.systrace_events_ ||
      process_filter_config_ != other.process_filter_config_ ||
      memory_dump_config_ != other.memory_dump_config_ ||
      !category_filter_.IsEquivalentTo(other.category_filter_)) {
    return false;
  }

  if (event_filters_.size() != other.event_filters_.size()) {
    return false;
  }
  for (const auto& filter : event_filters_) {
    bool equivalent_found = false;
    for (const auto& other_filter : other.event_filters_) {
      if (other_filter.IsEquivalentTo(filter)) {
        equivalent_found = true;
        break;
      }
    }
    if (!equivalent_found) {
      return false;
    }
  }

  return true;
}

std::string TraceConfig::ToString() const {
  Value dict = ToValue();
  std::string json;
  JSONWriter::Write(dict, &json);
  return json;
}

std::unique_ptr<ConvertableToTraceFormat>
TraceConfig::AsConvertableToTraceFormat() const {
  return std::make_unique<ConvertableTraceConfigToTraceFormat>(*this);
}

std::string TraceConfig::ToCategoryFilterString() const {
  return category_filter_.ToFilterString();
}

bool TraceConfig::IsCategoryGroupEnabled(
    std::string_view category_group_name) const {
  // TraceLog should call this method only as part of enabling/disabling
  // categories.
  return category_filter_.IsCategoryGroupEnabled(category_group_name);
}

void TraceConfig::Merge(const TraceConfig& config) {
  if (record_mode_ != config.record_mode_ ||
      enable_systrace_ != config.enable_systrace_ ||
      enable_argument_filter_ != config.enable_argument_filter_ ||
      enable_event_package_name_filter_ !=
          config.enable_event_package_name_filter_) {
    DLOG(ERROR) << "Attempting to merge trace config with a different "
                << "set of options.";
  }
  DCHECK_EQ(trace_buffer_size_in_events_, config.trace_buffer_size_in_events_)
      << "Cannot change trace buffer size";

  category_filter_.Merge(config.category_filter_);
  memory_dump_config_.Merge(config.memory_dump_config_);
  process_filter_config_.Merge(config.process_filter_config_);

  event_filters_.insert(event_filters_.end(), config.event_filters().begin(),
                        config.event_filters().end());
  histogram_names_.insert(config.histogram_names().begin(),
                          config.histogram_names().end());
}

void TraceConfig::Clear() {
  record_mode_ = RECORD_UNTIL_FULL;
  trace_buffer_size_in_events_ = 0;
  trace_buffer_size_in_kb_ = 0;
  enable_systrace_ = false;
  enable_argument_filter_ = false;
  enable_event_package_name_filter_ = false;
  category_filter_.Clear();
  memory_dump_config_.Clear();
  process_filter_config_.Clear();
  event_filters_.clear();
  histogram_names_.clear();
  systrace_events_.clear();
}

void TraceConfig::InitializeDefault() {
  record_mode_ = RECORD_UNTIL_FULL;
  trace_buffer_size_in_events_ = 0;
  trace_buffer_size_in_kb_ = 0;
  enable_systrace_ = false;
  enable_argument_filter_ = false;
  enable_event_package_name_filter_ = false;
}

void TraceConfig::InitializeFromConfigDict(const Value::Dict& dict) {
  record_mode_ = RECORD_UNTIL_FULL;
  const std::string* record_mode = dict.FindString(kRecordModeParam);
  if (record_mode) {
    if (*record_mode == kRecordUntilFull) {
      record_mode_ = RECORD_UNTIL_FULL;
    } else if (*record_mode == kRecordContinuously) {
      record_mode_ = RECORD_CONTINUOUSLY;
    } else if (*record_mode == kTraceToConsole) {
      record_mode_ = ECHO_TO_CONSOLE;
    } else if (*record_mode == kRecordAsMuchAsPossible) {
      record_mode_ = RECORD_AS_MUCH_AS_POSSIBLE;
    }
  }
  trace_buffer_size_in_events_ = base::saturated_cast<size_t>(
      dict.FindInt(kTraceBufferSizeInEvents).value_or(0));
  trace_buffer_size_in_kb_ = base::saturated_cast<size_t>(
      dict.FindInt(kTraceBufferSizeInKb).value_or(0));

  enable_systrace_ = dict.FindBool(kEnableSystraceParam).value_or(false);
  enable_argument_filter_ =
      dict.FindBool(kEnableArgumentFilterParam).value_or(false);
  enable_event_package_name_filter_ =
      dict.FindBool(kEnableEventPackageNameFilterParam).value_or(false);

  category_filter_.InitializeFromConfigDict(dict);
  process_filter_config_.InitializeFromConfigDict(dict);

  const Value::List* category_event_filters = dict.FindList(kEventFiltersParam);
  if (category_event_filters)
    SetEventFiltersFromConfigList(*category_event_filters);
  const Value::List* histogram_names = dict.FindList(kHistogramNamesParam);
  if (histogram_names)
    SetHistogramNamesFromConfigList(*histogram_names);

  if (category_filter_.IsCategoryEnabled(MemoryDumpManager::kTraceCategory)) {
    // If dump triggers not set, the client is using the legacy with just
    // category enabled. So, use the default periodic dump config.
    const Value::Dict* memory_dump_config =
        dict.FindDict(kMemoryDumpConfigParam);
    if (memory_dump_config)
      SetMemoryDumpConfigFromConfigDict(*memory_dump_config);
    else
      SetDefaultMemoryDumpConfig();
  }

  systrace_events_.clear();
  if (enable_systrace_) {
    const Value::List* systrace_events = dict.FindList(kSystraceEventsParam);
    if (systrace_events) {
      for (const Value& value : *systrace_events) {
        systrace_events_.insert(value.GetString());
      }
    }
  }
}

void TraceConfig::InitializeFromConfigString(std::string_view config_string) {
  std::optional<Value> dict = JSONReader::Read(config_string);
  if (dict && dict->is_dict())
    InitializeFromConfigDict(dict->GetDict());
  else
    InitializeDefault();
}

void TraceConfig::InitializeFromStrings(std::string_view category_filter_string,
                                        std::string_view trace_options_string) {
  if (!category_filter_string.empty())
    category_filter_.InitializeFromString(category_filter_string);

  record_mode_ = RECORD_UNTIL_FULL;
  trace_buffer_size_in_events_ = 0;
  trace_buffer_size_in_kb_ = 0;
  enable_systrace_ = false;
  systrace_events_.clear();
  enable_argument_filter_ = false;
  enable_event_package_name_filter_ = false;
  if (!trace_options_string.empty()) {
    std::vector<std::string> split =
        SplitString(trace_options_string, ",", TRIM_WHITESPACE, SPLIT_WANT_ALL);
    for (const std::string& token : split) {
      if (token == kRecordUntilFull) {
        record_mode_ = RECORD_UNTIL_FULL;
      } else if (token == kRecordContinuously) {
        record_mode_ = RECORD_CONTINUOUSLY;
      } else if (token == kTraceToConsole) {
        record_mode_ = ECHO_TO_CONSOLE;
      } else if (token == kRecordAsMuchAsPossible) {
        record_mode_ = RECORD_AS_MUCH_AS_POSSIBLE;
      } else if (token.find(kEnableSystrace) == 0) {
        // Find optional events list.
        const size_t length = token.length();
        if (length == kEnableSystraceLength) {
          // Use all predefined categories.
          enable_systrace_ = true;
          continue;
        }
        const auto system_events_not_trimmed =
            token.substr(kEnableSystraceLength);
        const auto system_events =
            TrimString(system_events_not_trimmed, kWhitespaceASCII, TRIM_ALL);
        if (system_events[0] != '=') {
          LOG(ERROR) << "Failed to parse " << token;
          continue;
        }
        enable_systrace_ = true;
        const std::vector<std::string> split_systrace_events = SplitString(
            system_events.substr(1), " ", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
        for (const std::string& systrace_event : split_systrace_events)
          systrace_events_.insert(systrace_event);
      } else if (token == kEnableArgumentFilter) {
        enable_argument_filter_ = true;
      }
    }
  }

  if (category_filter_.IsCategoryEnabled(MemoryDumpManager::kTraceCategory)) {
    SetDefaultMemoryDumpConfig();
  }
}

void TraceConfig::SetMemoryDumpConfigFromConfigDict(
    const Value::Dict& memory_dump_config) {
  // Set allowed dump modes.
  memory_dump_config_.allowed_dump_modes.clear();
  const Value::List* allowed_modes_list =
      memory_dump_config.FindList(kAllowedDumpModesParam);
  if (allowed_modes_list) {
    for (const Value& item : *allowed_modes_list) {
      DCHECK(item.is_string());
      memory_dump_config_.allowed_dump_modes.insert(
          StringToMemoryDumpLevelOfDetail(item.GetString()));
    }
  } else {
    // If allowed modes param is not given then allow all modes by default.
    memory_dump_config_.allowed_dump_modes = GetDefaultAllowedMemoryDumpModes();
  }

  // Set triggers
  memory_dump_config_.triggers.clear();
  const Value::List* trigger_list = memory_dump_config.FindList(kTriggersParam);
  if (trigger_list) {
    for (const Value& trigger : *trigger_list) {
      if (!trigger.is_dict()) {
        continue;
      }
      const Value::Dict& trigger_dict = trigger.GetDict();

      MemoryDumpConfig::Trigger dump_config;
      std::optional<int> interval = trigger_dict.FindInt(kMinTimeBetweenDumps);
      if (!interval) {
        // If "min_time_between_dumps_ms" param was not given, then the trace
        // config uses old format where only periodic dumps are supported.
        interval = trigger_dict.FindInt(kPeriodicIntervalLegacyParam);
        dump_config.trigger_type = MemoryDumpType::kPeriodicInterval;
      } else {
        const std::string* trigger_type_str =
            trigger_dict.FindString(kTriggerTypeParam);
        DCHECK(trigger_type_str);
        dump_config.trigger_type = StringToMemoryDumpType(*trigger_type_str);
      }
      DCHECK(interval.has_value());
      DCHECK_GT(*interval, 0);
      dump_config.min_time_between_dumps_ms = static_cast<uint32_t>(*interval);

      const std::string* level_of_detail_str =
          trigger_dict.FindString(kTriggerModeParam);
      DCHECK(level_of_detail_str);
      dump_config.level_of_detail =
          StringToMemoryDumpLevelOfDetail(*level_of_detail_str);

      memory_dump_config_.triggers.push_back(dump_config);
    }
  }

  // Set heap profiler options.
  const Value::Dict* heap_profiler_options =
      memory_dump_config.FindDict(kHeapProfilerOptions);
  if (heap_profiler_options) {
    std::optional<int> min_size_bytes =
        heap_profiler_options->FindInt(kBreakdownThresholdBytes);
    if (min_size_bytes && *min_size_bytes >= 0) {
      memory_dump_config_.heap_profiler_options.breakdown_threshold_bytes =
          static_cast<uint32_t>(*min_size_bytes);
    } else {
      memory_dump_config_.heap_profiler_options.breakdown_threshold_bytes =
          MemoryDumpConfig::HeapProfiler::kDefaultBreakdownThresholdBytes;
    }
  }
}

void TraceConfig::SetDefaultMemoryDumpConfig() {
  memory_dump_config_.Clear();
  memory_dump_config_.allowed_dump_modes = GetDefaultAllowedMemoryDumpModes();
}

void TraceConfig::SetProcessFilterConfig(const ProcessFilterConfig& config) {
  process_filter_config_ = config;
}

void TraceConfig::SetHistogramNamesFromConfigList(
    const Value::List& histogram_names) {
  histogram_names_.clear();
  for (const Value& value : histogram_names) {
    histogram_names_.insert(value.GetString());
  }
}

void TraceConfig::SetEventFiltersFromConfigList(
    const Value::List& category_event_filters) {
  event_filters_.clear();

  for (const Value& event_filter : category_event_filters) {
    if (!event_filter.is_dict()) {
      continue;
    }
    const Value::Dict& event_filter_dict = event_filter.GetDict();

    const std::string* predicate_name =
        event_filter_dict.FindString(kFilterPredicateParam);
    CHECK(predicate_name) << "Invalid predicate name in category event filter.";

    EventFilterConfig new_config(*predicate_name);
    new_config.InitializeFromConfigDict(event_filter_dict);
    event_filters_.push_back(new_config);
  }
}

Value TraceConfig::ToValue() const {
  Value::Dict dict;
  dict.Set(kRecordModeParam, TraceConfig::TraceRecordModeToStr(record_mode_));
  dict.Set(kEnableSystraceParam, enable_systrace_);
  dict.Set(kEnableArgumentFilterParam, enable_argument_filter_);
  if (trace_buffer_size_in_events_ > 0) {
    dict.Set(kTraceBufferSizeInEvents,
             base::checked_cast<int>(trace_buffer_size_in_events_));
  }
  if (trace_buffer_size_in_kb_ > 0) {
    dict.Set(kTraceBufferSizeInKb,
             base::checked_cast<int>(trace_buffer_size_in_kb_));
  }

  dict.Set(kEnableEventPackageNameFilterParam,
           enable_event_package_name_filter_);

  category_filter_.ToDict(dict);
  process_filter_config_.ToDict(dict);

  if (!event_filters_.empty()) {
    Value::List filter_list;
    for (const EventFilterConfig& filter : event_filters_) {
      Value::Dict filter_dict;
      filter.ToDict(filter_dict);
      filter_list.Append(std::move(filter_dict));
    }
    dict.Set(kEventFiltersParam, std::move(filter_list));
  }

  if (category_filter_.IsCategoryEnabled(MemoryDumpManager::kTraceCategory)) {
    Value::List allowed_modes;
    for (auto dump_mode : memory_dump_config_.allowed_dump_modes)
      allowed_modes.Append(MemoryDumpLevelOfDetailToString(dump_mode));

    Value::Dict memory_dump_config;
    memory_dump_config.Set(kAllowedDumpModesParam, std::move(allowed_modes));

    Value::List triggers_list;
    for (const auto& config : memory_dump_config_.triggers) {
      Value::Dict trigger_dict;

      trigger_dict.Set(kTriggerTypeParam,
                       MemoryDumpTypeToString(config.trigger_type));
      trigger_dict.Set(kMinTimeBetweenDumps,
                       static_cast<int>(config.min_time_between_dumps_ms));
      trigger_dict.Set(kTriggerModeParam,
                       MemoryDumpLevelOfDetailToString(config.level_of_detail));
      triggers_list.Append(std::move(trigger_dict));
    }

    // Empty triggers will still be specified explicitly since it means that
    // the periodic dumps are not enabled.
    memory_dump_config.Set(kTriggersParam, std::move(triggers_list));

    if (memory_dump_config_.heap_profiler_options.breakdown_threshold_bytes !=
        MemoryDumpConfig::HeapProfiler::kDefaultBreakdownThresholdBytes) {
      Value::Dict options;
      options.Set(
          kBreakdownThresholdBytes,
          base::checked_cast<int>(memory_dump_config_.heap_profiler_options
                                      .breakdown_threshold_bytes));
      memory_dump_config.Set(kHeapProfilerOptions, std::move(options));
    }
    dict.Set(kMemoryDumpConfigParam, std::move(memory_dump_config));
  }

  if (!histogram_names_.empty()) {
    base::Value::List histogram_names;
    for (const std::string& histogram_name : histogram_names_)
      histogram_names.Append(histogram_name);
    dict.Set(kHistogramNamesParam, std::move(histogram_names));
  }

  if (enable_systrace_) {
    if (!systrace_events_.empty()) {
      base::Value::List systrace_events;
      for (const std::string& systrace_event : systrace_events_)
        systrace_events.Append(systrace_event);
      dict.Set(kSystraceEventsParam, std::move(systrace_events));
    }
  }

  return Value(std::move(dict));
}

void TraceConfig::EnableSystraceEvent(const std::string& systrace_event) {
  systrace_events_.insert(systrace_event);
}

void TraceConfig::EnableHistogram(const std::string& histogram_name) {
  histogram_names_.insert(histogram_name);
}

std::string TraceConfig::ToTraceOptionsString() const {
  std::string ret;
  switch (record_mode_) {
    case RECORD_UNTIL_FULL:
      ret = kRecordUntilFull;
      break;
    case RECORD_CONTINUOUSLY:
      ret = kRecordContinuously;
      break;
    case RECORD_AS_MUCH_AS_POSSIBLE:
      ret = kRecordAsMuchAsPossible;
      break;
    case ECHO_TO_CONSOLE:
      ret = kTraceToConsole;
      break;
  }
  if (enable_systrace_) {
    ret += ",";
    ret += kEnableSystrace;
    bool first_param = true;
    for (const std::string& systrace_event : systrace_events_) {
      if (first_param) {
        ret += "=";
        first_param = false;
      } else {
        ret += " ";
      }
      ret = ret + systrace_event;
    }
  }
  if (enable_argument_filter_) {
    ret += ",";
    ret += kEnableArgumentFilter;
  }
  return ret;
}

std::string TraceConfig::ToPerfettoTrackEventConfigRaw(
    bool privacy_filtering_enabled) const {
  perfetto::protos::gen::TrackEventConfig te_cfg;
  if (!base::Contains(category_filter_.excluded_categories(), "*") &&
      !base::Contains(category_filter_.included_categories(), "*")) {
    // In the case when the default behavior is not specified, apply the
    // following rule: if no categories are explicitly enabled, enable the
    // default ones; otherwise only enable matching categories.
    if (category_filter_.included_categories().empty()) {
      te_cfg.add_enabled_categories("*");
    } else {
      te_cfg.add_disabled_categories("*");
    }
  }
  for (const auto& excluded : category_filter_.excluded_categories()) {
    te_cfg.add_disabled_categories(excluded);
  }
  for (const auto& included : category_filter_.included_categories()) {
    te_cfg.add_enabled_categories(included);
  }
  for (const auto& disabled : category_filter_.disabled_categories()) {
    te_cfg.add_enabled_categories(disabled);
  }
  // Metadata is always enabled.
  te_cfg.add_enabled_categories("__metadata");
  te_cfg.set_enable_thread_time_sampling(true);
  te_cfg.set_timestamp_unit_multiplier(1000);
  if (privacy_filtering_enabled) {
    te_cfg.set_filter_dynamic_event_names(true);
    te_cfg.set_filter_debug_annotations(true);
  }
  return te_cfg.SerializeAsString();
}

}  // namespace base::trace_event

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/perf_events_collector.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "chrome/browser/metrics/perf/cpu_identity.h"
#include "chrome/browser/metrics/perf/process_type_collector.h"
#include "chrome/browser/metrics/perf/windowed_incognito_observer.h"
#include "chrome/browser/ui/browser_list.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client_provider.h"
#include "components/variations/variations_associated_data.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

namespace {

const char kCWPFieldTrialName[] = "ChromeOSWideProfilingCollection";

// Name the histogram that represents the success and various failure modes for
// parsing CPU frequencies.
const char kParseFrequenciesHistogramName[] =
    "ChromeOS.CWP.ParseCPUFrequencies";

// Limit the total size of protobufs that can be cached, so they don't take up
// too much memory. If the size of cached protobufs exceeds this value, stop
// collecting further perf data. The current value is 4 MB.
const size_t kCachedPerfDataProtobufSizeThreshold = 4 * 1024 * 1024;

// Name of the perf events collector. It is appended to the UMA metric names
// for reporting collection and upload status.
const char kPerfCollectorName[] = "Perf";

// Gets parameter named by |key| from the map. If it is present and is an
// integer, stores the result in |out| and return true. Otherwise return false.
bool GetInt64Param(const std::map<std::string, std::string>& params,
                   const std::string& key,
                   int64_t* out) {
  auto it = params.find(key);
  if (it == params.end())
    return false;
  int64_t value;
  // NB: StringToInt64 will set value even if the conversion fails.
  if (!base::StringToInt64(it->second, &value))
    return false;
  *out = value;
  return true;
}

// Parses the key. e.g.: "PerfCommand::arm::0" returns "arm"
bool ExtractPerfCommandCpuSpecifier(const std::string& key,
                                    std::string* cpu_specifier) {
  std::vector<std::string> tokens = base::SplitStringUsingSubstr(
      key, "::", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.size() != 3)
    return false;
  if (tokens[0] != "PerfCommand")
    return false;
  *cpu_specifier = tokens[1];
  // tokens[2] is just a unique string (usually an index).
  return true;
}

// Parses the components of a version string, e.g. major.minor.bugfix
void ExtractVersionNumbers(const std::string& version,
                           int32_t* major_version,
                           int32_t* minor_version,
                           int32_t* bugfix_version) {
  *major_version = *minor_version = *bugfix_version = 0;
  // Parse out the version numbers from the string.
  sscanf(version.c_str(), "%d.%d.%d", major_version, minor_version,
         bugfix_version);
}

// Returns if a micro-architecture supports LBR callgraph profiling.
bool MicroarchitectureHasLBRCallgraph(const std::string& uarch) {
  return uarch == "Haswell" || uarch == "Broadwell" || uarch == "Skylake" ||
         uarch == "Kabylake";
}

// Returns if a kernel release supports LBR callgraph profiling.
bool KernelReleaseHasLBRCallgraph(const std::string& release) {
  int32_t major, minor, bugfix;
  ExtractVersionNumbers(release, &major, &minor, &bugfix);
  return major > 4 || (major == 4 && minor >= 4) || (major == 3 && minor == 18);
}

// Hopefully we never need a space in a command argument.
const char kPerfCommandDelimiter[] = " ";

const char kPerfRecordCyclesCmd[] = "perf record -a -e cycles -c 1000003";

const char kPerfRecordFPCallgraphCmd[] =
    "perf record -a -e cycles -g -c 4000037";

const char kPerfRecordLBRCallgraphCmd[] =
    "perf record -a -e cycles -c 4000037 --call-graph lbr";

const char kPerfRecordLBRCmd[] = "perf record -a -e r20c4 -b -c 200011";

// Silvermont, Airmont, Goldmont don't have a branches taken event. Therefore,
// we sample on the branches retired event.
const char kPerfRecordLBRCmdAtom[] = "perf record -a -e rc4 -b -c 300001";

// The following events count misses in the level 1 caches and TLBs.

// Perf doesn't support the generic dTLB-misses event for Goldmont. We define it
// in terms of raw event number and umask value. Event codes taken from
// "Intel 64 and IA-32 Architectures Software Developer's Manual, Vol 3".
const char kPerfRecordInstructionTLBMissesCmdGLM[] =
    "perf record -a -e r0481 -c 2003";

const char kPerfRecordDataTLBMissesCmdGLM[] = "perf record -a -e r13d0 -c 2003";

// Use the generic event names for the other microarchitectures.
const char kPerfRecordInstructionTLBMissesCmd[] =
    "perf record -a -e iTLB-misses -c 2003";

const char kPerfRecordDataTLBMissesCmd[] =
    "perf record -a -e dTLB-misses -c 2003";

const char kPerfRecordCacheMissesCmd[] =
    "perf record -a -e cache-misses -c 10007";

const std::vector<RandomSelector::WeightAndValue> GetDefaultCommands_x86_64(
    const CPUIdentity& cpuid) {
  using WeightAndValue = RandomSelector::WeightAndValue;
  std::vector<WeightAndValue> cmds;
  DCHECK_EQ(cpuid.arch, "x86_64");
  const std::string cpu_uarch = GetCpuUarch(cpuid);
  // Haswell and newer big Intel cores support LBR callstack profiling. This
  // requires kernel support, which was added in kernel 4.4, and it was
  // backported to kernel 3.18. Prefer LBR callstack profiling where supported
  // instead of FP callchains, because the former works with binaries compiled
  // with frame pointers disabled, such as the ARC runtime.
  const char* callgraph_cmd = kPerfRecordFPCallgraphCmd;
  if (MicroarchitectureHasLBRCallgraph(cpu_uarch) &&
      KernelReleaseHasLBRCallgraph(cpuid.release)) {
    callgraph_cmd = kPerfRecordLBRCallgraphCmd;
  }

  if (cpu_uarch == "IvyBridge" || cpu_uarch == "Haswell" ||
      cpu_uarch == "Broadwell" || cpu_uarch == "SandyBridge" ||
      cpu_uarch == "Skylake" || cpu_uarch == "Kabylake") {
    cmds.push_back(WeightAndValue(50.0, kPerfRecordCyclesCmd));
    cmds.push_back(WeightAndValue(20.0, callgraph_cmd));
    cmds.push_back(WeightAndValue(15.0, kPerfRecordLBRCmd));
    cmds.push_back(WeightAndValue(5.0, kPerfRecordInstructionTLBMissesCmd));
    cmds.push_back(WeightAndValue(5.0, kPerfRecordDataTLBMissesCmd));
    cmds.push_back(WeightAndValue(5.0, kPerfRecordCacheMissesCmd));
    return cmds;
  }
  if (cpu_uarch == "Silvermont" || cpu_uarch == "Airmont") {
    cmds.push_back(WeightAndValue(50.0, kPerfRecordCyclesCmd));
    cmds.push_back(WeightAndValue(20.0, callgraph_cmd));
    cmds.push_back(WeightAndValue(15.0, kPerfRecordLBRCmdAtom));
    cmds.push_back(WeightAndValue(5.0, kPerfRecordInstructionTLBMissesCmd));
    cmds.push_back(WeightAndValue(5.0, kPerfRecordDataTLBMissesCmd));
    cmds.push_back(WeightAndValue(5.0, kPerfRecordCacheMissesCmd));
    return cmds;
  }
  if (cpu_uarch == "Goldmont" || cpu_uarch == "GoldmontPlus") {
    cmds.push_back(WeightAndValue(50.0, kPerfRecordCyclesCmd));
    cmds.push_back(WeightAndValue(20.0, callgraph_cmd));
    cmds.push_back(WeightAndValue(15.0, kPerfRecordLBRCmdAtom));
    // Use the Goldmont variants of iTLB and dTLB misses.
    cmds.push_back(WeightAndValue(5.0, kPerfRecordInstructionTLBMissesCmdGLM));
    cmds.push_back(WeightAndValue(5.0, kPerfRecordDataTLBMissesCmdGLM));
    cmds.push_back(WeightAndValue(5.0, kPerfRecordCacheMissesCmd));
    return cmds;
  }
  // Other 64-bit x86
  cmds.push_back(WeightAndValue(65.0, kPerfRecordCyclesCmd));
  cmds.push_back(WeightAndValue(20.0, callgraph_cmd));
  cmds.push_back(WeightAndValue(5.0, kPerfRecordInstructionTLBMissesCmd));
  cmds.push_back(WeightAndValue(5.0, kPerfRecordDataTLBMissesCmd));
  cmds.push_back(WeightAndValue(5.0, kPerfRecordCacheMissesCmd));
  return cmds;
}

void OnCollectProcessTypes(SampledProfile* sampled_profile) {
  std::map<uint32_t, Process> process_types =
      ProcessTypeCollector::ChromeProcessTypes();
  std::map<uint32_t, Thread> thread_types =
      ProcessTypeCollector::ChromeThreadTypes();
  if (!process_types.empty() && !thread_types.empty()) {
    sampled_profile->mutable_process_types()->insert(process_types.begin(),
                                                     process_types.end());
    sampled_profile->mutable_thread_types()->insert(thread_types.begin(),
                                                    thread_types.end());
  }
}

}  // namespace

namespace internal {

std::vector<RandomSelector::WeightAndValue> GetDefaultCommandsForCpu(
    const CPUIdentity& cpuid) {
  using WeightAndValue = RandomSelector::WeightAndValue;

  if (cpuid.arch == "x86_64")  // 64-bit x86
    return GetDefaultCommands_x86_64(cpuid);

  std::vector<WeightAndValue> cmds;
  if (cpuid.arch == "x86" ||     // 32-bit x86, or...
      cpuid.arch == "armv7l") {  // ARM
    cmds.push_back(WeightAndValue(80.0, kPerfRecordCyclesCmd));
    cmds.push_back(WeightAndValue(20.0, kPerfRecordFPCallgraphCmd));
    return cmds;
  }

  // Unknown CPUs
  cmds.push_back(WeightAndValue(1.0, kPerfRecordCyclesCmd));
  return cmds;
}

}  // namespace internal

PerfCollector::PerfCollector()
    : internal::MetricCollector(kPerfCollectorName, CollectionParams()) {}

PerfCollector::~PerfCollector() = default;

void PerfCollector::SetUp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Create DebugdClientProvider to bind its private DBus connection to the
  // current sequence.
  debugd_client_provider_ =
      std::make_unique<chromeos::DebugDaemonClientProvider>();

  auto task_runner = base::SequencedTaskRunnerHandle::Get();
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&PerfCollector::ParseCPUFrequencies, task_runner,
                     weak_factory_.GetWeakPtr()));

  CHECK(command_selector_.SetOdds(
      internal::GetDefaultCommandsForCpu(GetCPUIdentity())));
  std::map<std::string, std::string> params;
  if (variations::GetVariationParams(kCWPFieldTrialName, &params)) {
    SetCollectionParamsFromVariationParams(params);
  }
}

const char* PerfCollector::ToolName() const {
  return kPerfCollectorName;
}

namespace internal {

std::string FindBestCpuSpecifierFromParams(
    const std::map<std::string, std::string>& params,
    const CPUIdentity& cpuid) {
  std::string ret;
  // The CPU specified in the variation params could be "default", a system
  // architecture, a CPU microarchitecture, or a CPU model substring. We should
  // prefer to match the most specific.
  enum MatchSpecificity {
    NO_MATCH,
    DEFAULT,
    SYSTEM_ARCH,
    CPU_UARCH,
    CPU_MODEL,
  };
  MatchSpecificity match_level = NO_MATCH;

  const std::string cpu_uarch = GetCpuUarch(cpuid);
  const std::string simplified_cpu_model =
      SimplifyCPUModelName(cpuid.model_name);

  for (const auto& key_val : params) {
    const std::string& key = key_val.first;

    std::string cpu_specifier;
    if (!ExtractPerfCommandCpuSpecifier(key, &cpu_specifier))
      continue;

    if (match_level < DEFAULT && cpu_specifier == "default") {
      match_level = DEFAULT;
      ret = cpu_specifier;
    }
    if (match_level < SYSTEM_ARCH && cpu_specifier == cpuid.arch) {
      match_level = SYSTEM_ARCH;
      ret = cpu_specifier;
    }
    if (match_level < CPU_UARCH && !cpu_uarch.empty() &&
        cpu_specifier == cpu_uarch) {
      match_level = CPU_UARCH;
      ret = cpu_specifier;
    }
    if (match_level < CPU_MODEL &&
        simplified_cpu_model.find(cpu_specifier) != std::string::npos) {
      match_level = CPU_MODEL;
      ret = cpu_specifier;
    }
  }
  return ret;
}

}  // namespace internal

void PerfCollector::SetCollectionParamsFromVariationParams(
    const std::map<std::string, std::string>& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t value;
  CollectionParams& collector_params = collection_params();
  if (GetInt64Param(params, "ProfileCollectionDurationSec", &value)) {
    collector_params.collection_duration = base::TimeDelta::FromSeconds(value);
  }
  if (GetInt64Param(params, "PeriodicProfilingIntervalMs", &value)) {
    collector_params.periodic_interval =
        base::TimeDelta::FromMilliseconds(value);
  }
  if (GetInt64Param(params, "ResumeFromSuspend::SamplingFactor", &value)) {
    collector_params.resume_from_suspend.sampling_factor = value;
  }
  if (GetInt64Param(params, "ResumeFromSuspend::MaxDelaySec", &value)) {
    collector_params.resume_from_suspend.max_collection_delay =
        base::TimeDelta::FromSeconds(value);
  }
  if (GetInt64Param(params, "RestoreSession::SamplingFactor", &value)) {
    collector_params.restore_session.sampling_factor = value;
  }
  if (GetInt64Param(params, "RestoreSession::MaxDelaySec", &value)) {
    collector_params.restore_session.max_collection_delay =
        base::TimeDelta::FromSeconds(value);
  }

  const std::string best_cpu_specifier =
      internal::FindBestCpuSpecifierFromParams(params, GetCPUIdentity());

  if (best_cpu_specifier.empty())  // No matching cpu specifier. Keep defaults.
    return;

  std::vector<RandomSelector::WeightAndValue> commands;
  for (const auto& key_val : params) {
    const std::string& key = key_val.first;
    const std::string& val = key_val.second;

    std::string cpu_specifier;
    if (!ExtractPerfCommandCpuSpecifier(key, &cpu_specifier))
      continue;
    if (cpu_specifier != best_cpu_specifier)
      continue;

    auto split = val.find(" ");
    if (split == std::string::npos)
      continue;  // Just drop invalid commands.
    std::string weight_str = std::string(val.begin(), val.begin() + split);

    double weight;
    if (!(base::StringToDouble(weight_str, &weight) && weight > 0.0))
      continue;  // Just drop invalid commands.
    std::string command(val.begin() + split + 1, val.end());
    commands.push_back(RandomSelector::WeightAndValue(weight, command));
  }
  command_selector_.SetOdds(commands);
}

std::unique_ptr<PerfOutputCall> PerfCollector::CreatePerfOutputCall(
    base::TimeDelta duration,
    const std::vector<std::string>& perf_args,
    PerfOutputCall::DoneCallback callback) {
  DCHECK(debugd_client_provider_.get());
  return std::make_unique<PerfOutputCall>(
      debugd_client_provider_->debug_daemon_client(), duration, perf_args,
      std::move(callback));
}

void PerfCollector::OnPerfOutputComplete(
    std::unique_ptr<WindowedIncognitoObserver> incognito_observer,
    std::unique_ptr<SampledProfile> sampled_profile,
    bool has_cycles,
    std::string perf_stdout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  current_trigger_ = SampledProfile::UNKNOWN_TRIGGER_EVENT;
  // We are done using |perf_output_call| and may destroy it.
  perf_output_call_ = nullptr;

  ParseOutputProtoIfValid(std::move(incognito_observer),
                          std::move(sampled_profile), has_cycles,
                          std::move(perf_stdout));
}

void PerfCollector::ParseOutputProtoIfValid(
    std::unique_ptr<WindowedIncognitoObserver> incognito_observer,
    std::unique_ptr<SampledProfile> sampled_profile,
    bool has_cycles,
    std::string perf_stdout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check whether an incognito window had been opened during profile
  // collection. If there was an incognito window, discard the incoming data.
  if (incognito_observer->IncognitoLaunched()) {
    AddToUmaHistogram(CollectionAttemptStatus::INCOGNITO_LAUNCHED);
    return;
  }
  if (has_cycles) {
    // Store CPU max frequencies in the sampled profile.
    std::copy(max_frequencies_mhz_.begin(), max_frequencies_mhz_.end(),
              google::protobuf::RepeatedFieldBackInserter(
                  sampled_profile->mutable_cpu_max_frequency_mhz()));
  }

  bool posted = base::PostTaskAndReply(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&OnCollectProcessTypes, sampled_profile.get()),
      base::BindOnce(&PerfCollector::SaveSerializedPerfProto,
                     weak_factory_.GetWeakPtr(), std::move(sampled_profile),
                     std::move(perf_stdout)));
  DCHECK(posted);
}

base::WeakPtr<internal::MetricCollector> PerfCollector::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

bool PerfCollector::ShouldCollect() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only allow one active collection.
  if (perf_output_call_) {
    AddToUmaHistogram(CollectionAttemptStatus::ALREADY_COLLECTING);
    return false;
  }

  // Do not collect further data if we've already collected a substantial amount
  // of data, as indicated by |kCachedPerfDataProtobufSizeThreshold|.
  if (cached_data_size_ >= kCachedPerfDataProtobufSizeThreshold) {
    AddToUmaHistogram(CollectionAttemptStatus::NOT_READY_TO_COLLECT);
    return false;
  }

  return true;
}

namespace internal {

bool CommandSamplesCPUCycles(const std::vector<std::string>& args) {
  // Command must start with "perf record".
  if (args.size() < 4 || args[0] != "perf" || args[1] != "record")
    return false;
  for (size_t i = 2; i + 1 < args.size(); ++i) {
    if (args[i] == "-e" && args[i + 1] == "cycles")
      return true;
  }
  return false;
}

}  // namespace internal

void PerfCollector::CollectProfile(
    std::unique_ptr<SampledProfile> sampled_profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto incognito_observer = WindowedIncognitoMonitor::CreateObserver();
  // For privacy reasons, Chrome should only collect perf data if there is no
  // incognito session active (or gets spawned during the collection).
  if (incognito_observer->IncognitoActive()) {
    AddToUmaHistogram(CollectionAttemptStatus::INCOGNITO_ACTIVE);
    return;
  }

  std::vector<std::string> command =
      base::SplitString(command_selector_.Select(), kPerfCommandDelimiter,
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  bool has_cycles = internal::CommandSamplesCPUCycles(command);

  DCHECK(sampled_profile->has_trigger_event());
  current_trigger_ = sampled_profile->trigger_event();

  perf_output_call_ = CreatePerfOutputCall(
      collection_params().collection_duration, command,
      base::BindOnce(&PerfCollector::OnPerfOutputComplete,
                     weak_factory_.GetWeakPtr(), std::move(incognito_observer),
                     std::move(sampled_profile), has_cycles));
}

// static
void PerfCollector::ParseCPUFrequencies(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::WeakPtr<PerfCollector> perf_collector) {
  const char kCPUMaxFreqPath[] =
      "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq";
  int num_cpus = base::SysInfo::NumberOfProcessors();
  int num_zeros = 0;
  std::vector<uint32_t> frequencies_mhz;
  for (int i = 0; i < num_cpus; ++i) {
    std::string content;
    unsigned int frequency_khz = 0;
    auto path = base::StringPrintf(kCPUMaxFreqPath, i);
    if (ReadFileToString(base::FilePath(path), &content)) {
      DCHECK(!content.empty());
      base::StringToUint(content, &frequency_khz);
    }
    if (frequency_khz == 0) {
      num_zeros++;
    }
    // Convert kHz frequencies to MHz.
    frequencies_mhz.push_back(static_cast<uint32_t>(frequency_khz / 1000));
  }
  if (num_cpus == 0) {
    base::UmaHistogramEnumeration(kParseFrequenciesHistogramName,
                                  ParseFrequencyStatus::kNumCPUsIsZero);
  } else if (num_zeros == num_cpus) {
    base::UmaHistogramEnumeration(kParseFrequenciesHistogramName,
                                  ParseFrequencyStatus::kAllZeroCPUFrequencies);
  } else if (num_zeros > 0) {
    base::UmaHistogramEnumeration(
        kParseFrequenciesHistogramName,
        ParseFrequencyStatus::kSomeZeroCPUFrequencies);
  } else {
    base::UmaHistogramEnumeration(kParseFrequenciesHistogramName,
                                  ParseFrequencyStatus::kSuccess);
  }
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&PerfCollector::SaveCPUFrequencies,
                                       perf_collector, frequencies_mhz));
}

void PerfCollector::SaveCPUFrequencies(
    const std::vector<uint32_t>& frequencies) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  max_frequencies_mhz_ = frequencies;
}

void PerfCollector::StopCollection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // StopCollection() can be called when a jank lasts for longer than the max
  // collection duration, and a new collection is requested by another trigger.
  // In this case, ignore the request to stop the collection.
  if (current_trigger_ != SampledProfile::JANKY_TASK)
    return;

  if (perf_output_call_)
    perf_output_call_->Stop();
}

}  // namespace metrics

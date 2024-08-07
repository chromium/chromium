// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/perf_events_collector.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/perf/cpu_identity.h"
#include "chrome/browser/metrics/perf/process_type_collector.h"
#include "chrome/browser/metrics/perf/windowed_incognito_observer.h"
#include "chrome/browser/ui/browser_list.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client_provider.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"
#include "third_party/re2/src/re2/re2.h"

namespace metrics {

BASE_FEATURE(kCWPCollectsETM,
             "CWPCollectsETM",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

const char kCWPFieldTrialName[] = "ChromeOSWideProfilingCollection";

// Name the histogram that represents the success and various failure modes for
// parsing CPU frequencies.
const char kParseFrequenciesHistogramName[] =
    "ChromeOS.CWP.ParseCPUFrequencies";

// Name of the histogram that represents the success and various failure modes
// for parsing PSI CPU data.
const char kParsePSICPUHistogramName[] = "ChromeOS.CWP.ParsePSICPU";

// Name of the histogram that represents the success and various failure modes
// for parsing a stateful Lacros path to get its version and channel.
const char kParseLacrosPathHistogramName[] = "ChromeOS.CWP.ParseLacrosPath";

// Limit the total size of protobufs that can be cached, so they don't take up
// too much memory. If the size of cached protobufs exceeds this value, stop
// collecting further perf data. The current value is 4 MB.
const size_t kCachedPerfDataProtobufSizeThreshold = 4 * 1024 * 1024;

// Name of the perf events collector. It is appended to the UMA metric names
// for reporting collection and upload status.
const char kPerfCollectorName[] = "Perf";

// File path that stores PSI CPU data.
const char kPSICPUPath[] = "/proc/pressure/cpu";

// The rootfs Lacros binary path prefix.
// TODO(b/210001558): remove this logic and use the BrowserManager API
// if that is implemented.
const char kRootfsLacrosPrefix[] = "/run/lacros/chrome";

// Matches Lacros version and channel from the stateful Lacros path.
// The stateful paths are defined at
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/crosapi/browser_util.cc;l=215-224;drc=a7f9d69da4cbe7d796753bce5229f5f8e562b153
const LazyRE2 kLacrosChannelVersionMatcher = {
    R"(/run/imageloader/lacros-dogfood-(\w+)/([\d.]+)/chrome)"};

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

// Returns if a micro-architecture supports the cycles:ppp event.
bool MicroarchitectureHasCyclesPPPEvent(const std::string& uarch) {
  return uarch == "Goldmont" || uarch == "GoldmontPlus" || uarch == "Tremont" ||
         uarch == "Broadwell" || uarch == "Kabylake" || uarch == "Tigerlake" ||
         uarch == "AlderLake" || uarch == "RaptorLake" || uarch == "Gracemont";
}

// Returns if a kernel release properly flushes PEBS on a context switch. The
// fix landed in kernel 5.12 upstream, but it was backported to CrOS kernels
// 4.14, 4.19, 5.4 and 5.10.
bool KernelReleaseHasPEBSFlushingFix(const std::string& release) {
  int32_t major, minor, bugfix;
  ExtractVersionNumbers(release, &major, &minor, &bugfix);
  return major >= 5 || (major == 4 && minor >= 14);
}

// Returns if a micro-architecture supports LBR callgraph profiling.
bool MicroarchitectureHasLBRCallgraph(const std::string& uarch) {
  return uarch == "Haswell" || uarch == "Broadwell" || uarch == "Skylake" ||
         uarch == "Kabylake" || uarch == "Tigerlake" || uarch == "Tremont" ||
         uarch == "AlderLake" || uarch == "RaptorLake" || uarch == "Gracemont";
}

// Returns if a kernel release supports LBR callgraph profiling.
bool KernelReleaseHasLBRCallgraph(const std::string& release) {
  int32_t major, minor, bugfix;
  ExtractVersionNumbers(release, &major, &minor, &bugfix);
  return major > 4 || (major == 4 && minor >= 4) || (major == 3 && minor == 18);
}

// Hopefully we never need a space in a command argument.
const char kPerfCommandDelimiter[] = " ";

// Collect precise=3 (:ppp) cycle events on microarchitectures and kernels that
// support it.
const char kPerfLBRCallgraphPPPCmd[] =
    "-- record -a -e cycles:ppp -c 6000011 --call-graph lbr";

const char kPerfCyclesPPPHGCmd[] = "-- record -a -e cycles:pppHG -c 1000003";

const char kPerfFPCallgraphPPPHGCmd[] =
    "-- record -a -e cycles:pppHG -g -c 4000037";

// Collect default (imprecise) cycle events everywhere else.
const char kPerfCyclesHGCmd[] = "-- record -a -e cycles:HG -c 1000003";

const char kPerfFPCallgraphHGCmd[] = "-- record -a -e cycles:HG -g -c 4000037";

const char kPerfLBRCallgraphCmd[] =
    "-- record -a -e cycles -c 6000011 --call-graph lbr";

const char kPerfLBRCmd[] = "-- record -a -e r20c4 -b -c 800011";

// Silvermont, Airmont, Goldmont don't have a branches taken event. Therefore,
// we sample on the branches retired event.
const char kPerfLBRCmdAtom[] = "-- record -a -e rc4 -b -c 800011";

// Tremont and Gracemont use different codes for BR_INST_RETIRED.NEAR_TAKEN.
const char kPerfLBRCmdTremont[] = "-- record -a -e rc0c4 -b -c 800011";

// Intel Hybrid architectures starting from AlderLake use different PMUs
// for PCore (e.g. Golden Cove) and ECore (e.g. Gracemont).
const char kPerfLBRCmdAlderLake[] =
    "-- record -a -e cpu_core/r20c4/ -e cpu_atom/rc0c4/ -b -c 800011";

// The following events count misses in the last level caches and level 2 TLBs.

// TLB miss cycles for IvyBridge, Haswell, Broadwell and SandyBridge.
const char kPerfITLBMissCyclesCmdIvyBridge[] =
    "-- record -a -e itlb_misses.walk_duration -c 30001";

const char kPerfDTLBMissCyclesCmdIvyBridge[] =
    "-- record -a -e dtlb_load_misses.walk_duration -g -c 350003";

// TLB miss cycles for Skylake, Kabylake, Tigerlake.
const char kPerfITLBMissCyclesCmdSkylake[] =
    "-- record -a -e itlb_misses.walk_pending -c 30001";

const char kPerfDTLBMissCyclesCmdSkylake[] =
    "-- record -a -e dtlb_load_misses.walk_pending -g -c 350003";

// TLB miss cycles for Atom, including Silvermont, Airmont and Goldmont.
const char kPerfITLBMissCyclesCmdAtom[] =
    "-- record -a -e page_walks.i_side_cycles -c 30001";

const char kPerfDTLBMissCyclesCmdAtom[] =
    "-- record -a -e page_walks.d_side_cycles -g -c 350003";

// TLB miss cycles using raw PMU event codes.
const char kPerfITLBMissCyclesCmdTremont[] = "-- record -a -e r1085 -c 30001";
const char kPerfDTLBMissCyclesCmdTremont[] =
    "-- record -a -e r1008 -g -c 350003";

// TLB misses event for Intel hybrid architectures starting from AlderLake.
const char kPerfITLBMissCyclesCmdAlderLake[] =
    "-- record -a -e cpu_core/r1011/ -e cpu_atom/r1085/ -c 30001";
const char kPerfDTLBMissCyclesCmdAlderLake[] =
    "-- record -a -e cpu_core/r1012/ -e cpu_atom/r1008/ -c 350003";

const char kPerfLLCMissesCmd[] = "-- record -a -e r412e -g -c 30007";
// Precise events (request zero skid) for last level cache misses.
const char kPerfLLCMissesPreciseCmd[] = "-- record -a -e r412e:pp -g -c 30007";

// Atom CPUs starting with Goldmont and big Intel cores starting with Haswell
// support Data Linear Address in PEBS. Collecting data addresses requires the
// use of precise events.
//
// On Goldmont & GoldmontPlus.
const char kPerfDTLBMissesDAPGoldmont[] =
    "-- record -a -e mem_uops_retired.dtlb_miss_loads:pp -c 2003 -d";

// Tremont on kernel 5.4 doesn't support the event name, but it supports the raw
// event code.
// AlderLake on kernel 5.10 doesn't support the event name, but it supports the
// raw event code.
const char kPerfDTLBMissesDAPTremont[] = "-- record -a -e r11d0:pp -c 2003 -d";

// On Haswell, Broadwell.
const char kPerfDTLBMissesDAPHaswell[] =
    "-- record -a -e mem_uops_retired.stlb_miss_loads:pp -c 2003 -d";

// On big Intel cores from Skylake forward.
const char kPerfDTLBMissesDAPSkylake[] =
    "-- record -a -e mem_inst_retired.stlb_miss_loads:pp -c 2003 -d";

// ETM for ARM boards including trogdor and herobrine.
const char kPerfETMCmd[] =
    "--run_inject --inject_args inject;--itrace=i512il;--strip -- record -a -e "
    "cs_etm/autofdo/";

const std::vector<RandomSelector::WeightAndValue> GetDefaultCommands_x86_64(
    const CPUIdentity& cpuid) {
  using WeightAndValue = RandomSelector::WeightAndValue;
  std::vector<WeightAndValue> cmds;
  DCHECK_EQ(cpuid.arch, "x86_64");
  const std::string cpu_uarch = GetCpuUarch(cpuid);

  // We use different perf events for iTLB, dTLB and LBR profiling on different
  // microarchitectures. Customize each command based on the microarchitecture.
  const char* itlb_miss_cycles_cmd = kPerfITLBMissCyclesCmdIvyBridge;
  const char* dtlb_miss_cycles_cmd = kPerfDTLBMissCyclesCmdIvyBridge;
  const char* lbr_cmd = kPerfLBRCmd;
  const char* cycles_cmd = kPerfCyclesHGCmd;
  const char* fp_callgraph_cmd = kPerfFPCallgraphHGCmd;
  const char* lbr_callgraph_cmd = kPerfLBRCallgraphCmd;
  const char* dap_dtlb_miss_cmd = nullptr;

  if (cpu_uarch == "Skylake" || cpu_uarch == "Kabylake" ||
      cpu_uarch == "Tigerlake" || cpu_uarch == "GoldmontPlus") {
    itlb_miss_cycles_cmd = kPerfITLBMissCyclesCmdSkylake;
    dtlb_miss_cycles_cmd = kPerfDTLBMissCyclesCmdSkylake;
  } else if (cpu_uarch == "Tremont" || cpu_uarch == "Gracemont") {
    itlb_miss_cycles_cmd = kPerfITLBMissCyclesCmdTremont;
    dtlb_miss_cycles_cmd = kPerfDTLBMissCyclesCmdTremont;
  } else if (cpu_uarch == "Silvermont" || cpu_uarch == "Airmont" ||
      cpu_uarch == "Goldmont") {
    itlb_miss_cycles_cmd = kPerfITLBMissCyclesCmdAtom;
    dtlb_miss_cycles_cmd = kPerfDTLBMissCyclesCmdAtom;
  } else if (cpu_uarch == "AlderLake" || cpu_uarch == "RaptorLake") {
    itlb_miss_cycles_cmd = kPerfITLBMissCyclesCmdAlderLake;
    dtlb_miss_cycles_cmd = kPerfDTLBMissCyclesCmdAlderLake;
  }
  if (cpu_uarch == "Silvermont" || cpu_uarch == "Airmont" ||
      cpu_uarch == "Goldmont" || cpu_uarch == "GoldmontPlus") {
    lbr_cmd = kPerfLBRCmdAtom;
  } else if (cpu_uarch == "Tremont" || cpu_uarch == "Gracemont") {
    lbr_cmd = kPerfLBRCmdTremont;
  } else if (cpu_uarch == "AlderLake" || cpu_uarch == "RaptorLake") {
    lbr_cmd = kPerfLBRCmdAlderLake;
  }
  if (cpu_uarch == "Skylake" || cpu_uarch == "Kabylake" ||
      cpu_uarch == "Tigerlake" || cpu_uarch == "IceLake" ||
      cpu_uarch == "CometLake") {
    dap_dtlb_miss_cmd = kPerfDTLBMissesDAPSkylake;
  } else if (cpu_uarch == "Goldmont" || cpu_uarch == "GoldmontPlus") {
    dap_dtlb_miss_cmd = kPerfDTLBMissesDAPGoldmont;
  } else if (cpu_uarch == "Haswell" || cpu_uarch == "Broadwell") {
    dap_dtlb_miss_cmd = kPerfDTLBMissesDAPHaswell;
  } else if (cpu_uarch == "Tremont" || cpu_uarch == "AlderLake" ||
             cpu_uarch == "RaptorLake" || cpu_uarch == "Gracemont") {
    dap_dtlb_miss_cmd = kPerfDTLBMissesDAPTremont;
  }

  if (MicroarchitectureHasCyclesPPPEvent(cpu_uarch)) {
    fp_callgraph_cmd = kPerfFPCallgraphPPPHGCmd;
    // Enable precise events for cycles.flat and cycles.lbr only if the kernel
    // has the fix for flushing PEBS on context switch.
    if (KernelReleaseHasPEBSFlushingFix(cpuid.release)) {
      cycles_cmd = kPerfCyclesPPPHGCmd;
      lbr_callgraph_cmd = kPerfLBRCallgraphPPPCmd;
    }
  }

  if (dap_dtlb_miss_cmd != nullptr) {
    cmds.emplace_back(45.0, cycles_cmd);
  } else {
    cmds.emplace_back(50.0, cycles_cmd);
  }

  // Haswell and newer big Intel cores support LBR callstack profiling. This
  // requires kernel support, which was added in kernel 4.4, and it was
  // backported to kernel 3.18. Collect LBR callstack profiling where
  // supported in addition to FP callchains. The former works with binaries
  // compiled with frame pointers disabled, but it only captures callchains
  // after profiling is enabled, so it's likely missing the lower frames of
  // the callstack.
  if (MicroarchitectureHasLBRCallgraph(cpu_uarch) &&
      KernelReleaseHasLBRCallgraph(cpuid.release)) {
    cmds.emplace_back(10.0, fp_callgraph_cmd);
    cmds.emplace_back(10.0, lbr_callgraph_cmd);
  } else {
    cmds.emplace_back(20.0, fp_callgraph_cmd);
  }

  if (dap_dtlb_miss_cmd != nullptr) {
    cmds.emplace_back(5.0, dap_dtlb_miss_cmd);
  }

  if (cpu_uarch == "IvyBridge" || cpu_uarch == "Haswell" ||
      cpu_uarch == "Broadwell" || cpu_uarch == "SandyBridge" ||
      cpu_uarch == "Skylake" || cpu_uarch == "Kabylake" ||
      cpu_uarch == "Tigerlake" || cpu_uarch == "Silvermont" ||
      cpu_uarch == "Airmont" || cpu_uarch == "Goldmont" ||
      cpu_uarch == "GoldmontPlus" || cpu_uarch == "Tremont" ||
      cpu_uarch == "AlderLake" || cpu_uarch == "RaptorLake" ||
      cpu_uarch == "Gracemont") {
    cmds.emplace_back(15.0, lbr_cmd);
    cmds.emplace_back(5.0, itlb_miss_cycles_cmd);
    cmds.emplace_back(5.0, dtlb_miss_cycles_cmd);
    // Record precise events on last level cache misses whenever the hardware
    // supports.
    if (cpu_uarch == "Goldmont" || cpu_uarch == "GoldmontPlus" ||
        cpu_uarch == "Tremont" || cpu_uarch == "AlderLake" ||
        cpu_uarch == "RaptorLake" || cpu_uarch == "Gracemont") {
      cmds.emplace_back(5.0, kPerfLLCMissesPreciseCmd);
    } else {
      cmds.emplace_back(5.0, kPerfLLCMissesCmd);
    }
    return cmds;
  }
  // Other 64-bit x86. We collect LLC misses for other Intel CPUs, but not for
  // non-Intel CPUs such as AMD, since the event code provided for LLC is
  // Intel specific.
  if (cpuid.vendor == "GenuineIntel") {
    cmds.emplace_back(25.0, cycles_cmd);
    cmds.emplace_back(5.0, kPerfLLCMissesCmd);
  } else {
    cmds.emplace_back(30.0, cycles_cmd);
  }
  return cmds;
}

std::vector<RandomSelector::WeightAndValue> GetDefaultCommands_aarch64(
    const std::string& model) {
  using WeightAndValue = RandomSelector::WeightAndValue;
  std::vector<WeightAndValue> cmds;

  if (base::FeatureList::IsEnabled(kCWPCollectsETM) &&
      (model == "TROGDOR" || model == "STRONGBAD" || model == "HEROBRINE")) {
    cmds.emplace_back(50.0, kPerfCyclesHGCmd);
    cmds.emplace_back(20.0, kPerfFPCallgraphHGCmd);
    cmds.emplace_back(30.0, kPerfETMCmd);
  } else {
    cmds.emplace_back(80.0, kPerfCyclesHGCmd);
    cmds.emplace_back(20.0, kPerfFPCallgraphHGCmd);
  }
  return cmds;
}

}  // namespace

namespace internal {

std::vector<RandomSelector::WeightAndValue> GetDefaultCommandsForCpuModel(
    const CPUIdentity& cpuid,
    const std::string& model) {
  using WeightAndValue = RandomSelector::WeightAndValue;

  if (cpuid.arch == "x86_64")  // 64-bit x86
    return GetDefaultCommands_x86_64(cpuid);

  if (cpuid.arch == "aarch64")  // ARM64
    return GetDefaultCommands_aarch64(model);

  std::vector<WeightAndValue> cmds;
  if (cpuid.arch == "x86" ||     // 32-bit x86, or...
      cpuid.arch == "armv7l") {  // ARM32
    cmds.emplace_back(80.0, kPerfCyclesHGCmd);
    cmds.emplace_back(20.0, kPerfFPCallgraphHGCmd);
    return cmds;
  }

  // Unknown CPUs
  cmds.emplace_back(1.0, kPerfCyclesHGCmd);
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
  debugd_client_provider_ = std::make_unique<ash::DebugDaemonClientProvider>();

  auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&PerfCollector::ParseCPUFrequencies, task_runner,
                     weak_factory_.GetWeakPtr(), /*attempt=*/1,
                     /*max_retries=*/3));

  CHECK(command_selector_.SetOdds(internal::GetDefaultCommandsForCpuModel(
      GetCPUIdentity(), base::SysInfo::HardwareModelName())));
  std::map<std::string, std::string> params;
  if (base::GetFieldTrialParams(kCWPFieldTrialName, &params)) {
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
    collector_params.collection_duration = base::Seconds(value);
  }
  if (GetInt64Param(params, "PeriodicProfilingIntervalMs", &value)) {
    collector_params.periodic_interval = base::Milliseconds(value);
  }
  if (GetInt64Param(params, "ResumeFromSuspend::SamplingFactor", &value)) {
    collector_params.resume_from_suspend.sampling_factor = value;
  }
  if (GetInt64Param(params, "ResumeFromSuspend::MaxDelaySec", &value)) {
    collector_params.resume_from_suspend.max_collection_delay =
        base::Seconds(value);
  }
  if (GetInt64Param(params, "RestoreSession::SamplingFactor", &value)) {
    collector_params.restore_session.sampling_factor = value;
  }
  if (GetInt64Param(params, "RestoreSession::MaxDelaySec", &value)) {
    collector_params.restore_session.max_collection_delay =
        base::Seconds(value);
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
    std::string weight_str = val.substr(0, split);

    double weight;
    if (!(base::StringToDouble(weight_str, &weight) && weight > 0.0))
      continue;  // Just drop invalid commands.
    std::string command(val.begin() + split + 1, val.end());
    commands.push_back(RandomSelector::WeightAndValue(weight, command));
  }
  command_selector_.SetOdds(commands);
}

std::unique_ptr<PerfOutputCall> PerfCollector::CreatePerfOutputCall(
    const std::vector<std::string>& perf_args,
    bool disable_cpu_idle,
    PerfOutputCall::DoneCallback callback) {
  DCHECK(debugd_client_provider_.get());
  return std::make_unique<PerfOutputCall>(
      debugd_client_provider_->debug_daemon_client(), perf_args,
      disable_cpu_idle, std::move(callback));
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
    base::ranges::copy(max_frequencies_mhz_,
                       google::protobuf::RepeatedFieldBackInserter(
                           sampled_profile->mutable_cpu_max_frequency_mhz()));
  }

  bool posted = base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&PerfCollector::PostCollectionProfileAnnotation,
                     sampled_profile.get(), has_cycles),
      base::BindOnce(&PerfCollector::SaveSerializedPerfProto,
                     weak_factory_.GetWeakPtr(), std::move(sampled_profile),
                     std::move(perf_stdout)));
  DCHECK(posted);
}

// static.
void PerfCollector::PostCollectionProfileAnnotation(
    SampledProfile* sampled_profile,
    bool has_cycles) {
  CollectProcessTypes(sampled_profile);
  if (has_cycles)
    PerfCollector::CollectPSICPU(sampled_profile, kPSICPUPath);
}

// static.
void PerfCollector::CollectProcessTypes(SampledProfile* sampled_profile) {
  std::vector<uint32_t> lacros_pids;
  std::string lacros_path;
  std::map<uint32_t, Process> process_types =
      ProcessTypeCollector::ChromeProcessTypes(lacros_pids, lacros_path);
  std::map<uint32_t, Thread> thread_types =
      ProcessTypeCollector::ChromeThreadTypes();
  if (!process_types.empty() && !thread_types.empty()) {
    sampled_profile->mutable_process_types()->insert(process_types.begin(),
                                                     process_types.end());
    sampled_profile->mutable_thread_types()->insert(thread_types.begin(),
                                                    thread_types.end());
  }
  if (!lacros_pids.empty()) {
    sampled_profile->mutable_lacros_pids()->Add(lacros_pids.begin(),
                                                lacros_pids.end());
  }
  if (!lacros_path.empty()) {
    metrics::SystemProfileProto_Channel channel;
    std::string version;
    if (PerfCollector::LacrosChannelAndVersion(lacros_path, channel, version)) {
      sampled_profile->set_lacros_channel(channel);
      sampled_profile->set_lacros_version(version);
    }
  }
}

// static.
void PerfCollector::CollectPSICPU(SampledProfile* sampled_profile,
                                  const std::string& psi_cpu_path) {
  // Example file content: some avg10=0.00 avg60=0.00 avg300=0.00 total=0
  const char kContentPrefix[] = "some";
  std::string content;
  if (!ReadFileToString(base::FilePath(psi_cpu_path), &content)) {
    base::UmaHistogramEnumeration(kParsePSICPUHistogramName,
                                  ParsePSICPUStatus::kReadFileFailed);
    return;
  }
  base::StringPairs kv_pairs;
  if (content.rfind(kContentPrefix) != 0 ||
      !base::SplitStringIntoKeyValuePairs(content.substr(5), '=', ' ',
                                          &kv_pairs)) {
    base::UmaHistogramEnumeration(kParsePSICPUHistogramName,
                                  ParsePSICPUStatus::kUnexpectedDataFormat);
    return;
  }
  // The first pair has PSI CPU data for the last 10 seconds and the second
  // pair has PSI CPU data for the last 60 seconds.
  double psi_cpu_last_10s_pct;
  double psi_cpu_last_60s_pct;
  if (!base::StringToDouble(kv_pairs[0].second, &psi_cpu_last_10s_pct) ||
      !base::StringToDouble(kv_pairs[1].second, &psi_cpu_last_60s_pct)) {
    base::UmaHistogramEnumeration(kParsePSICPUHistogramName,
                                  ParsePSICPUStatus::kParsePSIValueFailed);
    return;
  }

  base::UmaHistogramEnumeration(kParsePSICPUHistogramName,
                                ParsePSICPUStatus::kSuccess);
  sampled_profile->set_psi_cpu_last_10s_pct(
      static_cast<float>(psi_cpu_last_10s_pct));
  sampled_profile->set_psi_cpu_last_60s_pct(
      static_cast<float>(psi_cpu_last_60s_pct));
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

// static
PerfCollector::EventType PerfCollector::CommandEventType(
    const std::vector<std::string>& args) {
  if (args.size() < 4)
    return EventType::kOther;

  bool isRecord = false;
  for (size_t i = 0; i + 1 < args.size(); ++i) {
    if (!isRecord && args[i] == "record") {
      isRecord = true;
      continue;
    }
    if (isRecord && args[i] == "-e") {
      // Cycles event can be either the raw 'cycles' event, or the event name
      // can be annotated with some qualifier suffix. Check for all cases.
      if (args[i + 1] == "cycles" || args[i + 1].rfind("cycles:", 0) == 0)
        return EventType::kCycles;
      if (args[i + 1].rfind("cs_etm/autofdo", 0) == 0)
        return EventType::kETM;
    }
  }
  return EventType::kOther;
}

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

  // Prepend the duration to the command before splitting.
  std::vector<std::string> command = base::SplitString(
      base::StrCat({"--duration ",
                    base::NumberToString(
                        collection_params().collection_duration.InSeconds()),
                    " ", command_selector_.Select()}),
      kPerfCommandDelimiter, base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  auto event_type = CommandEventType(command);

  DCHECK(sampled_profile->has_trigger_event());
  current_trigger_ = sampled_profile->trigger_event();

  perf_output_call_ = CreatePerfOutputCall(
      command, event_type == EventType::kETM,
      base::BindOnce(&PerfCollector::OnPerfOutputComplete,
                     weak_factory_.GetWeakPtr(), std::move(incognito_observer),
                     std::move(sampled_profile),
                     event_type == EventType::kCycles));
}

// static
void PerfCollector::ParseCPUFrequencies(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::WeakPtr<PerfCollector> perf_collector,
    int attempt,
    int max_retries) {
  const char kCPUsDir[] = "/sys/devices/system/cpu/cpu%d";
  const std::string kCPUMaxFreqPathRel = "/cpufreq/cpuinfo_max_freq";
  int num_cpus = base::SysInfo::NumberOfProcessors();
  int num_zeros = 0;
  int num_found = 0;
  std::vector<uint32_t> frequencies_mhz;
  for (int i = 0; i < num_cpus; ++i) {
    std::string content;
    unsigned int frequency_khz = 0;
    auto path = base::StringPrintf(kCPUsDir, i);
    if (base::PathExists(base::FilePath(path))) {
      num_found++;
    } else {
      // We have seen the number of logical cores returned more than the
      // actual count.
      continue;
    }
    base::StrAppend(&path, {kCPUMaxFreqPathRel});
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
  // Save what we have even if we are going to retry. Collections are triggered
  // asynchronously, and we rather send partial CPU frequency data for any early
  // reports.
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&PerfCollector::SaveCPUFrequencies,
                                       perf_collector, frequencies_mhz));
  // Retry as long as the outcome is not successful and we didn't exhaust the
  // retry budget.
  if ((num_cpus == 0 || num_zeros > 0) && attempt < max_retries) {
    base::ThreadPool::PostDelayedTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&PerfCollector::ParseCPUFrequencies, task_runner,
                       perf_collector, attempt + 1, max_retries),
        base::Seconds(30 * attempt));
    return;
  }

  if (num_cpus == 0) {
    base::UmaHistogramEnumeration(kParseFrequenciesHistogramName,
                                  ParseFrequencyStatus::kNumCPUsIsZero);
  } else if (num_found < num_cpus) {
    base::UmaHistogramEnumeration(kParseFrequenciesHistogramName,
                                  ParseFrequencyStatus::kNumCPUsMoreThanPossible);
  } else if (num_zeros == num_cpus) {
    base::UmaHistogramEnumeration(kParseFrequenciesHistogramName,
                                  ParseFrequencyStatus::kAllZeroCPUFrequencies);
  } else if (num_zeros > 0) {
    base::UmaHistogramEnumeration(
        kParseFrequenciesHistogramName,
        ParseFrequencyStatus::kSomeZeroCPUFrequencies);
  } else if (attempt == 1) {
    base::UmaHistogramEnumeration(kParseFrequenciesHistogramName,
                                  ParseFrequencyStatus::kSuccess);
  } else {
    base::UmaHistogramEnumeration(kParseFrequenciesHistogramName,
                                  ParseFrequencyStatus::kSuccessOnRetry);
  }
}

void PerfCollector::SaveCPUFrequencies(
    const std::vector<uint32_t>& frequencies) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  max_frequencies_mhz_ = frequencies;
}

// static.
bool PerfCollector::LacrosChannelAndVersion(
    std::string_view lacros_path,
    metrics::SystemProfileProto_Channel& lacros_channel,
    std::string& lacros_version) {
  std::string channel;
  if (lacros_path == kRootfsLacrosPrefix) {
    base::UmaHistogramEnumeration(kParseLacrosPathHistogramName,
                                  ParseLacrosPath::kRootfs);
    return false;
  }
  if (!RE2::Consume(&lacros_path, *kLacrosChannelVersionMatcher, &channel,
                    &lacros_version)) {
    base::UmaHistogramEnumeration(kParseLacrosPathHistogramName,
                                  ParseLacrosPath::kUnrecognized);
    return false;
  }

  // We could also use the included parse helper, but it requires <channel>
  // converted to "CHANNEL_<CHANNEL>".
  if (channel == "stable")
    lacros_channel = SystemProfileProto_Channel_CHANNEL_STABLE;
  else if (channel == "beta")
    lacros_channel = SystemProfileProto_Channel_CHANNEL_BETA;
  else if (channel == "dev")
    lacros_channel = SystemProfileProto_Channel_CHANNEL_DEV;
  else if (channel == "canary")
    lacros_channel = SystemProfileProto_Channel_CHANNEL_CANARY;
  else
    lacros_channel = SystemProfileProto_Channel_CHANNEL_UNKNOWN;

  base::UmaHistogramEnumeration(kParseLacrosPathHistogramName,
                                ParseLacrosPath::kStateful);
  return true;
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

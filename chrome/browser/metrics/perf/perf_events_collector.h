// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PERF_PERF_EVENTS_COLLECTOR_H_
#define CHROME_BROWSER_METRICS_PERF_PERF_EVENTS_COLLECTOR_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/perf/metric_collector.h"
#include "chrome/browser/metrics/perf/perf_output.h"
#include "chrome/browser/metrics/perf/random_selector.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace ash {
class DebugDaemonClientProvider;
}

namespace base {
class SequencedTaskRunner;
}

namespace metrics {

struct CPUIdentity;
class WindowedIncognitoObserver;

BASE_DECLARE_FEATURE(kCWPCollectsETM);

// Enables collection of perf events profile data. perf aka "perf events" is a
// performance profiling infrastructure built into the linux kernel. For more
// information, see: https://perf.wiki.kernel.org/index.php/Main_Page.
class PerfCollector : public internal::MetricCollector {
 public:
  PerfCollector();

  PerfCollector(const PerfCollector&) = delete;
  PerfCollector& operator=(const PerfCollector&) = delete;

  // MetricCollector:
  ~PerfCollector() override;
  const char* ToolName() const override;

 protected:
  // For testing to mock PerfOutputCall.
  virtual std::unique_ptr<PerfOutputCall> CreatePerfOutputCall(
      const std::vector<std::string>& quipper_args,
      bool disable_cpu_idle,
      PerfOutputCall::DoneCallback callback);

  void OnPerfOutputComplete(
      std::unique_ptr<WindowedIncognitoObserver> incognito_observer,
      std::unique_ptr<SampledProfile> sampled_profile,
      bool has_cycles,
      std::string perf_stdout);

  // Parses a PerfDataProto from serialized data |perf_stdout|, if non-empty.
  // If |perf_stdout| is empty, it is counted as an error. |incognito_observer|
  // indicates whether an incognito window had been opened during the profile
  // collection period. If there was an incognito window, discard the incoming
  // data.
  void ParseOutputProtoIfValid(
      std::unique_ptr<WindowedIncognitoObserver> incognito_observer,
      std::unique_ptr<SampledProfile> sampled_profile,
      bool has_cycles,
      std::string perf_stdout);

  // MetricCollector:
  void SetUp() override;
  base::WeakPtr<internal::MetricCollector> GetWeakPtr() override;
  bool ShouldCollect() const override;
  void CollectProfile(std::unique_ptr<SampledProfile> sampled_profile) override;
  void StopCollection() override;

  const RandomSelector& command_selector() const { return command_selector_; }

  // Collects both Ash and Lacros Chrome process and thread types.
  static void CollectProcessTypes(SampledProfile* sampled_profile);

  // Executes asynchronously on another thread pool and retries with a delay if
  // all frequencies couldn't be read. Posts a task on the given task_runner
  // after each attempt.
  static void ParseCPUFrequencies(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::WeakPtr<PerfCollector> perf_collector,
      int attempt,
      int max_retries);
  // Saves the given frequencies to |max_frequencies_mhz_|.
  void SaveCPUFrequencies(const std::vector<uint32_t>& frequencies);

  const std::vector<uint32_t>& max_frequencies_mhz() const {
    return max_frequencies_mhz_;
  }

  // Enumeration representing success and various failure modes for parsing CPU
  // frequencies. These values are persisted to logs. Entries should not be
  // renumbered and numeric values should never be reused.
  enum class ParseFrequencyStatus {
    kSuccess,
    kNumCPUsIsZero,
    kSomeZeroCPUFrequencies,
    kAllZeroCPUFrequencies,
    kSuccessOnRetry,
    kNumCPUsMoreThanPossible,
    // Magic constant used by the histogram macros.
    kMaxValue = kNumCPUsMoreThanPossible,
  };

  // Extracts the |lacros_channel| and |lacros_version| from |lacros_path|.
  static bool LacrosChannelAndVersion(
      std::string_view lacros_path,
      metrics::SystemProfileProto_Channel& lacros_channel,
      std::string& lacros_version);

  // Enumeration of various locations gotten from parsing a Lacros binary path.
  // This is used to monitor any change to the Lacros path.
  enum class ParseLacrosPath {
    kRootfs,
    kStateful,
    kUnrecognized,
    // Magic constant used by the histogram macros.
    kMaxValue = kUnrecognized,
  };

  // Annotations on the collected sampled_profile, including adding process
  // types and PSI CPU data.
  static void PostCollectionProfileAnnotation(SampledProfile* sampled_profile,
                                              bool has_cycles);

  // Collect PSI CPU data and add to sampled_profile.
  static void CollectPSICPU(SampledProfile* sampled_profile,
                            const std::string& psi_cpu_path);

  // Enumeration representing success and various failure modes for parsing PSI
  // CPU data. These values are persisted to logs. Entries should not be
  // renumbered and numeric values should never be reused.
  enum class ParsePSICPUStatus {
    kSuccess,
    kReadFileFailed,
    kUnexpectedDataFormat,
    kParsePSIValueFailed,
    // Magic constant used by the histogram macros.
    kMaxValue = kParsePSIValueFailed,
  };

  SampledProfile::TriggerEvent current_trigger_ =
      SampledProfile::UNKNOWN_TRIGGER_EVENT;

  // Enumeration representing event types that need additional treatment
  // during or after the collection.
  enum class EventType {
    kOther,
    kCycles,
    kETM,
  };
  static EventType CommandEventType(const std::vector<std::string>& args);

 private:
  // Change the values in |collection_params_| and the commands in
  // |command_selector| for any keys that are present in |params|.
  void SetCollectionParamsFromVariationParams(
      const std::map<std::string, std::string>& params);

  // Set of commands to choose from.
  RandomSelector command_selector_;

  // |debugd_client_provider_| hosts the private DBus connection to debugd.
  std::unique_ptr<ash::DebugDaemonClientProvider> debugd_client_provider_;

  // An active call to perf/quipper, if set.
  std::unique_ptr<PerfOutputCall> perf_output_call_;

  // Vector of max frequencies associated with each logical CPU. Computed
  // asynchronously at start.
  std::vector<uint32_t> max_frequencies_mhz_;

  base::WeakPtrFactory<PerfCollector> weak_factory_{this};
};

// Exposed for unit testing.
namespace internal {

// Return the default set of perf commands and their odds of selection given
// the identity of the CPU in |cpuid|.
std::vector<RandomSelector::WeightAndValue> GetDefaultCommandsForCpuModel(
    const CPUIdentity& cpuid,
    const std::string& model);

// For the "PerfCommand::"-prefixed keys in |params|, return the cpu specifier
// that is the narrowest match for the CPU identified by |cpuid|.
// Valid CPU specifiers, in increasing order of specificity, are:
// "default", a system architecture (e.g. "x86_64"), a CPU microarchitecture
// (currently only some Intel and AMD uarchs supported), or a CPU model name
// substring.
std::string FindBestCpuSpecifierFromParams(
    const std::map<std::string, std::string>& params,
    const CPUIdentity& cpuid);

// Returns if the given perf command samples CPU cycles.
bool CommandSamplesCPUCycles(const std::vector<std::string>& args);

}  // namespace internal

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_PERF_PERF_EVENTS_COLLECTOR_H_

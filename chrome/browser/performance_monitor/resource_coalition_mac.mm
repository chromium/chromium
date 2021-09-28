// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/resource_coalition_mac.h"

#include <Foundation/Foundation.h>
#include <libproc.h>
#include <cstdint>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_ioobject.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process_handle.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/performance_monitor/resource_coalition_internal_types_mac.h"

extern "C" int coalition_info_resource_usage(
    uint64_t cid,
    struct coalition_resource_usage* cru,
    size_t sz);

namespace performance_monitor {
namespace {

const char kCoalitionAvailabilityHistogram[] =
    "PerformanceMonitor.ResourceCoalition.Availability";

// Details about whether or not it's possible to get coalition resource usage
// data on the system.
// This enum is reporting in metrics. Do not reorder; add additional values at
// the end and update the CoalitionIDAvailability enum in enums.xml.
enum class CoalitionAvailability {
  kAvailable = 0,
  kCoalitionIDNotAvailable = 1,
  kCoalitionResourceUsageNotAvailable = 2,
  kUnabletoGetParentCoalitionId = 3,
  kNotAloneInCoalition = 4,
  kMaxValue = kNotAloneInCoalition
};

// Returns the coalition ID that a given process belongs to, or nullopt if this
// information isn't available.
absl::optional<uint64_t> GetProcessCoalitionId(const base::ProcessId pid) {
  proc_pidcoalitioninfo coalition_info = {};
  int res = proc_pidinfo(pid, PROC_PIDCOALITIONINFO, 0, &coalition_info,
                         sizeof(coalition_info));

  if (res != sizeof(coalition_info))
    return absl::nullopt;

  return coalition_info.coalition_id[COALITION_TYPE_RESOURCE];
}

// Returns the coalition ID that the current process belongs to. If this isn't
// available or deemed not usable (e.g. if the process is not alone in its
// coalition) this will return nullopt and |availability_details| will receive
// some details about why this failed, otherwise this will return the ID and
// |availability_details| will have a success value.
absl::optional<uint64_t> GetCurrentCoalitionId(
    CoalitionAvailability* availability_details) {
  DCHECK(availability_details);
  auto cid = GetProcessCoalitionId(base::GetCurrentProcId());

  if (!cid.has_value()) {
    *availability_details = CoalitionAvailability::kCoalitionIDNotAvailable;
    return absl::nullopt;
  }

  // Check if resource usage metrics can be retrieved for this coalition ID.
  coalition_resource_usage cru = {};
  uint64_t res = coalition_info_resource_usage(cid.value(), &cru, sizeof(cru));
  if (res != 0) {
    *availability_details =
        CoalitionAvailability::kCoalitionResourceUsageNotAvailable;
    return absl::nullopt;
  }

  auto parent_cid = GetProcessCoalitionId(
      base::GetParentProcessId(base::GetCurrentProcessHandle()));

  if (!parent_cid.has_value()) {
    *availability_details =
        CoalitionAvailability::kUnabletoGetParentCoalitionId;
    return absl::nullopt;
  }

  // Do not report metrics if the coalition ID is shared with the parent
  // process.
  if (parent_cid.value() == cid.value()) {
    *availability_details = CoalitionAvailability::kNotAloneInCoalition;
    return absl::nullopt;
  }

  *availability_details = CoalitionAvailability::kAvailable;
  return cid;
}

// Returns the resource usage coalition data for the given coalition ID.
// This assumes that resource coalition data are always available for a given
// coalition ID (i.e. the coalition has a lifetime that exceeds the usage of the
// ID).
std::unique_ptr<coalition_resource_usage> GetResourceUsageData(
    int64_t coalition_id) {
  auto cru = std::make_unique<coalition_resource_usage>();
  uint64_t res = coalition_info_resource_usage(
      coalition_id, cru.get(), sizeof(coalition_resource_usage));
  DCHECK_EQ(0U, res);

  return cru;
}

NSDictionary* MaybeGetDictionaryFromPath(const base::FilePath& path) {
  // The folder where the energy coefficient plist files are stored.
  NSString* plist_path_string = base::SysUTF8ToNSString(path.value().c_str());
  return [NSDictionary dictionaryWithContentsOfFile:plist_path_string];
}

double GetNamedCoefficientOrZero(NSDictionary* dict, NSString* key) {
  NSObject* value = [dict objectForKey:key];
  NSNumber* num = base::mac::ObjCCast<NSNumber>(value);
  return [num floatValue];
}

}  // namespace

ResourceCoalition::DataRate::DataRate() = default;
ResourceCoalition::DataRate::DataRate(const DataRate& other) = default;
ResourceCoalition::DataRate& ResourceCoalition::DataRate::operator=(
    const DataRate& other) = default;
ResourceCoalition::DataRate::~DataRate() = default;

ResourceCoalition::ResourceCoalition() {
  CoalitionAvailability availability_details;
  SetCoalitionId(GetCurrentCoalitionId(&availability_details));
  base::UmaHistogramEnumeration(kCoalitionAvailabilityHistogram,
                                availability_details);

  // Initialize the machine timebase.
  kern_return_t kr = mach_timebase_info(&mach_timebase_);
  DCHECK_EQ(kr, KERN_SUCCESS);
  DCHECK(mach_timebase_.numer);
  DCHECK(mach_timebase_.denom);
}
ResourceCoalition::~ResourceCoalition() = default;

absl::optional<ResourceCoalition::DataRate> ResourceCoalition::GetDataRate() {
  DCHECK(IsAvailable());
  DCHECK_EQ(GetProcessCoalitionId(base::GetCurrentProcId()).value(),
            coalition_id_.value());
  DCHECK(last_data_sample_);
  return GetDataRateImpl(GetResourceUsageData(coalition_id_.value()),
                         base::TimeTicks::Now());
}

absl::optional<ResourceCoalition::DataRate>
ResourceCoalition::GetDataRateFromFakeDataForTesting(
    std::unique_ptr<coalition_resource_usage> old_data_sample,
    std::unique_ptr<coalition_resource_usage> recent_data_sample,
    base::TimeDelta interval_length) {
  last_data_sample_.swap(old_data_sample);
  auto now = base::TimeTicks::Now();
  last_data_sample_timestamp_ = now - interval_length;
  return GetDataRateImpl(std::move(recent_data_sample), now);
}

void ResourceCoalition::SetCoalitionIDToCurrentProcessIdForTesting() {
  SetCoalitionId(GetProcessCoalitionId(base::GetCurrentProcId()));
}

// static
absl::optional<ResourceCoalition::EnergyImpactCoefficients>
ResourceCoalition::ReadEnergyImpactCoefficientsFromPath(
    const base::FilePath& plist_file) {
  @autoreleasepool {
    NSDictionary* dict = MaybeGetDictionaryFromPath(plist_file);
    if (!dict)
      return absl::nullopt;

    // We want the energy_constants sub-dictionary.
    NSDictionary* energy_constants = [dict objectForKey:@"energy_constants"];
    if (!energy_constants)
      return absl::nullopt;

    EnergyImpactCoefficients coefficients{};
    coefficients.kcpu_time =
        GetNamedCoefficientOrZero(energy_constants, @"kcpu_time");
    coefficients.kcpu_wakeups =
        GetNamedCoefficientOrZero(energy_constants, @"kcpu_wakeups");

    coefficients.kqos_default =
        GetNamedCoefficientOrZero(energy_constants, @"kqos_default");
    coefficients.kqos_background =
        GetNamedCoefficientOrZero(energy_constants, @"kqos_background");
    coefficients.kqos_utility =
        GetNamedCoefficientOrZero(energy_constants, @"kqos_utility");
    coefficients.kqos_legacy =
        GetNamedCoefficientOrZero(energy_constants, @"kqos_legacy");
    coefficients.kqos_user_initiated =
        GetNamedCoefficientOrZero(energy_constants, @"kqos_user_initiated");
    coefficients.kqos_user_interactive =
        GetNamedCoefficientOrZero(energy_constants, @"kqos_user_interactive");

    coefficients.kdiskio_bytesread =
        GetNamedCoefficientOrZero(energy_constants, @"kdiskio_bytesread");
    coefficients.kdiskio_byteswritten =
        GetNamedCoefficientOrZero(energy_constants, @"kdiskio_byteswritten");

    coefficients.kgpu_time =
        GetNamedCoefficientOrZero(energy_constants, @"kgpu_time");

    coefficients.knetwork_recv_bytes =
        GetNamedCoefficientOrZero(energy_constants, @"knetwork_recv_bytes");
    coefficients.knetwork_recv_packets =
        GetNamedCoefficientOrZero(energy_constants, @"knetwork_recv_packets");
    coefficients.knetwork_sent_bytes =
        GetNamedCoefficientOrZero(energy_constants, @"knetwork_sent_bytes");
    coefficients.knetwork_sent_packets =
        GetNamedCoefficientOrZero(energy_constants, @"knetwork_sent_packets");

    return coefficients;
  }
}

// static
absl::optional<ResourceCoalition::EnergyImpactCoefficients>
ResourceCoalition::ReadEnergyImpactOrDefaultForBoardId(
    const base::FilePath& directory,
    const std::string& board_id) {
  auto coefficients = ReadEnergyImpactCoefficientsFromPath(
      directory.Append(board_id).AddExtension(FILE_PATH_LITERAL("plist")));
  if (coefficients.has_value())
    return coefficients;

  return ReadEnergyImpactCoefficientsFromPath(
      directory.Append(FILE_PATH_LITERAL("default.plist")));
}

double ResourceCoalition::ComputeEnergyImpactForCoalitionUsage(
    const EnergyImpactCoefficients& coefficients,
    const coalition_resource_usage& data_sample) {
  // TODO(https://crbug.com/1249536): The below coefficients are not used for
  //    now. Their units are unknown, and in the case of the network-related
  //    coefficients, it's not clear how to sample the data.
  // coefficients.kdiskio_bytesread;
  // coefficients.kdiskio_byteswritten;
  // coefficients.knetwork_recv_bytes;
  // coefficients.knetwork_recv_packets;
  // coefficients.knetwork_sent_bytes;
  // coefficients.knetwork_sent_packets;

  // The cumulative CPU usage in |data_sample| is in units of ns, and
  // |cpu_time_equivalent_ns| is computed in CPU ns up to the end of this
  // function, where it's converted to units of EnergyImpact.
  double cpu_time_equivalent_ns = 0.0;

  // The kcpu_wakeups coefficient on disk is in seconds, but our intermediate
  // result is in ns, so convert to ns on the fly.
  cpu_time_equivalent_ns += coefficients.kcpu_wakeups *
                            base::Time::kNanosecondsPerSecond *
                            data_sample.platform_idle_wakeups;

  // Presumably the kgpu_time coefficient has suitable units for the conversion
  // of GPU time energy to CPU time energy. There is a fairly wide spread on
  // this constant seen in /usr/share/pmenergy. On macOS 11.5.2 the spread is
  // from 0 through 5.9.
  cpu_time_equivalent_ns +=
      coefficients.kgpu_time * MachTimeToNs(data_sample.gpu_time);

  cpu_time_equivalent_ns +=
      coefficients.kqos_background *
      MachTimeToNs(data_sample.cpu_time_eqos[THREAD_QOS_BACKGROUND]);
  cpu_time_equivalent_ns +=
      coefficients.kqos_default *
      MachTimeToNs(data_sample.cpu_time_eqos[THREAD_QOS_DEFAULT]);
  cpu_time_equivalent_ns +=
      coefficients.kqos_legacy *
      MachTimeToNs(data_sample.cpu_time_eqos[THREAD_QOS_LEGACY]);
  cpu_time_equivalent_ns +=
      coefficients.kqos_user_initiated *
      MachTimeToNs(data_sample.cpu_time_eqos[THREAD_QOS_USER_INITIATED]);
  cpu_time_equivalent_ns +=
      coefficients.kqos_user_interactive *
      MachTimeToNs(data_sample.cpu_time_eqos[THREAD_QOS_USER_INTERACTIVE]);
  cpu_time_equivalent_ns +=
      coefficients.kqos_utility *
      MachTimeToNs(data_sample.cpu_time_eqos[THREAD_QOS_UTILITY]);

  // The conversion ratio for CPU time/EnergyImpact is ns/10ms
  constexpr double kNsToEI = 1E-7;
  return cpu_time_equivalent_ns * kNsToEI;
}

// static
absl::optional<std::string> ResourceCoalition::MaybeGetBoardIdForThisMachine() {
  base::mac::ScopedIOObject<io_service_t> platform_expert(
      IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceMatching("IOPlatformExpertDevice")));
  if (!platform_expert)
    return absl::nullopt;

  // This is what libpmenergy is observed to do in order to retrieve the correct
  // coefficients file for the local computer.
  base::ScopedCFTypeRef<CFDataRef> board_id_data(
      base::mac::CFCast<CFDataRef>(IORegistryEntryCreateCFProperty(
          platform_expert, CFSTR("board-id"), kCFAllocatorDefault, 0)));

  if (!board_id_data)
    return absl::nullopt;

  return reinterpret_cast<const char*>(CFDataGetBytePtr(board_id_data));
}

void ResourceCoalition::SetEnergyImpactCoefficientsForTesting(
    const absl::optional<EnergyImpactCoefficients>& coefficients) {
  energy_impact_coefficients_initialized_ = true;
  energy_impact_coefficients_ = coefficients;
}

void ResourceCoalition::SetMachTimebaseForTesting(
    const mach_timebase_info_data_t& mach_timebase) {
  mach_timebase_ = mach_timebase;
}

void ResourceCoalition::EnsureEnergyImpactCoefficientsIfAvailable() {
  if (energy_impact_coefficients_initialized_)
    return;
  energy_impact_coefficients_initialized_ = true;

  auto board_id = MaybeGetBoardIdForThisMachine();
  if (!board_id.has_value())
    return;

  energy_impact_coefficients_ = ReadEnergyImpactOrDefaultForBoardId(
      base::FilePath(FILE_PATH_LITERAL("/usr/share/pmenergy")),
      board_id.value());
}

void ResourceCoalition::SetCoalitionId(absl::optional<uint64_t> coalition_id) {
  coalition_id_ = coalition_id;
  if (coalition_id_.has_value()) {
    last_data_sample_ = GetResourceUsageData(coalition_id_.value());
    last_data_sample_timestamp_ = base::TimeTicks::Now();
  }
}

uint64_t ResourceCoalition::MachTimeToNs(uint64_t mach_time) {
  if (mach_timebase_.numer == mach_timebase_.denom)
    return mach_time;

  CHECK(
      !__builtin_umulll_overflow(mach_time, mach_timebase_.numer, &mach_time));
  return mach_time / mach_timebase_.denom;
}

absl::optional<ResourceCoalition::DataRate>
ResourceCoalition::GetCoalitionDataDiff(
    const coalition_resource_usage& new_sample,
    const coalition_resource_usage& old_sample,
    base::TimeDelta interval_length) {
  DCHECK(energy_impact_coefficients_initialized_);

  bool new_samples_exceeds_or_equals_old_ones =
      std::tie(new_sample.cpu_time, new_sample.interrupt_wakeups,
               new_sample.platform_idle_wakeups, new_sample.bytesread,
               new_sample.byteswritten, new_sample.gpu_time,
               new_sample.energy) >=
      std::tie(old_sample.cpu_time, old_sample.interrupt_wakeups,
               old_sample.platform_idle_wakeups, old_sample.bytesread,
               old_sample.byteswritten, old_sample.gpu_time, old_sample.energy);
  DCHECK_EQ(new_sample.cpu_time_eqos_len,
            static_cast<uint64_t>(COALITION_NUM_THREAD_QOS_TYPES));
  // Check for an overflow in the QoS data.
  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    if (new_sample.cpu_time_eqos[i] < old_sample.cpu_time_eqos[i]) {
      new_samples_exceeds_or_equals_old_ones = false;
      break;
    }
  }
  if (!new_samples_exceeds_or_equals_old_ones)
    return absl::nullopt;

  ResourceCoalition::DataRate ret;

  auto get_rate_per_second = [&interval_length](uint64_t new_sample,
                                                uint64_t old_sample) -> double {
    DCHECK_GE(new_sample, old_sample);
    uint64_t diff = new_sample - old_sample;
    return diff / interval_length.InSecondsF();
  };

  auto get_timedelta_rate_per_second = [&interval_length, self = this](
                                           uint64_t new_sample,
                                           uint64_t old_sample) -> double {
    DCHECK_GE(new_sample, old_sample);
    // Compute the delta in s, being careful to avoid truncation due to integral
    // division.
    double delta_sample_s =
        self->MachTimeToNs(new_sample - old_sample) /
        static_cast<double>(base::Time::kNanosecondsPerSecond);
    return delta_sample_s / interval_length.InSecondsF();
  };

  ret.cpu_time_per_second =
      get_timedelta_rate_per_second(new_sample.cpu_time, old_sample.cpu_time);
  ret.interrupt_wakeups_per_second = get_rate_per_second(
      new_sample.interrupt_wakeups, old_sample.interrupt_wakeups);
  ret.platform_idle_wakeups_per_second = get_rate_per_second(
      new_sample.platform_idle_wakeups, old_sample.platform_idle_wakeups);
  ret.bytesread_per_second =
      get_rate_per_second(new_sample.bytesread, old_sample.bytesread);
  ret.byteswritten_per_second =
      get_rate_per_second(new_sample.byteswritten, old_sample.byteswritten);
  ret.gpu_time_per_second =
      get_timedelta_rate_per_second(new_sample.gpu_time, old_sample.gpu_time);
  ret.power_nw = get_rate_per_second(new_sample.energy, old_sample.energy);

  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    ret.qos_time_per_second[i] = get_timedelta_rate_per_second(
        new_sample.cpu_time_eqos[i], old_sample.cpu_time_eqos[i]);
  }

  if (energy_impact_coefficients_.has_value()) {
    const EnergyImpactCoefficients& coefficients =
        energy_impact_coefficients_.value();

    ret.energy_impact_per_second =
        (ComputeEnergyImpactForCoalitionUsage(coefficients, new_sample) -
         ComputeEnergyImpactForCoalitionUsage(coefficients, old_sample)) /
        interval_length.InSecondsF();
  } else {
    // TODO(siggi): Use something else here as sentinel?
    ret.energy_impact_per_second = 0;
  }

  return ret;
}

absl::optional<ResourceCoalition::DataRate> ResourceCoalition::GetDataRateImpl(
    std::unique_ptr<coalition_resource_usage> new_data_sample,
    base::TimeTicks now) {
  // Make sure the EI coefficients are loaded, if possible.
  EnsureEnergyImpactCoefficientsIfAvailable();
  auto ret =
      GetCoalitionDataDiff(*new_data_sample.get(), *last_data_sample_.get(),
                           now - last_data_sample_timestamp_);
  last_data_sample_.swap(new_data_sample);
  last_data_sample_timestamp_ = now;

  return ret;
}

}  // namespace performance_monitor

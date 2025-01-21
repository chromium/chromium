// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_shared_memory.h"

#include <string_view>

#include "base/base_switches.h"
#include "base/debug/crash_logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/shared_memory_switch.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/process/process_info.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/unguessable_token.h"

// On Apple platforms, the shared memory handle is shared using a Mach port
// rendezvous key.
#if BUILDFLAG(IS_APPLE)
#include "base/apple/mach_port_rendezvous.h"
#endif

// On POSIX, the shared memory handle is a file_descriptor mapped in the
// GlobalDescriptors table.
#if BUILDFLAG(IS_POSIX)
#include "base/posix/global_descriptors.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/win_util.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/vmo.h>
#include <zircon/process.h>

#include "base/fuchsia/fuchsia_logging.h"
#endif

// This file supports passing a read/write histogram shared memory region
// between a parent process and child process. The information about the
// shared memory region is encoded into a command-line switch value.
//
// Format: "handle,[irp],guid-high,guid-low,size".
//
// The switch value is composed of 5 segments, separated by commas:
//
// 1. The platform-specific handle id for the shared memory as a string.
// 2. [irp] to indicate whether the handle is inherited (i, most platforms),
//    sent via rendezvous (r, MacOS), or should be queried from the parent
//    (p, Windows).
// 3. The high 64 bits of the shared memory block GUID.
// 4. The low 64 bits of the shared memory block GUID.
// 5. The size of the shared memory segment as a string.
//
// TODO(crbug.com/40109064): Refactor the common logic here and in
// base/metrics/field_trial.cc
namespace base {

BASE_FEATURE(kPassHistogramSharedMemoryOnLaunch,
             "PassHistogramSharedMemoryOnLaunch",
             FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_APPLE)
const MachPortsForRendezvous::key_type HistogramSharedMemory::kRendezvousKey =
    'hsmr';
#endif

HistogramSharedMemory::SharedMemory::SharedMemory(
    UnsafeSharedMemoryRegion r,
    std::unique_ptr<PersistentMemoryAllocator> a)
    : region(std::move(r)), allocator(std::move(a)) {
  CHECK(region.IsValid());
  CHECK(allocator);
}

HistogramSharedMemory::SharedMemory::~SharedMemory() = default;

HistogramSharedMemory::SharedMemory::SharedMemory(
    HistogramSharedMemory::SharedMemory&&) = default;

HistogramSharedMemory::SharedMemory&
HistogramSharedMemory::SharedMemory::operator=(
    HistogramSharedMemory::SharedMemory&&) = default;

// static
std::optional<HistogramSharedMemory::SharedMemory>
HistogramSharedMemory::Create(int process_id,
                              const HistogramSharedMemory::Config& config) {
  auto region = UnsafeSharedMemoryRegion::Create(config.memory_size_bytes);
  if (!region.IsValid()) {
    DVLOG(1) << "Failed to create shared memory region.";
    return std::nullopt;
  }
  auto mapping = region.Map();
  if (!mapping.IsValid()) {
    DVLOG(1) << "Failed to create shared memory mapping.";
    return std::nullopt;
  }

  return SharedMemory{std::move(region),
                      std::make_unique<WritableSharedPersistentMemoryAllocator>(
                          std::move(mapping), static_cast<uint64_t>(process_id),
                          config.allocator_name)};
}

// static
bool HistogramSharedMemory::PassOnCommandLineIsEnabled(
    std::string_view process_type) {
  // On ChromeOS and for "utility" processes on other platforms there seems to
  // be one or more mechanisms on startup which walk through all inherited
  // shared memory regions and take a read-only handle to them. When we later
  // attempt to deserialize the handle info and take a writable handle we
  // find that the handle is already owned in read-only mode, triggering
  // a crash due to "FD ownership violation".
  //
  // Example: The call to OpenSymbolFiles() in base/debug/stack_trace_posix.cc
  // grabs a read-only handle to the shmem region for some process types.
  //
  // TODO(crbug.com/40109064): Fix ChromeOS and utility processes.
  return (FeatureList::IsEnabled(kPassHistogramSharedMemoryOnLaunch)
#if BUILDFLAG(IS_CHROMEOS)
          && process_type != "gpu-process"
#elif BUILDFLAG(IS_ANDROID)
          && process_type != "utility"
#endif
  );
}

// static
void HistogramSharedMemory::AddToLaunchParameters(
    UnsafeSharedMemoryRegion histogram_shmem_region,
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
    GlobalDescriptors::Key descriptor_key,
    ScopedFD& descriptor_to_share,
#endif
    CommandLine* command_line,
    LaunchOptions* launch_options) {
  CHECK(command_line);

  const std::string process_type = command_line->GetSwitchValueASCII("type");
  const bool enabled = PassOnCommandLineIsEnabled(process_type);

  DVLOG(1) << (enabled ? "A" : "Not a")
           << "dding histogram shared memory launch parameters for "
           << process_type << " process.";

  if (!enabled) {
    return;
  }

  shared_memory::AddToLaunchParameters(::switches::kMetricsSharedMemoryHandle,
                                       std::move(histogram_shmem_region),
#if BUILDFLAG(IS_APPLE)
                                       kRendezvousKey,
#elif BUILDFLAG(IS_POSIX)
                                       descriptor_key, descriptor_to_share,
#endif
                                       command_line, launch_options);
}

// static
void HistogramSharedMemory::InitFromLaunchParameters(
    const CommandLine& command_line) {
  // TODO(crbug.com/40109064): Clean up once fully launched.
  if (!command_line.HasSwitch(switches::kMetricsSharedMemoryHandle)) {
    return;
  }
  CHECK(!GlobalHistogramAllocator::Get());
  DVLOG(1) << "Initializing histogram shared memory from command line for "
           << command_line.GetSwitchValueASCII("type");

  auto shmem_region = shared_memory::UnsafeSharedMemoryRegionFrom(
      command_line.GetSwitchValueASCII(switches::kMetricsSharedMemoryHandle));

  SCOPED_CRASH_KEY_NUMBER(
      "HistogramAllocator", "SharedMemError",
      static_cast<int>(shmem_region.has_value()
                           ? shared_memory::SharedMemoryError::kNoError
                           : shmem_region.error()));

  CHECK(shmem_region.has_value() && shmem_region.value().IsValid())
      << "Invald memory region passed on command line.";

  GlobalHistogramAllocator::CreateWithSharedMemoryRegion(shmem_region.value());

  auto* global_allocator = GlobalHistogramAllocator::Get();
  CHECK(global_allocator);
  global_allocator->CreateTrackingHistograms(global_allocator->Name());
}

}  // namespace base

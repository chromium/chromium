// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_shared_memory.h"

#include <string_view>

#include "base/base_switches.h"
#include "base/memory/shared_memory_mapping.h"
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
#include "base/mac/mach_port_rendezvous.h"
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
// TODO(crbug.com/1028263): Refactor the common logic here and in
// base/metrics/field_trial.cc
namespace base {

namespace {

// Serializes the shared memory region metadata to a string that can be added
// to the command-line of a child-process.
std::string SerializeSharedMemoryRegionMetadata(
    UnsafeSharedMemoryRegion shmem_region_to_share,
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
    GlobalDescriptors::Key descriptor_key,
    ScopedFD& descriptor_to_share,
#endif
    [[maybe_unused]] LaunchOptions* launch_options) {
#if !BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_APPLE)
  CHECK(launch_options != nullptr);
#endif

  CHECK(shmem_region_to_share.IsValid());

  auto shmem_region = UnsafeSharedMemoryRegion::TakeHandleForSerialization(
      std::move(shmem_region_to_share));
  auto shmem_token = shmem_region.GetGUID();
  auto shmem_size = shmem_region.GetSize();
  auto shmem_handle = shmem_region.PassPlatformHandle();

  CHECK(shmem_token);
  CHECK(shmem_size != 0u);

  // Reserve memory for the serialized value.
  // handle,method,hi,lo,size = 4 * 64-bit number + 1 char + 4 commas + NUL
  //                          = (4 * 20-max decimal char) + 6 chars
  //                          = 86 bytes
  constexpr size_t kSerializedReservedSize = 86;

  std::string serialized;
  serialized.reserve(kSerializedReservedSize);

#if BUILDFLAG(IS_WIN)
  // Ownership of the handle is passed to |launch_options|. We keep a non-
  // owning alias for a moment, so we can serialize the handle's numeric
  // value.
  HANDLE handle = shmem_handle.release();
  launch_options->handles_to_inherit.push_back(handle);

  // Tell the child process the name of the HANDLE and whether to handle can
  // be inherited ('i') or must be duplicate from the parent process ('p').
  base::StrAppend(&serialized, {NumberToString(win::HandleToUint32(handle)),
                                (launch_options->elevated ? ",p," : ",i,")});
#elif BUILDFLAG(IS_APPLE)
  // In the receiving child, the handle is looked up using the rendezvous key.
  launch_options->mach_ports_for_rendezvous.emplace(
      HistogramSharedMemory::kRendezvousKey,
      MachRendezvousPort(std::move(shmem_handle)));
  base::StrAppend(
      &serialized,
      {base::NumberToString(HistogramSharedMemory::kRendezvousKey), ",r,"});
#elif BUILDFLAG(IS_FUCHSIA)
  // The handle is passed via the handles to transfer launch options. The child
  // will use the returned handle_id to lookup the handle. Ownership of the
  // handle is transferred to |launch_options|.
  uint32_t handle_id = LaunchOptions::AddHandleToTransfer(
      &launch_options->handles_to_transfer, shmem_handle.release());
  base::StrAppend(&serialized, {base::NumberToString(handle_id), ",i,"});
#elif BUILDFLAG(IS_POSIX)
  // Serialize the key by which the child can lookup the shared memory handle.
  // Ownership of the handle is transferred, via |descriptor_to_share|, to the
  // caller, who is responsible for updating |launch_options| or the zygote
  // launch parameters, as appropriate.
  //
  // TODO(crbug.com/1028263): Create a wrapper to release and return the primary
  // descriptor for android (ScopedFD) vs non-android (ScopedFDPair).
  //
  // TODO(crbug.com/1028263): Get rid of |descriptor_to_share| and just populate
  // |launch_options|. The caller should be responsible for translating between
  // |launch_options| and zygote parameters as necessary.
#if BUILDFLAG(IS_ANDROID)
  descriptor_to_share = std::move(shmem_handle);
#else
  descriptor_to_share = std::move(shmem_handle.fd);
#endif
  DVLOG(1) << "Sharing fd=" << descriptor_to_share.get()
           << " with child process as fd_key=" << descriptor_key;
  base::StrAppend(&serialized, {base::NumberToString(descriptor_key), ",i,"});
#else
#error "Unsupported OS"
#endif

  base::StrAppend(
      &serialized,
      {base::NumberToString(shmem_token.GetHighForSerialization()), ",",
       base::NumberToString(shmem_token.GetLowForSerialization()), ",",
       base::NumberToString(shmem_size)});

  DCHECK_LT(serialized.size(), kSerializedReservedSize);
  return serialized;
}

// Deserialize |guid| from |hi_part| and |lo_part|, returning true on success.
absl::optional<UnguessableToken> DeserializeGUID(std::string_view hi_part,
                                                 std::string_view lo_part) {
  uint64_t hi = 0;
  uint64_t lo = 0;
  if (!StringToUint64(hi_part, &hi) || !StringToUint64(lo_part, &lo)) {
    return absl::nullopt;
  }
  return UnguessableToken::Deserialize(hi, lo);
}

// Deserialize |switch_value| and return a corresponding writable shared memory
// region. On POSIX the handle is passed by |histogram_memory_descriptor_key|
// but |switch_value| is still required to describe the memory region.
UnsafeSharedMemoryRegion DeserializeSharedMemoryRegionMetadata(
    const std::string& switch_value) {
  std::vector<std::string_view> tokens =
      SplitStringPiece(switch_value, ",", KEEP_WHITESPACE, SPLIT_WANT_ALL);
  if (tokens.size() != 5) {
    return UnsafeSharedMemoryRegion();
  }

  // Parse the handle from tokens[0].
  uint64_t shmem_handle = 0;
  if (!StringToUint64(tokens[0], &shmem_handle)) {
    return UnsafeSharedMemoryRegion();
  }

  // token[1] has a fixed value but is ignored on all platforms except
  // Windows, where it can be 'i' or 'p' to indicate that the handle is
  // inherited or must be obtained from the parent.
#if BUILDFLAG(IS_WIN)
  HANDLE handle = win::Uint32ToHandle(checked_cast<uint32_t>(shmem_handle));
  if (tokens[1] == "p") {
    DCHECK(IsCurrentProcessElevated());
    // LaunchProcess doesn't have a way to duplicate the handle, but this
    // process can since by definition it's not sandboxed.
    win::ScopedHandle parent_handle(OpenProcess(
        PROCESS_ALL_ACCESS, FALSE, GetParentProcessId(GetCurrentProcess())));
    DuplicateHandle(parent_handle.get(), handle, GetCurrentProcess(), &handle,
                    0, FALSE, DUPLICATE_SAME_ACCESS);
  } else if (tokens[1] != "i") {
    return UnsafeSharedMemoryRegion();
  }
  win::ScopedHandle scoped_handle(handle);
#elif BUILDFLAG(IS_APPLE)
  DCHECK_EQ(tokens[1], "r");
  auto* rendezvous = MachPortRendezvousClient::GetInstance();
  if (!rendezvous) {
    LOG(ERROR) << "No rendezvous client.";
    return UnsafeSharedMemoryRegion();
  }
  apple::ScopedMachSendRight scoped_handle = rendezvous->TakeSendRight(
      static_cast<MachPortsForRendezvous::key_type>(shmem_handle));
  if (!scoped_handle.is_valid()) {
    LOG(ERROR) << "Failed to initialize mach send right.";
    return UnsafeSharedMemoryRegion();
  }
#elif BUILDFLAG(IS_FUCHSIA)
  DCHECK_EQ(tokens[1], "i");
  static bool startup_handle_taken = false;
  DCHECK(!startup_handle_taken) << "Shared memory region initialized twice";
  const uint32_t handle = checked_cast<uint32_t>(shmem_handle);
  zx::vmo scoped_handle(zx_take_startup_handle(handle));
  startup_handle_taken = true;
  if (!scoped_handle.is_valid()) {
    return UnsafeSharedMemoryRegion();
  }
#elif BUILDFLAG(IS_POSIX)
  DCHECK_EQ(tokens[1], "i");
  const int fd = GlobalDescriptors::GetInstance()->MaybeGet(
      checked_cast<GlobalDescriptors::Key>(shmem_handle));
  if (fd == -1) {
    DVLOG(1) << "Failed global descriptor lookup: " << shmem_handle;
    return UnsafeSharedMemoryRegion();
  }
  DVLOG(1) << "Opening shared memory handle " << fd << " shared as "
           << shmem_handle;
  ScopedFD scoped_handle(fd);
#else
#error Unsupported OS
#endif

  // Together, tokens[2] and tokens[3] encode the shared memory guid.
  auto guid = DeserializeGUID(tokens[2], tokens[3]);
  if (!guid.has_value()) {
    return UnsafeSharedMemoryRegion();
  }

  // The size is in tokens[4].
  uint64_t size;
  if (!StringToUint64(tokens[4], &size)) {
    return UnsafeSharedMemoryRegion();
  }

  // Resolve the handle to a shared memory region.
  auto platform_handle = subtle::PlatformSharedMemoryRegion::Take(
      std::move(scoped_handle),
      subtle::PlatformSharedMemoryRegion::Mode::kUnsafe,
      checked_cast<size_t>(size), guid.value());
  return UnsafeSharedMemoryRegion::Deserialize(std::move(platform_handle));
}

}  // namespace

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
absl::optional<HistogramSharedMemory::SharedMemory>
HistogramSharedMemory::Create(int process_id,
                              const HistogramSharedMemory::Config& config) {
  auto region = UnsafeSharedMemoryRegion::Create(config.memory_size_bytes);
  if (!region.IsValid()) {
    DVLOG(1) << "Failed to create shared memory region.";
    return absl::nullopt;
  }
  auto mapping = region.Map();
  if (!mapping.IsValid()) {
    DVLOG(1) << "Failed to create shared memory mapping.";
    return absl::nullopt;
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
  // TODO(crbug.com/1028263): Fix ChromeOS and utility processes.
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

  std::string encoded_switch_value =
      SerializeSharedMemoryRegionMetadata(std::move(histogram_shmem_region),
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
                                          descriptor_key, descriptor_to_share,
#endif
                                          launch_options);
  command_line->AppendSwitchASCII(::switches::kMetricsSharedMemoryHandle,
                                  encoded_switch_value);
}

// static
void HistogramSharedMemory::InitFromLaunchParameters(
    const CommandLine& command_line) {
  // TODO(crbug.com/1028263): Clean up once fully launched.
  if (!command_line.HasSwitch(switches::kMetricsSharedMemoryHandle)) {
    return;
  }
  CHECK(!GlobalHistogramAllocator::Get());
  DVLOG(1) << "Initializing histogram shared memory from command line for "
           << command_line.GetSwitchValueASCII("type");

  auto shmem_region = DeserializeSharedMemoryRegionMetadata(
      command_line.GetSwitchValueASCII(switches::kMetricsSharedMemoryHandle));
  CHECK(shmem_region.IsValid())
      << "Invald memory region passed on command line.";

  GlobalHistogramAllocator::CreateWithSharedMemoryRegion(shmem_region);

  auto* global_allocator = GlobalHistogramAllocator::Get();
  CHECK(global_allocator);
  global_allocator->CreateTrackingHistograms(global_allocator->Name());
}

}  // namespace base

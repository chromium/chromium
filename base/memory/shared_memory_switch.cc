// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory_switch.h"

#include <optional>
#include <string_view>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/process/process_info.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/types/expected.h"
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

namespace base {
namespace shared_memory {
namespace {

using subtle::PlatformSharedMemoryRegion;
using subtle::ScopedPlatformSharedMemoryHandle;

// The max shared memory size is artificially limited. This serves as a sanity
// check when serializing/deserializing the handle info. This value should be
// slightly larger than the largest shared memory size used in practice.
constexpr size_t kMaxSharedMemorySize = 8 << 20;  // 8 MiB

// Return a scoped platform shared memory handle for |shmem_region|, possibly
// with permissions reduced to make the handle read-only.
ScopedPlatformSharedMemoryHandle GetPlatformHandle(
    PlatformSharedMemoryRegion& shmem_region,
    [[maybe_unused]] bool make_read_only) {
#if BUILDFLAG(IS_FUCHSIA)
  if (make_read_only) {
    // For Fuchsia, ScopedPlatformSharedMemoryHandle <==> zx::vmo
    zx::vmo scoped_handle;
    zx_status_t status = shmem_region.GetPlatformHandle()->duplicate(
        ZX_RIGHT_READ | ZX_RIGHT_MAP | ZX_RIGHT_TRANSFER |
            ZX_RIGHT_GET_PROPERTY | ZX_RIGHT_DUPLICATE,
        &scoped_handle);
    ZX_CHECK(status == ZX_OK, status) << "zx_handle_duplicate";
    return scoped_handle;
  }
#endif  // BUILDFLAG(IS_FUCHSIA)
  return shmem_region.PassPlatformHandle();
}

// Serializes the shared memory region metadata to a string that can be added
// to the command-line of a child-process.
std::string Serialize(PlatformSharedMemoryRegion shmem_region,
                      bool is_read_only,
#if BUILDFLAG(IS_APPLE)
                      MachPortsForRendezvous::key_type rendezvous_key,
#elif BUILDFLAG(IS_POSIX)
                      GlobalDescriptors::Key descriptor_key,
                      ScopedFD& descriptor_to_share,
#endif
                      [[maybe_unused]] LaunchOptions* launch_options) {
#if !BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_APPLE)
  CHECK(launch_options != nullptr);
#endif

  CHECK(shmem_region.IsValid());

  auto shmem_token = shmem_region.GetGUID();
  auto shmem_size = shmem_region.GetSize();
  auto shmem_handle = GetPlatformHandle(shmem_region, is_read_only);

  CHECK(shmem_token);
  CHECK_NE(shmem_size, 0u);
  CHECK_LE(shmem_size, kMaxSharedMemorySize);

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
  StrAppend(&serialized, {NumberToString(win::HandleToUint32(handle)),
                          (launch_options->elevated ? ",p," : ",i,")});
#elif BUILDFLAG(IS_APPLE)
  // In the receiving child, the handle is looked up using the rendezvous key.
  launch_options->mach_ports_for_rendezvous.emplace(
      rendezvous_key, MachRendezvousPort(std::move(shmem_handle)));
  StrAppend(&serialized, {NumberToString(rendezvous_key), ",r,"});
#elif BUILDFLAG(IS_FUCHSIA)
  // The handle is passed via the handles to transfer launch options. The child
  // will use the returned handle_id to lookup the handle. Ownership of the
  // handle is transferred to |launch_options|.
  uint32_t handle_id = LaunchOptions::AddHandleToTransfer(
      &launch_options->handles_to_transfer, shmem_handle.release());
  StrAppend(&serialized, {NumberToString(handle_id), ",i,"});
#elif BUILDFLAG(IS_POSIX)
  // Serialize the key by which the child can lookup the shared memory handle.
  // Ownership of the handle is transferred, via |descriptor_to_share|, to the
  // caller, who is responsible for updating |launch_options| or the zygote
  // launch parameters, as appropriate.
  //
  // TODO(crbug.com/40109064): Create a wrapper to release and return the
  // primary descriptor for android (ScopedFD) vs non-android (ScopedFDPair).
  //
  // TODO(crbug.com/40109064): Get rid of |descriptor_to_share| and just
  // populate |launch_options|. The caller should be responsible for translating
  // between |launch_options| and zygote parameters as necessary.
#if BUILDFLAG(IS_ANDROID)
  descriptor_to_share = std::move(shmem_handle);
#else
  descriptor_to_share = std::move(shmem_handle.fd);
#endif
  DVLOG(1) << "Sharing fd=" << descriptor_to_share.get()
           << " with child process as fd_key=" << descriptor_key;
  StrAppend(&serialized, {NumberToString(descriptor_key), ",i,"});
#else
#error "Unsupported OS"
#endif

  StrAppend(&serialized,
            {NumberToString(shmem_token.GetHighForSerialization()), ",",
             NumberToString(shmem_token.GetLowForSerialization()), ",",
             NumberToString(shmem_size)});

  DCHECK_LT(serialized.size(), kSerializedReservedSize);
  return serialized;
}

// Deserialize |guid| from |hi_part| and |lo_part|, returning true on success.
std::optional<UnguessableToken> DeserializeGUID(std::string_view hi_part,
                                                std::string_view lo_part) {
  uint64_t hi = 0;
  uint64_t lo = 0;
  if (!StringToUint64(hi_part, &hi) || !StringToUint64(lo_part, &lo)) {
    return std::nullopt;
  }
  return UnguessableToken::Deserialize(hi, lo);
}

// Deserialize |switch_value| and return a corresponding writable shared memory
// region. On POSIX the handle is passed by |histogram_memory_descriptor_key|
// but |switch_value| is still required to describe the memory region.
expected<PlatformSharedMemoryRegion, SharedMemoryError> Deserialize(
    std::string_view switch_value,
    PlatformSharedMemoryRegion::Mode mode) {
  std::vector<std::string_view> tokens =
      SplitStringPiece(switch_value, ",", KEEP_WHITESPACE, SPLIT_WANT_ALL);
  if (tokens.size() != 5) {
    return unexpected(SharedMemoryError::kUnexpectedTokensCount);
  }

  // Parse the handle from tokens[0].
  uint64_t shmem_handle = 0;
  if (!StringToUint64(tokens[0], &shmem_handle)) {
    return unexpected(SharedMemoryError::kParseInt0Failed);
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
    return unexpected(SharedMemoryError::kUnexpectedHandleType);
  }
  win::ScopedHandle scoped_handle(handle);
#elif BUILDFLAG(IS_APPLE)
  DCHECK_EQ(tokens[1], "r");
  auto* rendezvous = MachPortRendezvousClient::GetInstance();
  if (!rendezvous) {
    // Note: This matches mojo behavior in content/child/child_thread_impl.cc.
    LOG(ERROR) << "No rendezvous client, terminating process (parent died?)";
    Process::TerminateCurrentProcessImmediately(0);
  }
  apple::ScopedMachSendRight scoped_handle = rendezvous->TakeSendRight(
      static_cast<MachPortsForRendezvous::key_type>(shmem_handle));
  if (!scoped_handle.is_valid()) {
    // Note: This matches mojo behavior in content/child/child_thread_impl.cc.
    LOG(ERROR) << "Mach rendezvous failed, terminating process (parent died?)";
    base::Process::TerminateCurrentProcessImmediately(0);
  }
#elif BUILDFLAG(IS_FUCHSIA)
  DCHECK_EQ(tokens[1], "i");
  const uint32_t handle = checked_cast<uint32_t>(shmem_handle);
  zx::vmo scoped_handle(zx_take_startup_handle(handle));
  if (!scoped_handle.is_valid()) {
    LOG(ERROR) << "Invalid shared mem handle: " << handle;
    return unexpected(SharedMemoryError::kInvalidHandle);
  }
#elif BUILDFLAG(IS_POSIX)
  DCHECK_EQ(tokens[1], "i");
  const int fd = GlobalDescriptors::GetInstance()->MaybeGet(
      checked_cast<GlobalDescriptors::Key>(shmem_handle));
  if (fd == -1) {
    LOG(ERROR) << "Failed global descriptor lookup: " << shmem_handle;
    return unexpected(SharedMemoryError::kGetFDFailed);
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
    return unexpected(SharedMemoryError::kDeserializeGUIDFailed);
  }

  // The size is in tokens[4].
  uint64_t size = 0;
  if (!StringToUint64(tokens[4], &size)) {
    return unexpected(SharedMemoryError::kParseInt4Failed);
  }
  if (size == 0 || size > kMaxSharedMemorySize) {
    return unexpected(SharedMemoryError::kUnexpectedSize);
  }

  // Resolve the handle to a shared memory region.
  return PlatformSharedMemoryRegion::Take(
      std::move(scoped_handle), mode, checked_cast<size_t>(size), guid.value());
}

}  // namespace

void AddToLaunchParameters(std::string_view switch_name,
                           ReadOnlySharedMemoryRegion read_only_memory_region,
#if BUILDFLAG(IS_APPLE)
                           MachPortsForRendezvous::key_type rendezvous_key,
#elif BUILDFLAG(IS_POSIX)
                           GlobalDescriptors::Key descriptor_key,
                           ScopedFD& out_descriptor_to_share,
#endif
                           CommandLine* command_line,
                           LaunchOptions* launch_options) {
  std::string switch_value =
      Serialize(ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
                    std::move(read_only_memory_region)),
                /*is_read_only=*/true,
#if BUILDFLAG(IS_APPLE)
                rendezvous_key,
#elif BUILDFLAG(IS_POSIX)
                descriptor_key, out_descriptor_to_share,
#endif
                launch_options);
  command_line->AppendSwitchASCII(switch_name, switch_value);
}

void AddToLaunchParameters(std::string_view switch_name,
                           UnsafeSharedMemoryRegion unsafe_memory_region,
#if BUILDFLAG(IS_APPLE)
                           MachPortsForRendezvous::key_type rendezvous_key,
#elif BUILDFLAG(IS_POSIX)
                           GlobalDescriptors::Key descriptor_key,
                           ScopedFD& out_descriptor_to_share,
#endif
                           CommandLine* command_line,
                           LaunchOptions* launch_options) {
  std::string switch_value =
      Serialize(UnsafeSharedMemoryRegion::TakeHandleForSerialization(
                    std::move(unsafe_memory_region)),
                /*is_read_only=*/false,
#if BUILDFLAG(IS_APPLE)
                rendezvous_key,
#elif BUILDFLAG(IS_POSIX)
                descriptor_key, out_descriptor_to_share,
#endif
                launch_options);
  command_line->AppendSwitchASCII(switch_name, switch_value);
}

expected<UnsafeSharedMemoryRegion, SharedMemoryError>
UnsafeSharedMemoryRegionFrom(std::string_view switch_value) {
  auto platform_handle =
      Deserialize(switch_value, PlatformSharedMemoryRegion::Mode::kUnsafe);
  if (!platform_handle.has_value()) {
    return unexpected(platform_handle.error());
  }
  auto shmem_region =
      UnsafeSharedMemoryRegion::Deserialize(std::move(platform_handle).value());
  if (!shmem_region.IsValid()) {
    LOG(ERROR) << "Failed to deserialize writable memory handle";
    return unexpected(SharedMemoryError::kDeserializeFailed);
  }
  return ok(std::move(shmem_region));
}

expected<ReadOnlySharedMemoryRegion, SharedMemoryError>
ReadOnlySharedMemoryRegionFrom(std::string_view switch_value) {
  auto platform_handle =
      Deserialize(switch_value, PlatformSharedMemoryRegion::Mode::kReadOnly);
  if (!platform_handle.has_value()) {
    return unexpected(platform_handle.error());
  }
  auto shmem_region = ReadOnlySharedMemoryRegion::Deserialize(
      std::move(platform_handle).value());
  if (!shmem_region.IsValid()) {
    LOG(ERROR) << "Faield to deserialize read-only memory handle";
    return unexpected(SharedMemoryError::kDeserializeFailed);
  }
  return ok(std::move(shmem_region));
}

}  // namespace shared_memory
}  // namespace base

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_SHARED_MEMORY_SWITCH_H_
#define BASE_MEMORY_SHARED_MEMORY_SWITCH_H_

#include <string_view>

#include "base/base_export.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/stack_allocated.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/types/expected.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"

#if !BUILDFLAG(USE_BLINK)
#error "This is only intended for platforms that use blink."
#endif

#if BUILDFLAG(IS_APPLE)
#include "base/apple/mach_port_rendezvous.h"
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
#include "base/files/platform_file.h"
#include "base/posix/global_descriptors.h"
#endif

namespace base {

class CommandLine;
struct LaunchOptions;

namespace shared_memory {

// Indicates failure modes of deserializing a shared memory switch value.
enum class SharedMemoryError {
  kNoError,
  kUnexpectedTokensCount,
  kParseInt0Failed,
  kParseInt4Failed,
  kUnexpectedHandleType,
  kInvalidHandle,
  kGetFDFailed,
  kDeserializeGUIDFailed,
  kDeserializeFailed,
  kCreateTrialsFailed,
  kUnexpectedSize,
};

// Platform-specific options to share a shared memory region with a child
// process. On Apple platforms, this uses a mach port rendezvous key. On other
// POSIX platforms, this uses a file descriptor key.
struct BASE_EXPORT SharedMemorySwitch {
  // This class is intended to just live on the stack for passing parameters,
  // as it merely stores a std::string_view reference to the switch name.
  STACK_ALLOCATED();

 public:
#if BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_IOS_TVOS)
  // The rendezvous key used to share the mach port.
  using RendezvousKey = MachPortsForRendezvous::key_type;
#else
  // On tvOS and non-Apple platforms, the rendezvous type is unused but defined
  // to allow for a consistent function interface.
  using RendezvousKey = uint32_t;
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
  // The key used to identify the file descriptor in the child process.
  using DescriptorKey = GlobalDescriptors::Key;
#else
  // On other platforms, the descriptor key is unused but defined to allow for a
  // consistent function interface.
  using DescriptorKey = uint32_t;
#endif

  // Initializes the shared memory switch with the given switch name, rendezvous
  // key, and descriptor key. The rendezvous key and descriptor key are unused
  // on platforms that do not use them but are required arguments to allow for a
  // consistent function interface.
  SharedMemorySwitch(std::string_view switch_name_in,
                     [[maybe_unused]] RendezvousKey rendezvous_key_in,
                     [[maybe_unused]] DescriptorKey descriptor_key_in);
  ~SharedMemorySwitch();

  SharedMemorySwitch(SharedMemorySwitch&&);
  SharedMemorySwitch& operator=(SharedMemorySwitch&&);

  // Updates `command_line` and `launch_options` to use `switch_name` to pass
  // `read_only_memory_region` to child process that is about to be launched.
  // This should be called in the parent process as a part of setting up the
  // launch conditions of the child. This call will update the `command_line`
  // and `launch_options`. On posix, where we prefer to use a zygote instead of
  // using the launch_options to launch a new process, the platform
  // `out_descriptor_to_share` is populated. The caller is expected to transmit
  // the descriptor to the launch flow for the zygote.
  void AddToLaunchParameters(
      const ReadOnlySharedMemoryRegion& read_only_memory_region,
      CommandLine* command_line,
      LaunchOptions* launch_options);

  // Updates `command_line` and `launch_options` to use `switch_name` to pass
  // `unsafe_memory_region` to a child process that is about to be launched.
  // This should be called in the parent process as a part of setting up the
  // launch conditions of the child. This call will update the `command_line`
  // and `launch_options`.
  void AddToLaunchParameters(
      const UnsafeSharedMemoryRegion& unsafe_memory_region,
      CommandLine* command_line,
      LaunchOptions* launch_options);

  // The name of the switch to use to pass the shared memory region to the
  // child process.
  std::string_view switch_name;

#if BUILDFLAG(IS_APPLE)
  RendezvousKey rendezvous_key;
#endif
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
  // The key used to identify the file descriptor in the child process.
  DescriptorKey descriptor_key;
  // The descriptor to share, as an out-parameter. This is populated by
  // AddToLaunchParameters() and expected to be transferred to the launch flow
  // for the zygote.
  ScopedFD out_descriptor_to_share;
#endif
};

// Returns an UnsafeSharedMemoryRegion deserialized from `switch_value`.
BASE_EXPORT expected<UnsafeSharedMemoryRegion, SharedMemoryError>
UnsafeSharedMemoryRegionFrom(std::string_view switch_value);

// Returns an ReadOnlySharedMemoryRegion deserialized from `switch_value`.
BASE_EXPORT expected<ReadOnlySharedMemoryRegion, SharedMemoryError>
ReadOnlySharedMemoryRegionFrom(std::string_view switch_value);

}  // namespace shared_memory
}  // namespace base

#endif  // BASE_MEMORY_SHARED_MEMORY_SWITCH_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_USERSPACE_SWAP_CHROMEOS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_USERSPACE_SWAP_CHROMEOS_H_

#include "base/byte_count.h"
#include "chromeos/ash/components/memory/userspace_swap/userspace_swap.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace performance_manager {

class ProcessNode;

namespace mechanism::userspace_swap {

// NOTE: All of the following methods MUST be called from the PM sequence.

// The policy must query the mechanism IsEligibleToSwap because the mechanism
// knows things that the policy doesn't related to the swap file and
// userfaultfd, this method will return false if an unexpected event that
// prevents swapping occurred.
bool IsEligibleToSwap(const ProcessNode* process_node);

// Swap a |process_node|.
void SwapProcessNode(const ProcessNode* process_node);

// Returns the free space available on the device which is backing the swap
// files.
base::ByteCount GetSwapDeviceFreeSpace();

// Returns the amount currently in use by the swap file for |process_node|.
base::ByteCount GetProcessNodeSwapFileUsage(const ProcessNode* process_node);

// Returns the size that this process node has had reclaimed. Reclaim refers to
// physical memory which were swapped.
base::ByteCount GetProcessNodeReclaimedSpace(const ProcessNode* process_node);

// Returns the amount currently in use across all swap files.
base::ByteCount GetTotalSwapFileUsage();

// Returns the amount which have been reclaimed.
base::ByteCount GetTotalReclaimedSpace();

class UserspaceSwapInitializationImpl
    : public ::userspace_swap::mojom::UserspaceSwapInitialization {
 public:
  using MemoryRegionPtr = ::userspace_swap::mojom::MemoryRegionPtr;

  explicit UserspaceSwapInitializationImpl(int render_process_host_id);

  UserspaceSwapInitializationImpl(const UserspaceSwapInitializationImpl&) =
      delete;
  UserspaceSwapInitializationImpl& operator=(
      const UserspaceSwapInitializationImpl&) = delete;

  ~UserspaceSwapInitializationImpl() override;

  static bool UserspaceSwapSupportedAndEnabled();
  static void Create(
      int render_process_host_id,
      mojo::PendingReceiver<
          ::userspace_swap::mojom::UserspaceSwapInitialization> receiver);

  // UserspaceSwapInitialization impl:
  void TransferUserfaultFD(uint64_t uffd_error,
                           mojo::PlatformHandle uffd_handle,
                           uint64_t mmap_error,
                           MemoryRegionPtr swap_area,
                           TransferUserfaultFDCallback cb) override;

 private:
  int render_process_host_id_ = 0;
  bool received_transfer_cb_ = false;
};

}  // namespace mechanism::userspace_swap

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_USERSPACE_SWAP_CHROMEOS_H_

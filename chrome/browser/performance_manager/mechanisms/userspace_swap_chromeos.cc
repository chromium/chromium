// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/userspace_swap_chromeos.h"

#include <memory>

#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/page_size.h"
#include "base/posix/safe_strerror.h"
#include "base/threading/scoped_blocking_call.h"
#include "chromeos/ash/components/memory/userspace_swap/region.h"
#include "chromeos/ash/components/memory/userspace_swap/swap_storage.h"
#include "chromeos/ash/components/memory/userspace_swap/userfaultfd.h"
#include "chromeos/ash/components/memory/userspace_swap/userspace_swap.h"
#include "chromeos/ash/components/memory/userspace_swap/userspace_swap.mojom.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace performance_manager {
namespace mechanism {
namespace userspace_swap {

namespace {

using ::ash::memory::userspace_swap::Region;
using ::ash::memory::userspace_swap::RendererSwapData;
using ::ash::memory::userspace_swap::SwapFile;
using ::ash::memory::userspace_swap::UserfaultFD;
using ::ash::memory::userspace_swap::UserspaceSwapConfig;

// We cache the swap device free space so we don't hammer the FS layer with
// unnecessary syscalls. The initial value of 30s was chosen because it seemed
// like a safe value that would prevent hitting the disk too frequently while
// preventing space from getting too low in times of heavy swap. Feel free to
// change it if you find a better value.
constexpr base::TimeDelta kSwapDeviceAvailableSpaceCheckInterval =
    base::Seconds(30);
base::TimeTicks g_last_swap_device_free_space_check;
uint64_t g_swap_device_free_swap_bytes;

// We must bind our mojo remote on the UI thread, this callback does that.
void BindUserspaceSwapReceiverOnUIThread(
    RenderProcessHostProxy proxy,
    mojo::PendingReceiver<::userspace_swap::mojom::UserspaceSwap> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderProcessHost* render_process_host = proxy.Get();
  if (!render_process_host) {
    return;
  }

  render_process_host->BindReceiver(std::move(receiver));
}

// UserspaceSwapMechanismData contains process node specific details and
// handles.
class UserspaceSwapMechanismData
    : public ExternalNodeAttachedDataImpl<UserspaceSwapMechanismData> {
 public:
  explicit UserspaceSwapMechanismData(const ProcessNode* node) {}
  ~UserspaceSwapMechanismData() override = default;

  std::unique_ptr<RendererSwapData> swap_data;
};

void InitializeProcessNodeOnGraph(int render_process_host_id,
                                  base::ScopedFD uffd,
                                  Region swap_area,
                                  performance_manager::Graph* graph) {
  // Now look up the ProcessNode so we can complete initialization.
  DCHECK(graph);
  DCHECK(uffd.is_valid());

  const ProcessNode* process_node = nullptr;
  for (const ProcessNode* proc : graph->GetAllProcessNodes()) {
    if (proc->GetRenderProcessHostId().GetUnsafeValue() ==
        render_process_host_id) {
      process_node = proc;
      break;
    }
  }

  if (!process_node) {
    LOG(ERROR) << "Couldn't find process node for rphid: "
               << render_process_host_id;
    return;
  }

  if (UserspaceSwapMechanismData::Destroy(process_node)) {
    LOG(ERROR) << "ProcessNode contained UserspaceSwapMechanismData";
    return;
  }

  auto* data = UserspaceSwapMechanismData::GetOrCreate(process_node);

  // If all other setup has completed successfully, we can tell the renderer to
  // construct an implementation of userspace_swap::mojom::UserspaceSwap.
  // The RenderProcessHostProxy is a WeakPtr that should only be accessed on the
  // UI thread.
  mojo::PendingRemote<::userspace_swap::mojom::UserspaceSwap> remote;
  const RenderProcessHostProxy& proxy =
      process_node->GetRenderProcessHostProxy();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BindUserspaceSwapReceiverOnUIThread, proxy,
                                remote.InitWithNewPipeAndPassReceiver()));

  // Wrap up the received userfaultfd into a UserfaultFD instance.
  std::unique_ptr<UserfaultFD> userfaultfd =
      UserfaultFD::WrapFD(std::move(uffd));

  // The SwapFile is always encrypted but the compression layer is optional.
  SwapFile::Type swap_type = SwapFile::Type::kEncrypted;
  if (UserspaceSwapConfig::Get().use_compressed_swap_file) {
    swap_type =
        static_cast<SwapFile::Type>(swap_type | SwapFile::Type::kCompressed);
  }

  std::unique_ptr<SwapFile> swap_file = SwapFile::Create(swap_type);

  if (!swap_file) {
    PLOG(ERROR) << "Unable to complete userspace swap initialization failure "
                   "creating swap file";

    // If we can't create a swap file, then we will bail freeing our resources.
    UserspaceSwapMechanismData::Destroy(process_node);

    return;
  }

  data->swap_data = RendererSwapData::Create(
      render_process_host_id, process_node->GetProcessId(),
      std::move(userfaultfd), std::move(swap_file), swap_area,
      std::move(remote));
}

}  // namespace

bool IsEligibleToSwap(const ProcessNode* process_node) {
  if (!process_node->GetProcess().IsValid()) {
    return false;
  }

  auto* data = UserspaceSwapMechanismData::Get(process_node);
  if (!data || !data->swap_data) {
    return false;
  }

  // Now let the implementation decide if swap should actually be allowed.
  return data->swap_data->SwapAllowed();
}

uint64_t GetSwapDeviceFreeSpaceBytes() {
  auto now_ticks = base::TimeTicks::Now();
  if (now_ticks - g_last_swap_device_free_space_check >
      kSwapDeviceAvailableSpaceCheckInterval) {
    g_last_swap_device_free_space_check = now_ticks;
    g_swap_device_free_swap_bytes = SwapFile::GetBackingStoreFreeSpaceKB()
                                    << 10;  // convert to bytes.
  }

  return g_swap_device_free_swap_bytes;
}

uint64_t GetProcessNodeSwapFileUsageBytes(const ProcessNode* process_node) {
  auto* data = UserspaceSwapMechanismData::Get(process_node);
  if (!data || !data->swap_data) {
    return 0;
  }

  return data->swap_data->SwapDiskspaceUsedBytes();
}

uint64_t GetProcessNodeReclaimedBytes(const ProcessNode* process_node) {
  auto* data = UserspaceSwapMechanismData::Get(process_node);
  if (!data || !data->swap_data) {
    return 0;
  }

  return data->swap_data->ReclaimedBytes();
}

uint64_t GetTotalSwapFileUsageBytes() {
  return ash::memory::userspace_swap::GetGlobalSwapDiskspaceUsed();
}

uint64_t GetTotalReclaimedBytes() {
  return ash::memory::userspace_swap::GetGlobalMemoryReclaimed();
}

void SwapProcessNode(const ProcessNode* process_node) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  auto* data = UserspaceSwapMechanismData::Get(process_node);
  if (!data || !data->swap_data) {
    return;
  }

  auto& swap_data = data->swap_data;

  // SwapProcessNode always starts by determining exactly how many regions we
  // can swap based on current swapfile usage for this renderer and globally.
  static const size_t kPageSize = base::GetPageSize();
  static const uint64_t kPagesPerRegion =
      UserspaceSwapConfig::Get().number_of_pages_per_region;
  static const uint64_t kRegionSize = kPagesPerRegion * kPageSize;

  const auto& config = UserspaceSwapConfig::Get();

  uint64_t swap_file_disk_space_used_bytes =
      swap_data->SwapDiskspaceUsedBytes();

  // This renderer can only swap up to what's available in the global swap file
  // limit or what's available in it's own swap file limit.
  int64_t available_swap_bytes =
      std::min(config.maximum_swap_disk_space_bytes -
                   ash::memory::userspace_swap::GetGlobalSwapDiskspaceUsed(),
               config.renderer_maximum_disk_swap_file_size_bytes -
                   swap_file_disk_space_used_bytes);

  // We have a configurable limit to the number of regions we will consider per
  // iteration and adjust based on how much disk space is actually
  // available for us which was calculated before.
  // Finally, we know how many regions this renderer is able to swap.
  int64_t available_swap_regions = available_swap_bytes / kRegionSize;
  int64_t total_regions_swapable =
      std::min(static_cast<int64_t>(config.renderer_region_limit_per_swap),
               available_swap_regions);

  if (total_regions_swapable <= 0) {
    // We don't have enough space available to swap a single region.
    return;
  }

  // Now we know how many regions this renderer can theoretically swap after
  // enforcing all configurable limits.
  ash::memory::userspace_swap::SwapRenderer(
      swap_data.get(), total_regions_swapable * kRegionSize);
}

UserspaceSwapInitializationImpl::UserspaceSwapInitializationImpl(
    int render_process_host_id)
    : render_process_host_id_(render_process_host_id) {
  CHECK(UserspaceSwapInitializationImpl::UserspaceSwapSupportedAndEnabled());
}

UserspaceSwapInitializationImpl::~UserspaceSwapInitializationImpl() = default;

// static
bool UserspaceSwapInitializationImpl::UserspaceSwapSupportedAndEnabled() {
  return ash::memory::userspace_swap::UserspaceSwapSupportedAndEnabled();
}

// static
void UserspaceSwapInitializationImpl::Create(
    int render_process_host_id,
    mojo::PendingReceiver<::userspace_swap::mojom::UserspaceSwapInitialization>
        receiver) {
  auto impl =
      std::make_unique<UserspaceSwapInitializationImpl>(render_process_host_id);
  mojo::MakeSelfOwnedReceiver(std::move(impl), std::move(receiver));
}

void UserspaceSwapInitializationImpl::TransferUserfaultFD(
    uint64_t uffd_error,
    mojo::PlatformHandle uffd_handle,
    uint64_t mmap_error,
    MemoryRegionPtr swap_area,
    TransferUserfaultFDCallback cb) {
  base::ScopedClosureRunner scr(std::move(cb));

  if (received_transfer_cb_) {
    return;
  }

  received_transfer_cb_ = true;
  if (uffd_error != 0) {
    LOG(ERROR) << "Unable to create userfaultfd for renderer: "
               << base::safe_strerror(uffd_error);
    return;
  }

  if (mmap_error != 0) {
    LOG(ERROR) << "Unable to create memory area for renderer: "
               << base::safe_strerror(mmap_error);
    return;
  }

  if (!uffd_handle.is_valid()) {
    LOG(ERROR) << "FD received is invalid.";
    return;
  }

  // Make sure we're on the graph and complete the initialization.
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindOnce(&InitializeProcessNodeOnGraph,
                                render_process_host_id_, uffd_handle.TakeFD(),
                                Region(swap_area->address, swap_area->length)));
}

}  // namespace userspace_swap
}  // namespace mechanism
}  // namespace performance_manager

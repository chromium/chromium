// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/userspace_swap_chromeos.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/scoped_file.h"
#include "base/posix/safe_strerror.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "chromeos/memory/userspace_swap/swap_storage.h"
#include "chromeos/memory/userspace_swap/userfaultfd.h"
#include "chromeos/memory/userspace_swap/userspace_swap.h"
#include "chromeos/memory/userspace_swap/userspace_swap.mojom.h"
#include "components/performance_manager/graph/node_attached_data.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/process_node_source.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/performance_manager/render_process_user_data.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace performance_manager {
namespace mechanism {
namespace userspace_swap {

namespace {
using chromeos::memory::userspace_swap::SwapFile;
using chromeos::memory::userspace_swap::UserfaultFD;
using chromeos::memory::userspace_swap::UserspaceSwapConfig;

// The RendererSwapData structure contains all the state related to userspace
// swap for an individual renderer.
//
// TODO(bgeffon): This moves to a shared file later when the remainder of the
// code lands.
struct RendererSwapData {
  int render_process_host_id;
  bool setup_complete = false;

  std::unique_ptr<UserfaultFD> uffd;
  std::unique_ptr<SwapFile> swap_file;
};

// UserspaceSwapMechanismData contains process node specific details and
// handles.
class UserspaceSwapMechanismData
    : public ExternalNodeAttachedDataImpl<UserspaceSwapMechanismData> {
 public:
  explicit UserspaceSwapMechanismData(const ProcessNode* node)
      : swap_data(new RendererSwapData) {}
  ~UserspaceSwapMechanismData() override = default;

  // Note: This is a unique_ptr because it will be used with code that is added
  // in a follow up CL.
  std::unique_ptr<RendererSwapData> swap_data;
};

void InitializeProcessNodeOnGraph(int render_process_host_id,
                                  base::ScopedFD uffd,
                                  performance_manager::Graph* graph) {
  // Now look up the ProcessNode so we can complete initialization.
  DCHECK(graph);
  DCHECK(uffd.is_valid());

  const ProcessNode* process_node = nullptr;
  for (const ProcessNode* proc : graph->GetAllProcessNodes()) {
    if (proc->GetRenderProcessHostId().GetUnsafeValue() ==
        render_process_host_id) {
      process_node = proc;
    }
  }

  if (!process_node) {
    LOG(ERROR) << "Couldn't find process node for RPH: "
               << render_process_host_id;
    return;
  }

  if (UserspaceSwapMechanismData::Destroy(process_node)) {
    LOG(ERROR) << "ProcessNode contained UserspaceSwapMechanismData";
    return;
  }

  auto* data = UserspaceSwapMechanismData::GetOrCreate(process_node);
  auto& swap_data = data->swap_data;

  swap_data->render_process_host_id = render_process_host_id;

  // Finally wrap up the received userfaultfd into a UserfaultFD instance.
  swap_data->uffd = UserfaultFD::WrapFD(std::move(uffd));

  // The SwapFile is always encrypted but the compression layer is optional.
  SwapFile::Type swap_type = SwapFile::Type::kEncrypted;
  if (UserspaceSwapConfig::Get().use_compressed_swap_file) {
    swap_type =
        static_cast<SwapFile::Type>(swap_type | SwapFile::Type::kCompressed);
  }

  swap_data->swap_file = SwapFile::Create(swap_type);

  if (!swap_data->swap_file) {
    PLOG(ERROR) << "Unable to complete userspace swap initialization failure "
                   "creating swap file";

    // If we can't create a swap file, then we will bail freeing our resources.
    UserspaceSwapMechanismData::Destroy(process_node);

    return;
  }

  swap_data->setup_complete = true;
}

}  // namespace

bool IsEligibleToSwap(const ProcessNode* process_node) {
  if (!process_node->GetProcess().IsValid()) {
    return false;
  }

  auto* data = UserspaceSwapMechanismData::Get(process_node);
  if (!data) {
    return false;
  }

  auto& swap_data = data->swap_data;
  if (!swap_data->setup_complete || !swap_data->swap_file || !swap_data->uffd) {
    return false;
  }

  return true;
}

UserspaceSwapInitializationImpl::UserspaceSwapInitializationImpl(
    int render_process_host_id)
    : render_process_host_id_(render_process_host_id) {
  CHECK(UserspaceSwapInitializationImpl::UserspaceSwapSupportedAndEnabled());
}

UserspaceSwapInitializationImpl::~UserspaceSwapInitializationImpl() = default;

// static
bool UserspaceSwapInitializationImpl::UserspaceSwapSupportedAndEnabled() {
  return chromeos::memory::userspace_swap::UserspaceSwapSupportedAndEnabled();
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
    uint64_t error,
    mojo::PlatformHandle uffd_handle,
    TransferUserfaultFDCallback cb) {
  base::ScopedClosureRunner scr(std::move(cb));

  if (received_transfer_cb_) {
    return;
  }
  received_transfer_cb_ = true;

  if (error != 0) {
    LOG(ERROR) << "Unable to create userfaultfd for renderer: "
               << base::safe_strerror(error);
    return;
  }

  if (!uffd_handle.is_valid()) {
    LOG(ERROR) << "FD received is invalid.";
    return;
  }

  // Make sure we're on the graph and complete the initialization.
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindOnce(&InitializeProcessNodeOnGraph,
                                render_process_host_id_, uffd_handle.TakeFD()));
}

}  // namespace userspace_swap
}  // namespace mechanism
}  // namespace performance_manager

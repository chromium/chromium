// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/chrome_content_browser_client_performance_manager_part.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "components/performance_manager/render_process_user_data.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace {

void BindProcessNode(
    int render_process_host_id,
    mojo::PendingReceiver<performance_manager::mojom::ProcessCoordinationUnit>
        receiver) {
  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(render_process_host_id);
  if (!render_process_host)
    return;

  performance_manager::RenderProcessUserData* user_data =
      performance_manager::RenderProcessUserData::GetForRenderProcessHost(
          render_process_host);

  DCHECK(performance_manager::PerformanceManagerImpl::GetInstance());
  performance_manager::PerformanceManagerImpl::GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&performance_manager::ProcessNodeImpl::Bind,
                                base::Unretained(user_data->process_node()),
                                std::move(receiver)));
}

}  // namespace

ChromeContentBrowserClientPerformanceManagerPart::
    ChromeContentBrowserClientPerformanceManagerPart() = default;
ChromeContentBrowserClientPerformanceManagerPart::
    ~ChromeContentBrowserClientPerformanceManagerPart() = default;

void ChromeContentBrowserClientPerformanceManagerPart::
    ExposeInterfacesToRenderer(
        service_manager::BinderRegistry* registry,
        blink::AssociatedInterfaceRegistry* associated_registry,
        content::RenderProcessHost* render_process_host) {
  registry->AddInterface(
      base::BindRepeating(&BindProcessNode, render_process_host->GetID()),
      base::SequencedTaskRunnerHandle::Get());

  // Ideally this would strictly be a "CreateForRenderProcess", but when a
  // RenderFrameHost is "resurrected" with a new process it will already have
  // user data attached. This will happen on renderer crash.
  performance_manager::RenderProcessUserData::GetOrCreateForRenderProcessHost(
      render_process_host);
}

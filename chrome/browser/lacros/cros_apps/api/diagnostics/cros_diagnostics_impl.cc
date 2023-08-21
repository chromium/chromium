// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/cros_apps/api/diagnostics/cros_diagnostics_impl.h"

#include "base/functional/bind.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/render_frame_host.h"

namespace {

blink::mojom::CrosCpuInfoPtr GetCpuInfoPostTask() {
  blink::mojom::CrosCpuInfoPtr cpu_info_mojom =
      blink::mojom::CrosCpuInfo::New();

  cpu_info_mojom->architecture_name = base::SysInfo::ProcessCPUArchitecture();
  cpu_info_mojom->num_of_processors = base::SysInfo::NumberOfProcessors();

  // Calls that may be thread-blocking.
  cpu_info_mojom->model_name = base::SysInfo::CPUModelName();

  return cpu_info_mojom;
}

}  // namespace

CrosDiagnosticsImpl::~CrosDiagnosticsImpl() = default;

// static
void CrosDiagnosticsImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::CrosDiagnostics> receiver) {
  CHECK(!CrosDiagnosticsImpl::GetForCurrentDocument(render_frame_host));
  CrosDiagnosticsImpl::CreateForCurrentDocument(render_frame_host,
                                                std::move(receiver));
}

CrosDiagnosticsImpl::CrosDiagnosticsImpl(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::CrosDiagnostics> receiver)
    : content::DocumentUserData<CrosDiagnosticsImpl>(render_frame_host),
      receiver_(this, std::move(receiver)) {}

void CrosDiagnosticsImpl::GetCpuInfo(GetCpuInfoCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&GetCpuInfoPostTask),
      base::BindOnce(&CrosDiagnosticsImpl::GetCpuInfoPostTaskCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrosDiagnosticsImpl::GetCpuInfoPostTaskCallback(
    GetCpuInfoCallback callback,
    blink::mojom::CrosCpuInfoPtr cpu_info_mojom) {
  std::move(callback).Run(std::move(cpu_info_mojom));
}

DOCUMENT_USER_DATA_KEY_IMPL(CrosDiagnosticsImpl);

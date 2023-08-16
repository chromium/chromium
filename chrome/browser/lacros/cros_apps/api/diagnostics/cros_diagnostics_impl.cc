// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/cros_apps/api/diagnostics/cros_diagnostics_impl.h"

#include "content/public/browser/render_frame_host.h"

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

DOCUMENT_USER_DATA_KEY_IMPL(CrosDiagnosticsImpl);

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/output_protection_impl.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/media/output_protection_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

// static
void OutputProtectionImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<media::mojom::OutputProtection> receiver) {
  DVLOG(2) << __func__;

  // OutputProtectionProxy requires to run on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(render_frame_host);

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new OutputProtectionImpl(*render_frame_host, std::move(receiver));
}

OutputProtectionImpl::OutputProtectionImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<media::mojom::OutputProtection> receiver)
    : DocumentService(render_frame_host, std::move(receiver)),
      render_process_id_(render_frame_host.GetProcess()->GetID()),
      render_frame_id_(render_frame_host.GetRoutingID()) {}

OutputProtectionImpl::~OutputProtectionImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void OutputProtectionImpl::QueryStatus(QueryStatusCallback callback) {
  DVLOG(2) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GetProxy()->QueryStatus(
      base::BindOnce(&OutputProtectionImpl::OnQueryStatusResult,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void OutputProtectionImpl::EnableProtection(uint32_t desired_protection_mask,
                                            EnableProtectionCallback callback) {
  DVLOG(2) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GetProxy()->EnableProtection(
      desired_protection_mask,
      base::BindOnce(&OutputProtectionImpl::OnEnableProtectionResult,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void OutputProtectionImpl::OnQueryStatusResult(QueryStatusCallback callback,
                                               bool success,
                                               uint32_t link_mask,
                                               uint32_t protection_mask) {
  DVLOG(2) << __func__ << ": success=" << success << ", link_mask=" << link_mask
           << ", protection_mask=" << protection_mask;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::move(callback).Run(success, link_mask, protection_mask);
}

void OutputProtectionImpl::OnEnableProtectionResult(
    EnableProtectionCallback callback,
    bool success) {
  DVLOG(2) << __func__ << ": success=" << success;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::move(callback).Run(success);
}

// Helper function to lazily create the |proxy_| and return it.
OutputProtectionProxy* OutputProtectionImpl::GetProxy() {
  if (!proxy_) {
    proxy_ = std::make_unique<OutputProtectionProxy>(render_process_id_,
                                                     render_frame_id_);
  }

  return proxy_.get();
}

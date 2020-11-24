// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/output_protection_proxy.h"

#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/types/display_constants.h"

namespace {

gfx::NativeView GetRenderFrameView(int render_process_id, int render_frame_id) {
  auto* host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  return host ? host->GetNativeView() : gfx::kNullNativeView;
}

}  // namespace

OutputProtectionProxy::OutputProtectionProxy(int render_process_id,
                                             int render_frame_id)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id)
#if BUILDFLAG(IS_CHROMEOS_ASH)
      ,
      output_protection_delegate_(
          // On OS_CHROMEOS, NativeView and NativeWindow are both aura::Window*.
          GetRenderFrameView(render_process_id, render_frame_id))
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
{
}

OutputProtectionProxy::~OutputProtectionProxy() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void OutputProtectionProxy::QueryStatus(QueryStatusCallback callback) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  output_protection_delegate_.QueryStatus(
      base::BindOnce(&OutputProtectionProxy::ProcessQueryStatusResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  ProcessQueryStatusResult(std::move(callback), true, 0, 0);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void OutputProtectionProxy::EnableProtection(
    uint32_t desired_method_mask,
    EnableProtectionCallback callback) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  output_protection_delegate_.SetProtection(desired_method_mask,
                                            std::move(callback));
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  NOTIMPLEMENTED();
  std::move(callback).Run(false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void OutputProtectionProxy::ProcessQueryStatusResult(
    QueryStatusCallback callback,
    bool success,
    uint32_t link_mask,
    uint32_t protection_mask) {
  DVLOG(1) << __func__ << ": " << success << ", " << link_mask;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(xjz): Investigate whether this check should be removed.
  if (!GetRenderFrameView(render_process_id_, render_frame_id_)) {
    LOG(WARNING) << "RenderFrameHost is not alive.";
    std::move(callback).Run(false, 0, 0);
    return;
  }

  uint32_t new_link_mask = link_mask;
  // If we successfully retrieved the device level status, check for capturers.
  if (success) {
    const bool is_insecure_capture_detected =
        MediaCaptureDevicesDispatcher::GetInstance()
            ->IsInsecureCapturingInProgress(render_process_id_,
                                            render_frame_id_);
    if (is_insecure_capture_detected)
      new_link_mask |= display::DISPLAY_CONNECTION_TYPE_NETWORK;
  }

  std::move(callback).Run(success, new_link_mask, protection_mask);
}

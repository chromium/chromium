// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/password_change_page_stability_waiter.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

PasswordChangePageStabilityWaiter::PasswordChangePageStabilityWaiter(
    content::WebContents* web_contents,
    base::OnceClosure callback)
    : callback_(std::move(callback)) {
  if (!web_contents || !web_contents->GetPrimaryMainFrame()) {
    OnDisconnect();
    return;
  }

  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  web_contents->GetPrimaryMainFrame()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&chrome_render_frame);

  if (!chrome_render_frame) {
    OnDisconnect();
    return;
  }

  chrome_render_frame->CreatePageStabilityMonitor(
      monitor_.BindNewPipeAndPassReceiver(), actor::TaskId(),
      /*supports_paint_stability=*/true);

  monitor_.set_disconnect_handler(
      base::BindOnce(&PasswordChangePageStabilityWaiter::OnDisconnect,
                     weak_ptr_factory_.GetWeakPtr()));

  monitor_->NotifyWhenStable(
      base::Seconds(1),
      base::BindOnce(&PasswordChangePageStabilityWaiter::OnStable,
                     weak_ptr_factory_.GetWeakPtr()));
}

PasswordChangePageStabilityWaiter::~PasswordChangePageStabilityWaiter() =
    default;

void PasswordChangePageStabilityWaiter::OnStable() {
  if (callback_) {
    std::move(callback_).Run();
  }
}

void PasswordChangePageStabilityWaiter::OnDisconnect() {
  // Even if disconnected, we invoke the callback unconditionally so that we
  // don't leave callers hanging forever in case of mojo errors.
  if (callback_) {
    std::move(callback_).Run();
  }
}

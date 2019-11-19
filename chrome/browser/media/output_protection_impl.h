// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_OUTPUT_PROTECTION_IMPL_H_
#define CHROME_BROWSER_MEDIA_OUTPUT_PROTECTION_IMPL_H_

#include "content/public/browser/frame_service_base.h"
#include "media/mojo/mojom/output_protection.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

class OutputProtectionProxy;

namespace content {
class RenderFrameHost;
}

// Implements media::mojom::OutputProtection to check display links and
// their statuses. On all platforms we'll check the network links. On ChromeOS
// we'll also check the hardware links. Can only be used on the UI thread.
class OutputProtectionImpl final
    : public content::FrameServiceBase<media::mojom::OutputProtection> {
 public:
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<media::mojom::OutputProtection> receiver);

  OutputProtectionImpl(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<media::mojom::OutputProtection> receiver);

  // media::mojom::OutputProtection implementation.
  void QueryStatus(QueryStatusCallback callback) final;
  void EnableProtection(uint32_t desired_protection_mask,
                        EnableProtectionCallback callback) final;

 private:
  // |this| can only be destructed as a FrameServiceBase.
  ~OutputProtectionImpl() final;

  // Callbacks for QueryStatus and EnableProtection results.
  // Note: These are bound using weak pointers so that we won't fire |callback|
  // after the binding is destroyed.
  void OnQueryStatusResult(QueryStatusCallback callback,
                           bool success,
                           uint32_t link_mask,
                           uint32_t protection_mask);
  void OnEnableProtectionResult(EnableProtectionCallback callback,
                                bool success);

  // Helper function to lazily create the |proxy_| and return it.
  OutputProtectionProxy* GetProxy();

  // TODO(crbug.com/770958): Remove these IDs after OutputProtection PPAPI is
  // deprecated.
  const int render_process_id_;
  const int render_frame_id_;

  std::unique_ptr<OutputProtectionProxy> proxy_;

  base::WeakPtrFactory<OutputProtectionImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_OUTPUT_PROTECTION_IMPL_H_

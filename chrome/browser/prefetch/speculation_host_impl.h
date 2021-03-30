// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_SPECULATION_HOST_IMPL_H_
#define CHROME_BROWSER_PREFETCH_SPECULATION_HOST_IMPL_H_

#include <vector>

#include "content/public/browser/frame_service_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {
class RenderFrameHost;
}  // namespace content

// Receiver for speculation rules from the web platform. See
// third_party/blink/renderer/core/speculation_rules/README.md
class SpeculationHostImpl
    : public content::FrameServiceBase<blink::mojom::SpeculationHost> {
 public:
  // Creates and binds an instance of this per-frame.
  static void Bind(
      content::RenderFrameHost* frame_host,
      mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver);

  ~SpeculationHostImpl() override;

 private:
  SpeculationHostImpl(
      content::RenderFrameHost* frame_host,
      mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver);

  void UpdateSpeculationCandidates(
      std::vector<blink::mojom::SpeculationCandidatePtr> candidates) override;

  // Track if an update has been received. The current implementation only
  // processes one update per document. At present, updates after the first are
  // ignored.
  bool received_update_ = false;
};

#endif  // CHROME_BROWSER_PREFETCH_SPECULATION_HOST_IMPL_H_

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_ANCHOR_ELEMENT_PRELOADER_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_ANCHOR_ELEMENT_PRELOADER_H_

#include "content/public/browser/document_service.h"
#include "third_party/blink/public/mojom/loader/anchor_element_interaction_host.mojom.h"

extern const char kPreloadingAnchorElementPreloaderPreloadingTriggered[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AnchorElementPreloaderType {
  kUnspecified = 0,
  kPreconnect = 1,
  kMaxValue = kPreconnect,
};

class AnchorElementPreloader
    : content::DocumentService<blink::mojom::AnchorElementInteractionHost> {
 public:
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::AnchorElementInteractionHost>
          receiver);

 private:
  AnchorElementPreloader(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::AnchorElementInteractionHost>
          receiver);

  // Preconnects to the given URL `target`.
  void OnPointerDown(const GURL& target) override;

  void RecordUmaPreloadedTriggered(AnchorElementPreloaderType);

  void RecordUkmPreloadType(AnchorElementPreloaderType);
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_ANCHOR_ELEMENT_PRELOADER_H_

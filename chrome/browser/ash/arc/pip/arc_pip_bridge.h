// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_PIP_ARC_PIP_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_PIP_ARC_PIP_BRIDGE_H_

#include <memory>

#include "ash/components/arc/mojom/pip.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {

class BrowserContext;

}  // namespace content

namespace arc {

class ArcBridgeService;
class ArcPictureInPictureWindowControllerImpl;

class ArcPipBridge : public KeyedService,
                     public ConnectionObserver<mojom::PipInstance>,
                     public mojom::PipHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcPipBridge* GetForBrowserContext(content::BrowserContext* context);

  ArcPipBridge(content::BrowserContext* context,
               ArcBridgeService* bridge_service);

  ArcPipBridge(const ArcPipBridge&) = delete;
  ArcPipBridge& operator=(const ArcPipBridge&) = delete;

  ~ArcPipBridge() override;

  // ConnectionObserver<mojom::PipInstance> overrides.
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // PipHost overrides.
  void OnPipEvent(arc::mojom::ArcPipEvent event) override;

  // PipInstance methods:
  void ClosePip();

  static void EnsureFactoryBuilt();

 private:
  const raw_ptr<ArcBridgeService> arc_bridge_service_;

  std::unique_ptr<ArcPictureInPictureWindowControllerImpl>
      pip_window_controller_;

  bool prevent_closing_pip_ = false;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_PIP_ARC_PIP_BRIDGE_H_

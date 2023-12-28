// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_MEDIA_SESSION_ARC_MEDIA_SESSION_BRIDGE_H_
#define ASH_COMPONENTS_ARC_MEDIA_SESSION_ARC_MEDIA_SESSION_BRIDGE_H_

#include "ash/components/arc/mojom/media_session.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// ArcMediaSessionBridge exposes the media session service to ARC. This allows
// Android apps to request and manage audio focus using the internal Chrome
// API. This means that audio focus management is unified across both Android
// and Chrome.
class ArcMediaSessionBridge
    : public KeyedService,
      public ConnectionObserver<mojom::MediaSessionInstance> {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcMediaSessionBridge* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcMediaSessionBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcMediaSessionBridge(content::BrowserContext* context,
                        ArcBridgeService* bridge_service);

  ArcMediaSessionBridge(const ArcMediaSessionBridge&) = delete;
  ArcMediaSessionBridge& operator=(const ArcMediaSessionBridge&) = delete;

  ~ArcMediaSessionBridge() override;

  // ConnectionObserver<mojom::MediaSessionInstance> overrides.
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  static void EnsureFactoryBuilt();

 private:
  void SetupAudioFocus();

  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_MEDIA_SESSION_ARC_MEDIA_SESSION_BRIDGE_H_

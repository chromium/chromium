// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_CAST_RECEIVER_ARC_CAST_RECEIVER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_CAST_RECEIVER_ARC_CAST_RECEIVER_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/arc/mojom/cast_receiver.mojom.h"
#include "components/arc/session/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

class PrefChangeRegistrar;

namespace arc {

class ArcBridgeService;

// Provides control of the Android Cast Receiver.
class ArcCastReceiverService
    : public KeyedService,
      public ConnectionObserver<mojom::CastReceiverInstance> {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcCastReceiverService* GetForBrowserContext(
      content::BrowserContext* context);

  ArcCastReceiverService(content::BrowserContext* context,
                         ArcBridgeService* bridge_service);
  ~ArcCastReceiverService() override;

  // ConnectionObserver<mojom::CastReceiverInstance> overrides:
  void OnConnectionReady() override;

 private:
  // Callback for when the pref for enabling the Cast Receiver changes.
  void OnCastReceiverEnabledChanged() const;

  // Callback for when the pref for naming the Cast Receiver changes.
  void OnCastReceiverNameChanged() const;

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  // Observers for preferences and settings changes.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      receiver_name_subscription_;

  DISALLOW_COPY_AND_ASSIGN(ArcCastReceiverService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_CAST_RECEIVER_ARC_CAST_RECEIVER_SERVICE_H_

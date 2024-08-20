// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_SSO_FLOATING_SSO_SERVICE_H_
#define CHROME_BROWSER_ASH_FLOATING_SSO_FLOATING_SSO_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/floating_sso/floating_sso_sync_bridge.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/model/data_type_store.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace syncer {
class DataTypeControllerDelegate;
class DataTypeLocalChangeProcessor;
}  // namespace syncer

class PrefService;

namespace ash::floating_sso {

class FloatingSsoService : public KeyedService,
                           public network::mojom::CookieChangeListener {
 public:
  FloatingSsoService(
      PrefService* prefs,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      network::mojom::CookieManager* cookie_manager,
      syncer::OnceDataTypeStoreFactory create_store_callback);
  FloatingSsoService(const FloatingSsoService&) = delete;
  FloatingSsoService& operator=(const FloatingSsoService&) = delete;

  ~FloatingSsoService() override;

  // KeyedService:
  void Shutdown() override;

  // network::mojom::CookieChangeListener:
  void OnCookieChange(const net::CookieChangeInfo& change) override;

  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate();

  FloatingSsoSyncBridge* GetBridgeForTesting() { return &bridge_; }

  // TODO: b/346354327 - temporary flag used for testing. Remove after
  // actual behavior is implemented.
  bool is_enabled_for_testing_ = false;

 private:
  // Check if the feature is enabled based on the corresponding enterprise
  // policy. If yes, start observing cookies and uploading them to Sync, and
  // apply cookies from Sync if needed. If not, stop all of the above.
  void StartOrStop();

  void MaybeStartListening();
  void BindToCookieManager();
  void OnCookiesLoaded(const net::CookieList& cookies);
  bool ShouldSyncCookie(const net::CanonicalCookie& cookie) const;
  void OnConnectionError();
  bool IsFloatingWorkspaceEnabled() const;

  raw_ptr<PrefService> prefs_ = nullptr;
  const raw_ptr<network::mojom::CookieManager> cookie_manager_;
  FloatingSsoSyncBridge bridge_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Whether we connect for the first time to the cookie manager or we are
  // reconnecting after a disconnect.
  bool is_initial_cookie_manager_bind_ = true;

  mojo::Receiver<network::mojom::CookieChangeListener> receiver_{this};
};

}  // namespace ash::floating_sso

#endif  // CHROME_BROWSER_ASH_FLOATING_SSO_FLOATING_SSO_SERVICE_H_

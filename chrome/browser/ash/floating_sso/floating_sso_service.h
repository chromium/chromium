// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_SSO_FLOATING_SSO_SERVICE_H_
#define CHROME_BROWSER_ASH_FLOATING_SSO_FLOATING_SSO_SERVICE_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/floating_sso/floating_sso_sync_bridge.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/model/data_type_store.h"
#include "components/url_matcher/url_matcher.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

class PrefService;

namespace ash::floating_sso {

class FloatingSsoService : public KeyedService,
                           public network::mojom::CookieChangeListener,
                           public FloatingSsoSyncBridge::Observer {
 public:
  // Callback used to get a CookieManager.
  using CookieManagerGetter =
      base::RepeatingCallback<network::mojom::CookieManager*()>;

  FloatingSsoService(PrefService* prefs,
                     std::unique_ptr<FloatingSsoSyncBridge> bridge,
                     CookieManagerGetter cookie_manager_getter);
  FloatingSsoService(const FloatingSsoService&) = delete;
  FloatingSsoService& operator=(const FloatingSsoService&) = delete;

  ~FloatingSsoService() override;

  // KeyedService:
  void Shutdown() override;

  // network::mojom::CookieChangeListener:
  void OnCookieChange(const net::CookieChangeInfo& change) override;

  // FloatingSsoSyncBridge::Observer:
  void OnCookiesAddedOrUpdatedRemotely(
      const std::vector<net::CanonicalCookie>& cookies) override;
  void OnCookiesRemovedRemotely(
      const std::vector<net::CanonicalCookie>& cookies) override;

  bool IsFloatingSsoEnabled();
  // `callback` will be run once there are no cookie manager changes in
  // progress. This can be called repeatedly but only the latest callback will
  // be executed.
  void RunWhenCookiesAreReady(base::OnceClosure callback);
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate();

  FloatingSsoSyncBridge* GetBridgeForTesting() { return bridge_.get(); }
  bool IsBoundToCookieManagerForTesting() { return receiver_.is_bound(); }

 private:
  void RegisterPolicyListeners();

  // Map the FloatingSsoDomainBlocklist and FloatingSsoDomainBlocklistExceptions
  // policies to URL matchers.
  void UpdateUrlMatchers();

  // Check if the feature is enabled based on the corresponding enterprise
  // policy. If yes, start observing cookies and uploading them to Sync, and
  // apply cookies from Sync if needed. If not, stop all of the above.
  void StartOrStop();

  void MaybeStartListening();
  void StopListening();
  void BindToCookieManager();
  void OnCookiesLoaded(const net::CookieList& cookies);
  bool ShouldSyncCookie(const net::CanonicalCookie& cookie) const;
  void OnConnectionError();
  bool IsDomainAllowed(const net::CanonicalCookie& cookie) const;
  void OnCookieSet(net::CookieAccessResult result);
  void OnCookieDeleted(bool success);
  void DecrementChangesCountAndMaybeNotify();

  raw_ptr<PrefService> prefs_ = nullptr;
  // TODO(crbug.com/378091718): investigated lifetime issues when using
  // raw_ptr<network::mojom::CookieManager> instead of a callback here.
  CookieManagerGetter cookie_manager_getter_;
  std::unique_ptr<FloatingSsoSyncBridge> bridge_;
  base::ScopedObservation<FloatingSsoSyncBridge,
                          FloatingSsoSyncBridge::Observer>
      scoped_observation_{this};
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // We fetch the accumulated cookies the first time the service is started, as
  // well when the service stops listening and resumes due to setting changes.
  // We do not fetch accumulated cookies when the connection to the cookie
  // manager is disrupted because we attempt to reconnect right away.
  bool fetch_accumulated_cookies_ = true;

  // Count of changes (additions or deletions of cookies) currently being
  // performed by `cookie_manager_`.
  // TODO(crbug.com/377471962): Add explicit tests for this counter and for
  // `on_no_changes_in_progress_callback_`.
  int changes_in_progress_count_ = 0;

  base::OnceClosure on_no_changes_in_progress_callback_;

  mojo::Receiver<network::mojom::CookieChangeListener> receiver_{this};

  std::unique_ptr<url_matcher::URLMatcher> block_url_matcher_;
  std::unique_ptr<url_matcher::URLMatcher> except_url_matcher_;
};

}  // namespace ash::floating_sso

#endif  // CHROME_BROWSER_ASH_FLOATING_SSO_FLOATING_SSO_SERVICE_H_

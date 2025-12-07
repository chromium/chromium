// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_DICE_BOUND_SESSION_COOKIE_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_DICE_BOUND_SESSION_COOKIE_SERVICE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/device_bound_sessions.mojom.h"

namespace signin {
class IdentityManager;
}

namespace content {
class StoragePartition;
}

// Supports cookie binding for DICe profiles including:
// - On bound session termination, expire short lived bound cookies if needed
// and trigger /ListAccounts to ensure account consistency is maintained.
// - TODO(b/312719798): Support setting a new bound session from OAuthMultiLogin
// response.
// Note: This work can't be done directly in `BoundSessionCookieRefreshService`
// as it produces circular dependency.
class DiceBoundSessionCookieService
    : public KeyedService,
      public BoundSessionCookieRefreshService::Observer,
      public network::mojom::DeviceBoundSessionAccessObserver {
 public:
  DiceBoundSessionCookieService(
      BoundSessionCookieRefreshService& bound_session_cookie_refresh_service,
      signin::IdentityManager& identity_manager,
      content::StoragePartition& storage_partition);

  ~DiceBoundSessionCookieService() override;

  DiceBoundSessionCookieService(const DiceBoundSessionCookieService&) = delete;
  DiceBoundSessionCookieService& operator=(
      const DiceBoundSessionCookieService&) = delete;

  // BoundSessionCookieRefreshService::Observer:
  void OnBoundSessionTerminated(
      const GURL& site,
      const base::flat_set<std::string>& bound_cookie_names) override;

  // network::mojom::DeviceBoundSessionAccessObserver:
  void OnDeviceBoundSessionAccessed(
      const net::device_bound_sessions::SessionAccess& access) override;
  void Clone(
      mojo::PendingReceiver<network::mojom::DeviceBoundSessionAccessObserver>
          observer) override;

 private:
  void DeleteCookies(const base::flat_set<std::string>& bound_cookie_names);
  void TriggerCookieJarUpdate();

  const raw_ref<signin::IdentityManager> identity_manager_;
  const raw_ref<content::StoragePartition> storage_partition_;
  base::ScopedObservation<BoundSessionCookieRefreshService,
                          BoundSessionCookieRefreshService::Observer>
      bound_session_cookie_refresh_service_observer_{this};
  mojo::Receiver<network::mojom::DeviceBoundSessionAccessObserver> receiver_{
      this};
  base::WeakPtrFactory<DiceBoundSessionCookieService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_DICE_BOUND_SESSION_COOKIE_SERVICE_H_

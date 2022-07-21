// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager.h"

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zone.h"
#include "chrome/browser/ash/printing/oauth2/profile_auth_servers_sync_bridge.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chromeos/printing/uri.h"
#include "components/sync/model/model_type_store_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace ash::printing::oauth2 {

namespace {

class AuthorizationZonesManagerImpl
    : public AuthorizationZonesManager,
      private ProfileAuthServersSyncBridge::Observer {
 public:
  explicit AuthorizationZonesManagerImpl(Profile* profile)
      : sync_bridge_(ProfileAuthServersSyncBridge::Create(
            this,
            ModelTypeStoreServiceFactory::GetForProfile(profile)
                ->GetStoreFactory())),
        url_loader_factory_(profile->GetURLLoaderFactory()) {}

  StatusCode SaveAuthorizationServerAsTrusted(
      const GURL& auth_server) override {
    return ValidateURLAndSave(
        auth_server,
        AuthorizationZone::Create(url_loader_factory_, auth_server));
  }

  StatusCode SaveAuthorizationServerAsTrustedForTesting(
      const GURL& auth_server,
      std::unique_ptr<AuthorizationZone> auth_zone) override {
    return ValidateURLAndSave(auth_server, std::move(auth_zone));
  }

  void InitAuthorization(const GURL& auth_server,
                         const std::string& scope,
                         StatusCallback callback) override {
    AuthorizationZone* zone = GetAuthorizationZone(auth_server);
    if (!zone) {
      std::move(callback).Run(StatusCode::kUnknownAuthorizationServer,
                              auth_server.possibly_invalid_spec());
      return;
    }

    zone->InitAuthorization(scope, std::move(callback));
  }

  void FinishAuthorization(const GURL& auth_server,
                           const GURL& redirect_url,
                           StatusCallback callback) override {
    AuthorizationZone* zone = GetAuthorizationZone(auth_server);
    if (!zone) {
      std::move(callback).Run(StatusCode::kUnknownAuthorizationServer,
                              auth_server.possibly_invalid_spec());
      return;
    }

    zone->FinishAuthorization(redirect_url, std::move(callback));
  }

  void GetEndpointAccessToken(const GURL& auth_server,
                              const chromeos::Uri& ipp_endpoint,
                              const std::string& scope,
                              StatusCallback callback) override {
    AuthorizationZone* zone = GetAuthorizationZone(auth_server);
    if (!zone) {
      std::move(callback).Run(StatusCode::kUnknownAuthorizationServer,
                              auth_server.possibly_invalid_spec());
      return;
    }

    zone->GetEndpointAccessToken(ipp_endpoint, scope, std::move(callback));
  }

  void MarkEndpointAccessTokenAsExpired(
      const GURL& auth_server,
      const chromeos::Uri& ipp_endpoint,
      const std::string& endpoint_access_token) override {
    AuthorizationZone* zone = GetAuthorizationZone(auth_server);
    if (!zone) {
      return;
    }

    zone->MarkEndpointAccessTokenAsExpired(ipp_endpoint, endpoint_access_token);
  }

 private:
  // Helper method for adding a new element to `servers_`.
  StatusCode ValidateURLAndSave(const GURL& auth_server,
                                std::unique_ptr<AuthorizationZone> auth_zone) {
    if (!auth_server.is_valid() || !auth_server.SchemeIs("https") ||
        !auth_server.has_host() || auth_server.has_username() ||
        auth_server.has_query() || auth_server.has_ref()) {
      // TODO(pawliczek): log why the URL is invalid
      return StatusCode::kInvalidURL;
    }
    if (!base::Contains(servers_, auth_server)) {
      servers_.emplace(auth_server, std::move(auth_zone));
    }
    return StatusCode::kOK;
  }

  // Returns a pointer to the corresponding element in `servers_` or nullptr if
  // `auth_server` is unknown.
  AuthorizationZone* GetAuthorizationZone(const GURL& auth_server) {
    auto it_server = servers_.find(auth_server);
    if (it_server == servers_.end()) {
      return nullptr;
    }
    return it_server->second.get();
  }

  syncer::ModelTypeSyncBridge* GetModelTypeSyncBridge() override {
    return sync_bridge_.get();
  }

  void OnProfileAuthorizationServersInitialized() override {
    // TODO(pawliczek)
    NOTIMPLEMENTED();
  }

  void OnProfileAuthorizationServersUpdate(std::set<GURL> removed,
                                           std::set<GURL> added) override {
    // TODO(pawliczek)
    NOTIMPLEMENTED();
  }

  std::unique_ptr<ProfileAuthServersSyncBridge> sync_bridge_;
  std::map<GURL, std::unique_ptr<AuthorizationZone>> servers_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace

std::unique_ptr<AuthorizationZonesManager> AuthorizationZonesManager::Create(
    Profile* profile) {
  DCHECK(profile);
  return std::make_unique<AuthorizationZonesManagerImpl>(profile);
}

AuthorizationZonesManager::~AuthorizationZonesManager() = default;

AuthorizationZonesManager::AuthorizationZonesManager() = default;

}  // namespace ash::printing::oauth2

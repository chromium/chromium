// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager.h"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zone.h"
#include "chrome/browser/ash/printing/oauth2/client_ids_database.h"
#include "chrome/browser/ash/printing/oauth2/log_entry.h"
#include "chrome/browser/ash/printing/oauth2/profile_auth_servers_sync_bridge.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chromeos/printing/uri.h"
#include "components/device_event_log/device_event_log.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_store_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace ash::printing::oauth2 {

namespace {

// Logs results to device-log and calls `callback` with parameters `status` and
// `data`.
void LogAndCall(StatusCallback callback,
                std::string_view method,
                const GURL& auth_server,
                const chromeos::Uri& ipp_endpoint,
                StatusCode status,
                std::string data) {
  if (status == StatusCode::kOK || status == StatusCode::kAuthorizationNeeded) {
    PRINTER_LOG(EVENT) << LogEntry((status == StatusCode::kOK) ? "" : data,
                                   method, auth_server, status, ipp_endpoint);
  } else {
    PRINTER_LOG(ERROR) << LogEntry(data, method, auth_server, status,
                                   ipp_endpoint);
  }
  std::move(callback).Run(status, std::move(data));
}

void AddLoggingToCallback(StatusCallback& callback,
                          std::string_view method,
                          const GURL& auth_server,
                          const chromeos::Uri& ipp_endpoint = chromeos::Uri()) {
  // Wrap the `callback` with the function LogAndCall() defined above.
  auto new_call = base::BindOnce(&LogAndCall, std::move(callback), method,
                                 auth_server, ipp_endpoint);
  callback = std::move(new_call);
}

class AuthorizationZonesManagerImpl
    : public AuthorizationZonesManager,
      private ProfileAuthServersSyncBridge::Observer {
 public:
  explicit AuthorizationZonesManagerImpl(Profile* profile)
      : client_ids_database_(ClientIdsDatabase::Create()),
        sync_bridge_(ProfileAuthServersSyncBridge::Create(
            this,
            DataTypeStoreServiceFactory::GetForProfile(profile)
                ->GetStoreFactory())),
        url_loader_factory_(profile->GetURLLoaderFactory()),
        auth_zone_creator_(base::BindRepeating(AuthorizationZone::Create,
                                               url_loader_factory_)) {}

  // Constructor for testing.
  AuthorizationZonesManagerImpl(
      Profile* profile,
      CreateAuthZoneCallback auth_zone_creator,
      std::unique_ptr<ClientIdsDatabase> client_ids_database,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory store_factory)
      : client_ids_database_(std::move(client_ids_database)),
        sync_bridge_(ProfileAuthServersSyncBridge::CreateForTesting(
            this,
            std::move(change_processor),
            std::move(store_factory))),
        url_loader_factory_(profile->GetURLLoaderFactory()),
        auth_zone_creator_(std::move(auth_zone_creator)) {}

  StatusCode SaveAuthorizationServerAsTrusted(
      const GURL& auth_server) override {
    if (!auth_server.is_valid() || !auth_server.SchemeIs("https") ||
        !auth_server.has_host() || auth_server.has_username() ||
        auth_server.has_query() || auth_server.has_ref()) {
      PRINTER_LOG(USER) << LogEntry("", __func__, auth_server,
                                    StatusCode::kInvalidURL);
      return StatusCode::kInvalidURL;
    }
    std::unique_ptr<AuthorizationZone> auth_zone =
        auth_zone_creator_.Run(auth_server, client_ids_database_.get());
    if (sync_bridge_->IsInitialized()) {
      if (!base::Contains(servers_, auth_server)) {
        servers_.emplace(auth_server, std::move(auth_zone));
        sync_bridge_->AddAuthorizationServer(auth_server);
      }
    } else {
      if (!base::Contains(waiting_servers_, auth_server)) {
        waiting_servers_[auth_server].server = std::move(auth_zone);
      }
    }
    PRINTER_LOG(USER) << LogEntry("", __func__, auth_server, StatusCode::kOK);
    return StatusCode::kOK;
  }

  void InitAuthorization(const GURL& auth_server,
                         const std::string& scope,
                         StatusCallback callback) override {
    PRINTER_LOG(USER) << LogEntry("scope=" + scope, __func__, auth_server);
    AddLoggingToCallback(callback, __func__, auth_server);
    AuthorizationZone* zone = GetAuthorizationZone(auth_server);

    if (!zone) {
      auto it = waiting_servers_.find(auth_server);
      if (it == waiting_servers_.end()) {
        std::move(callback).Run(StatusCode::kUntrustedAuthorizationServer, "");
      } else {
        it->second.init_calls.emplace_back(
            InitAuthorizationCall{scope, std::move(callback)});
      }
      return;
    }

    zone->InitAuthorization(scope, std::move(callback));
  }

  void FinishAuthorization(const GURL& auth_server,
                           const GURL& redirect_url,
                           StatusCallback callback) override {
    PRINTER_LOG(USER) << LogEntry("", __func__, auth_server);
    AddLoggingToCallback(callback, __func__, auth_server);

    AuthorizationZone* zone = GetAuthorizationZone(auth_server);
    if (!zone) {
      const StatusCode code = base::Contains(waiting_servers_, auth_server)
                                  ? StatusCode::kAuthorizationNeeded
                                  : StatusCode::kUntrustedAuthorizationServer;
      std::move(callback).Run(code, "");
      return;
    }

    zone->FinishAuthorization(redirect_url, std::move(callback));
  }

  void GetEndpointAccessToken(const GURL& auth_server,
                              const chromeos::Uri& ipp_endpoint,
                              const std::string& scope,
                              StatusCallback callback) override {
    PRINTER_LOG(USER) << LogEntry("scope=" + scope, __func__, auth_server,
                                  std::nullopt, ipp_endpoint);
    AddLoggingToCallback(callback, __func__, auth_server, ipp_endpoint);

    AuthorizationZone* zone = GetAuthorizationZone(auth_server);
    if (!zone) {
      const StatusCode code = base::Contains(waiting_servers_, auth_server)
                                  ? StatusCode::kAuthorizationNeeded
                                  : StatusCode::kUntrustedAuthorizationServer;
      std::move(callback).Run(code, "");
      return;
    }

    zone->GetEndpointAccessToken(ipp_endpoint, scope, std::move(callback));
  }

  void MarkEndpointAccessTokenAsExpired(
      const GURL& auth_server,
      const chromeos::Uri& ipp_endpoint,
      const std::string& endpoint_access_token) override {
    AuthorizationZone* zone = GetAuthorizationZone(auth_server);
    PRINTER_LOG(EVENT) << LogEntry(
        "", __func__, auth_server,
        zone ? StatusCode::kOK : StatusCode::kUntrustedAuthorizationServer,
        ipp_endpoint);
    if (zone) {
      zone->MarkEndpointAccessTokenAsExpired(ipp_endpoint,
                                             endpoint_access_token);
    }
  }

 private:
  struct InitAuthorizationCall {
    std::string scope;
    StatusCallback callback;
  };
  struct WaitingServer {
    std::unique_ptr<AuthorizationZone> server;
    std::vector<InitAuthorizationCall> init_calls;
  };

  // Returns a pointer to the corresponding element in `servers_` or nullptr if
  // `auth_server` is untrusted.
  AuthorizationZone* GetAuthorizationZone(const GURL& auth_server) {
    auto it_server = servers_.find(auth_server);
    if (it_server == servers_.end()) {
      return nullptr;
    }
    return it_server->second.get();
  }

  syncer::DataTypeSyncBridge* GetDataTypeSyncBridge() override {
    return sync_bridge_.get();
  }

  void OnProfileAuthorizationServersInitialized() override {
    for (auto& [url, ws] : waiting_servers_) {
      auto [it, created] = servers_.emplace(url, std::move(ws.server));
      if (created) {
        sync_bridge_->AddAuthorizationServer(url);
      }
      for (InitAuthorizationCall& iac : ws.init_calls) {
        it->second->InitAuthorization(iac.scope, std::move(iac.callback));
      }
    }
    waiting_servers_.clear();
  }

  void OnProfileAuthorizationServersUpdate(std::set<GURL> added,
                                           std::set<GURL> deleted) override {
    for (const GURL& url : deleted) {
      auto it = servers_.find(url);
      if (it == servers_.end()) {
        continue;
      }
      // First, we have to remove the AuthorizationZone from `servers_` to make
      // sure it is not accessed in any callbacks returned by
      // MarkAuthorizationZoneAsUntrusted().
      std::unique_ptr<AuthorizationZone> auth_zone = std::move(it->second);
      servers_.erase(it);
      auth_zone->MarkAuthorizationZoneAsUntrusted();
    }
    for (const GURL& url : added) {
      if (!base::Contains(servers_, url)) {
        servers_.emplace(
            url, auth_zone_creator_.Run(url, client_ids_database_.get()));
      }
    }
  }

  // Must live longer than all instances of AuthorizationZone
  // (`waiting_servers_` and `servers_`).
  std::unique_ptr<ClientIdsDatabase> client_ids_database_;

  std::map<GURL, WaitingServer> waiting_servers_;
  std::unique_ptr<ProfileAuthServersSyncBridge> sync_bridge_;
  std::map<GURL, std::unique_ptr<AuthorizationZone>> servers_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  CreateAuthZoneCallback auth_zone_creator_;
};

}  // namespace

std::unique_ptr<AuthorizationZonesManager> AuthorizationZonesManager::Create(
    Profile* profile) {
  DCHECK(profile);
  return std::make_unique<AuthorizationZonesManagerImpl>(profile);
}

std::unique_ptr<AuthorizationZonesManager>
AuthorizationZonesManager::CreateForTesting(
    Profile* profile,
    CreateAuthZoneCallback auth_zone_creator,
    std::unique_ptr<ClientIdsDatabase> client_ids_database,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory store_factory) {
  DCHECK(profile);
  return std::make_unique<AuthorizationZonesManagerImpl>(
      profile, std::move(auth_zone_creator), std::move(client_ids_database),
      std::move(change_processor), std::move(store_factory));
}

AuthorizationZonesManager::~AuthorizationZonesManager() = default;

AuthorizationZonesManager::AuthorizationZonesManager() = default;

}  // namespace ash::printing::oauth2

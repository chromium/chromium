// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/client_ids_database.h"

#include <memory>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "base/check_deref.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace ash::printing::oauth2 {

class ClientIdsDatabaseImpl : public ClientIdsDatabase {
 public:
  // `local_state` must be non-null and must outlive `this`.
  explicit ClientIdsDatabaseImpl(PrefService* local_state)
      : local_state_(CHECK_DEREF(local_state)),
        data_(local_state
                  ->GetDict(ash::prefs::kPrintingOAuth2AuthorizationServers)
                  .Clone()) {}

  ClientIdsDatabaseImpl(const ClientIdsDatabaseImpl&) = delete;
  ClientIdsDatabaseImpl& operator=(const ClientIdsDatabaseImpl&) = delete;
  ~ClientIdsDatabaseImpl() override = default;

  void FetchId(const GURL& url, StatusCallback callback) override {
    const std::string* value = data_.FindString(url.spec());
    std::move(callback).Run(StatusCode::kOK, (value ? *value : ""));
  }

  void StoreId(const GURL& url, const std::string& client_id) override {
    const std::string key = url.spec();
    DCHECK(!data_.FindString(key));
    DCHECK(!client_id.empty());
    data_.Set(key, client_id);
    local_state_->SetDict(ash::prefs::kPrintingOAuth2AuthorizationServers,
                          data_.Clone());
  }

 private:
  const raw_ref<PrefService> local_state_;
  base::DictValue data_;
};

std::unique_ptr<ClientIdsDatabase> ClientIdsDatabase::Create(
    PrefService* local_state) {
  return std::make_unique<ClientIdsDatabaseImpl>(local_state);
}

void ClientIdsDatabase::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(
      ash::prefs::kPrintingOAuth2AuthorizationServers);
}

}  // namespace ash::printing::oauth2

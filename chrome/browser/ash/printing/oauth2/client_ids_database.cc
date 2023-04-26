// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/client_ids_database.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace ash::printing::oauth2 {

class ClientIdsDatabaseImpl : public ClientIdsDatabase {
 public:
  ClientIdsDatabaseImpl()
      : pref_(g_browser_process->local_state()),
        data_(pref_->GetDict(prefs::kPrintingOAuth2AuthorizationServers)
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
    pref_->SetDict(prefs::kPrintingOAuth2AuthorizationServers, data_.Clone());
  }

 private:
  raw_ptr<PrefService> pref_;
  base::Value::Dict data_;
};

std::unique_ptr<ClientIdsDatabase> ClientIdsDatabase::Create() {
  return std::make_unique<ClientIdsDatabaseImpl>();
}

void ClientIdsDatabase::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kPrintingOAuth2AuthorizationServers);
}

}  // namespace ash::printing::oauth2

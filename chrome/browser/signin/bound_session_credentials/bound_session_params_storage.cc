// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_params_storage.h"

#include <optional>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/gurl.h"

namespace {

const char kBoundSessionParamsPref[] =
    "bound_session_credentials_bound_session_params";

class BoundSessionParamsPrefsStorage : public BoundSessionParamsStorage {
 public:
  explicit BoundSessionParamsPrefsStorage(PrefService& pref_service);
  ~BoundSessionParamsPrefsStorage() override;

  [[nodiscard]] bool SaveParams(
      const bound_session_credentials::BoundSessionParams& params) override;

  std::vector<bound_session_credentials::BoundSessionParams>
  ReadAllParamsAndCleanStorageIfNecessary() override;

  bool ClearParams(const GURL& site, std::string_view session_id) override;

  void ClearAllParams() override;

 private:
  const raw_ref<PrefService> pref_service_;
};

BoundSessionParamsPrefsStorage::BoundSessionParamsPrefsStorage(
    PrefService& pref_service)
    : pref_service_(pref_service) {}

BoundSessionParamsPrefsStorage::~BoundSessionParamsPrefsStorage() = default;

bool BoundSessionParamsPrefsStorage::SaveParams(
    const bound_session_credentials::BoundSessionParams& params) {
  // TODO(b/300627729): limit the maximum number of saved sessions.
  if (!bound_session_credentials::AreParamsValid(params)) {
    return false;
  }

  std::string serialized_params = params.SerializeAsString();
  if (serialized_params.empty()) {
    return false;
  }

  ScopedDictPrefUpdate update(&pref_service_.get(), kBoundSessionParamsPref);
  base::Value::Dict* site_dict = update->EnsureDict(params.site());
  CHECK(site_dict);
  site_dict->Set(params.session_id(), base::Base64Encode(serialized_params));
  return true;
}

std::vector<bound_session_credentials::BoundSessionParams>
BoundSessionParamsPrefsStorage::ReadAllParamsAndCleanStorageIfNecessary() {
  const base::Value::Dict& root =
      pref_service_->GetDict(kBoundSessionParamsPref);

  std::vector<bound_session_credentials::BoundSessionParams> result;
  bool clean_up_needed = false;
  for (const auto [site, sessions] : root) {
    const base::Value::Dict* sessions_dict = sessions.GetIfDict();
    if (!sessions_dict) {
      clean_up_needed = true;
      continue;
    }

    if (sessions_dict->empty()) {
      clean_up_needed = true;
      continue;
    }

    for (const auto [session_id, encoded_params] : *sessions_dict) {
      const std::string* encoded_params_str = encoded_params.GetIfString();
      if (!encoded_params_str) {
        clean_up_needed = true;
        continue;
      }
      std::string params_str;
      if (!base::Base64Decode(*encoded_params_str, &params_str)) {
        clean_up_needed = true;
        continue;
      }
      bound_session_credentials::BoundSessionParams params;
      if (!params.ParseFromString(params_str)) {
        clean_up_needed = true;
        continue;
      }
      if (site != params.site()) {
        clean_up_needed = true;
        continue;
      }
      // Canonicalize `params.site()` if needed. Fix for
      // https://crbug.com/349411334.
      GURL site_url(params.site());
      if (site_url.spec() != params.site()) {
        params.set_site(site_url.spec());
        clean_up_needed = true;
      }
      // Populate `params.refresh_url()` if needed. Migration for
      // https://crbug.com/325441004.
      if (!params.has_refresh_url()) {
        params.set_refresh_url(
            GaiaUrls::GetInstance()->rotate_bound_cookies_url().spec());
        clean_up_needed = true;
      }

      if (bound_session_credentials::AreParamsValid(params)) {
        result.push_back(std::move(params));
      } else {
        clean_up_needed = true;
      }
    }
  }

  if (clean_up_needed) {
    ScopedDictPrefUpdate update(&pref_service_.get(), kBoundSessionParamsPref);
    update->clear();
    // Write all patched valid entries from scratch. Do not use `SaveParams()`
    // to avoid sending multiple pref updates.
    for (const auto& params : result) {
      std::string serialized_params = params.SerializeAsString();
      if (serialized_params.empty()) {
        // Valid entries should be serializable. If this case is hit (which
        // shouldn't normally happen), the session data will be lost at the next
        // startup.
        continue;
      }

      base::Value::Dict* site_dict = update->EnsureDict(params.site());
      CHECK(site_dict);
      site_dict->Set(params.session_id(),
                     base::Base64Encode(serialized_params));
    }
  }

  return result;
}

bool BoundSessionParamsPrefsStorage::ClearParams(const GURL& site,
                                                 std::string_view session_id) {
  ScopedDictPrefUpdate update(&pref_service_.get(), kBoundSessionParamsPref);
  base::Value::Dict* site_dict = update->FindDict(site.spec());
  if (!site_dict) {
    return false;
  }
  bool result = site_dict->Remove(session_id);
  if (site_dict->empty()) {
    update->Remove(site.spec());
  }
  return result;
}

void BoundSessionParamsPrefsStorage::ClearAllParams() {
  pref_service_->ClearPref(kBoundSessionParamsPref);
}

class BoundSessionParamsInMemoryStorage : public BoundSessionParamsStorage {
 public:
  BoundSessionParamsInMemoryStorage();
  ~BoundSessionParamsInMemoryStorage() override;

  [[nodiscard]] bool SaveParams(
      const bound_session_credentials::BoundSessionParams& params) override;

  std::vector<bound_session_credentials::BoundSessionParams>
  ReadAllParamsAndCleanStorageIfNecessary() override;

  bool ClearParams(const GURL& site, std::string_view session_id) override;

  void ClearAllParams() override;

 private:
  std::vector<bound_session_credentials::BoundSessionParams> in_memory_params_;
};

BoundSessionParamsInMemoryStorage::BoundSessionParamsInMemoryStorage() =
    default;
BoundSessionParamsInMemoryStorage::~BoundSessionParamsInMemoryStorage() =
    default;

bool BoundSessionParamsInMemoryStorage::SaveParams(
    const bound_session_credentials::BoundSessionParams& params) {
  // TODO(b/300627729): limit the maximum number of saved sessions.
  if (!bound_session_credentials::AreParamsValid(params)) {
    return false;
  }

  // Erase existing params for this session, if any.
  std::erase_if(in_memory_params_, [&params](const auto& saved_params) {
    return bound_session_credentials::AreSameSessionParams(params,
                                                           saved_params);
  });

  in_memory_params_.push_back(params);
  return true;
}

std::vector<bound_session_credentials::BoundSessionParams>
BoundSessionParamsInMemoryStorage::ReadAllParamsAndCleanStorageIfNecessary() {
  // No clean-up is needed for an in-memory storage as entries are always added
  // by the same binary version and validity is checked in `SaveParams()`.
  return in_memory_params_;
}

bool BoundSessionParamsInMemoryStorage::ClearParams(
    const GURL& site,
    std::string_view session_id) {
  return std::erase_if(in_memory_params_,
                       [&site, session_id](const auto& params) {
                         return params.site() == site.spec() &&
                                params.session_id() == session_id;
                       }) > 0;
}

void BoundSessionParamsInMemoryStorage::ClearAllParams() {
  in_memory_params_.clear();
}

}  // namespace

// static
std::unique_ptr<BoundSessionParamsStorage>
BoundSessionParamsStorage::CreateForProfile(Profile& profile) {
  if (profile.IsOffTheRecord()) {
    return std::make_unique<BoundSessionParamsInMemoryStorage>();
  }
  return std::make_unique<BoundSessionParamsPrefsStorage>(*profile.GetPrefs());
}

// static
std::unique_ptr<BoundSessionParamsStorage>
BoundSessionParamsStorage::CreatePrefsStorageForTesting(
    PrefService& pref_service) {
  return std::make_unique<BoundSessionParamsPrefsStorage>(pref_service);
}

// static
void BoundSessionParamsStorage::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kBoundSessionParamsPref);
}

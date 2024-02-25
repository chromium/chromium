// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/state_store.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident.h"
#include "chrome/browser/safe_browsing/incident_reporting/platform_state_store.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safe_browsing {

// StateStore::Transaction -----------------------------------------------------

StateStore::Transaction::Transaction(StateStore* store) : store_(store) {
#if DCHECK_IS_ON()
  DCHECK(!store_->has_transaction_);
  store_->has_transaction_ = true;
#endif
}

StateStore::Transaction::~Transaction() {
#if DCHECK_IS_ON()
  store_->has_transaction_ = false;
#endif
  if (pref_update_)
    platform_state_store::Store(store_->profile_, *store_->incidents_sent_);
}

void StateStore::Transaction::MarkAsReported(IncidentType type,
                                             const std::string& key,
                                             IncidentDigest digest) {
  std::string type_string(base::NumberToString(static_cast<int>(type)));
  base::Value::Dict& incidents_sent = GetPrefDict();
  base::Value::Dict* type_dict = incidents_sent.EnsureDict(type_string);
  type_dict->Set(key, base::NumberToString(digest));
}

void StateStore::Transaction::Clear(IncidentType type, const std::string& key) {
  // Nothing to do if the pref dict does not exist.
  if (!store_->incidents_sent_)
    return;

  // Use the read-only view on the preference to figure out if there is a value
  // to remove before committing to making a change since any use of GetPrefDict
  // will result in a full serialize-and-write operation on the preferences
  // store.
  std::string type_string(base::NumberToString(static_cast<int>(type)));

  const base::Value::Dict* const_type_dict =
      store_->incidents_sent_->FindDict(type_string);
  if (const_type_dict && const_type_dict->Find(key)) {
    base::Value::Dict* type_dict = GetPrefDict().FindDict(type_string);
    type_dict->Remove(key);
  }
}

void StateStore::Transaction::ClearForType(IncidentType type) {
  // Nothing to do if the pref dict does not exist.
  if (!store_->incidents_sent_)
    return;

  // Use the read-only view on the preference to figure out if there is a value
  // to remove before committing to making a change since any use of GetPrefDict
  // will result in a full serialize-and-write operation on the preferences
  // store.
  std::string type_string(base::NumberToString(static_cast<int>(type)));
  if (store_->incidents_sent_->FindDict(type_string))
    GetPrefDict().Remove(type_string);
}

void StateStore::Transaction::ClearAll() {
  // Clear the preference if it exists and contains any values.
  if (store_->incidents_sent_ && !store_->incidents_sent_->empty())
    GetPrefDict().clear();
}

base::Value::Dict& StateStore::Transaction::GetPrefDict() {
  if (!pref_update_) {
    pref_update_ = std::make_unique<ScopedDictPrefUpdate>(
        store_->profile_->GetPrefs(), prefs::kSafeBrowsingIncidentsSent);
    // Getting the dict will cause it to be created if it doesn't exist.
    // Unconditionally refresh the store's read-only view on the preference so
    // that it will always be correct.
    store_->incidents_sent_ = &pref_update_->Get();
  }
  return pref_update_->Get();
}

void StateStore::Transaction::ReplacePrefDict(base::Value::Dict pref_dict) {
  GetPrefDict() = std::move(pref_dict);
}

// StateStore ------------------------------------------------------------------

StateStore::StateStore(Profile* profile)
    : profile_(profile),
      incidents_sent_(nullptr)
#if DCHECK_IS_ON()
      ,
      has_transaction_(false)
#endif
{
  // Cache a read-only view of the preference.
  incidents_sent_ =
      &profile_->GetPrefs()->GetDict(prefs::kSafeBrowsingIncidentsSent);

  // Apply the platform data.
  Transaction transaction(this);
  std::optional<base::Value::Dict> value_dict(
      platform_state_store::Load(profile_));
  if (value_dict.has_value()) {
    if (value_dict->empty()) {
      transaction.ClearAll();
    } else if (!incidents_sent_ || *incidents_sent_ != value_dict.value()) {
      transaction.ReplacePrefDict(std::move(value_dict.value()));
    }
  }

  CleanLegacyValues(&transaction);
}

StateStore::~StateStore() {
#if DCHECK_IS_ON()
  DCHECK(!has_transaction_);
#endif
}

bool StateStore::HasBeenReported(IncidentType type,
                                 const std::string& key,
                                 IncidentDigest digest) {
  const base::Value::Dict* type_dict =
      incidents_sent_ ? incidents_sent_->FindDict(
                            base::NumberToString(static_cast<int>(type)))
                      : nullptr;
  if (!type_dict)
    return false;
  const std::string* digest_string = type_dict->FindString(key);
  return (digest_string && *digest_string == base::NumberToString(digest));
}

void StateStore::CleanLegacyValues(Transaction* transaction) {
  static const IncidentType kLegacyTypes[] = {
      IncidentType::OBSOLETE_BLOCKLIST_LOAD,
      IncidentType::OBSOLETE_SUSPICIOUS_MODULE};

  for (IncidentType type : kLegacyTypes)
    transaction->ClearForType(type);
}

}  // namespace safe_browsing

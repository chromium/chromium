// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_STATE_STORE_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_STATE_STORE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/prefs/scoped_user_pref_update.h"

class Profile;

namespace safe_browsing {

enum class IncidentType : int32_t;

// The storage to track which incidents have been reported for a profile. Only
// usable on the UI thread.
class StateStore {
 public:
  using IncidentDigest = uint32_t;

  // The result of state store initialization. Values here are used for UMA so
  // they must not be changed.
  enum InitializationResult {
    PSS_NULL = 0,     // The platform state store was absent or not supported.
    PSS_EMPTY = 1,    // The platform state store was empty and the pref wasn't.
    PSS_DIFFERS = 2,  // The platform state store differed from the preference.
    PSS_MATCHES = 3,  // The platform state store matched the preference.
    NUM_INITIALIZATION_RESULTS = 4,
  };

  // An object through which modifications to a StateStore can be made. Changes
  // are visible to the StateStore immediately and are written to persistent
  // storage when the instance is destroyed (or shortly thereafter). Only one
  // transaction may be live for a given StateStore at a given time. Instances
  // are typically created on the stack for immediate use.
  class Transaction {
   public:
    explicit Transaction(StateStore* store);

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    ~Transaction();

    // Marks the described incident as having been reported.
    void MarkAsReported(IncidentType type,
                        const std::string& key,
                        IncidentDigest digest);

    // Clears the described incident data.
    void Clear(IncidentType type, const std::string& key);

    // Clears all data associated with an incident type.
    void ClearForType(IncidentType type);

    // Clears all data in the store.
    void ClearAll();

   private:
    friend class StateStore;

    // Returns a writable view on the incidents_sent preference. The act of
    // obtaining this view will cause a serialize-and-write operation to be
    // scheduled when the transaction terminates. Use the store's
    // |incidents_sent_| member directly to simply query the preference.
    base::Value::Dict& GetPrefDict();

    // Replaces the contents of the underlying dictionary value.
    void ReplacePrefDict(base::Value::Dict pref_dict);

    // The store corresponding to this transaction.
    raw_ptr<StateStore> store_;

    // A ScopedDictPrefUpdate through which changes to the incidents_sent
    // preference are made.
    std::unique_ptr<ScopedDictPrefUpdate> pref_update_;
  };

  explicit StateStore(Profile* profile);

  StateStore(const StateStore&) = delete;
  StateStore& operator=(const StateStore&) = delete;

  ~StateStore();

  // Returns true if the described incident has already been reported.
  bool HasBeenReported(IncidentType type,
                       const std::string& key,
                       IncidentDigest digest);

 private:
  // Called on load to clear values that are no longer used.
  void CleanLegacyValues(Transaction* transaction);

  // The profile to which this state corresponds.
  raw_ptr<Profile, DanglingUntriaged> profile_;

  // A read-only view on the profile's incidents_sent preference.
  raw_ptr<const base::Value::Dict, DanglingUntriaged> incidents_sent_ = nullptr;

#if DCHECK_IS_ON()
  // True when a Transaction instance is outstanding.
  bool has_transaction_;
#endif
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_STATE_STORE_H_

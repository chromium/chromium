// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_STORAGE_H_
#define CHROME_BROWSER_DIPS_DIPS_STORAGE_H_

#include <map>
#include <string>

#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/dips/dips_state.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace dips {

// Manages the storage of DIPSState values.
//
// This is currently in-memory only. It will be replaced with a SQLite
// implementation soon.
class DIPSStorage {
 public:
  DIPSStorage();
  ~DIPSStorage();

  DIPSState Read(const GURL& url);

  // Returns an opaque value representing the "privacy boundary" that the URL
  // belongs to. Currently returns eTLD+1, but this is an implementation detail
  // and will change (e.g. after adding support for First-Party Sets).
  static std::string GetSite(const GURL& url);

 private:
  friend class DIPSState;
  void Write(const DIPSState& state);

  struct StateValue {
    absl::optional<base::Time> site_storage_time;
    absl::optional<base::Time> user_interaction_time;
  };

  // We don't store DIPSState instances in the map, because we don't want
  // mutations to be persisted until they are flushed, nor do we want to allow
  // copying DIPSState values because that won't be possible once they are
  // backed by SQLite transactions.
  std::map<std::string, StateValue> map_ GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace dips

#endif  // CHROME_BROWSER_DIPS_DIPS_STORAGE_H_

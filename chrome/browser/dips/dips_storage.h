// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_STORAGE_H_
#define CHROME_BROWSER_DIPS_DIPS_STORAGE_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/dips/dips_database.h"
#include "chrome/browser/dips/dips_state.h"

class GURL;

// Manages the storage of DIPSState values.
class DIPSStorage {
 public:
  DIPSStorage();
  ~DIPSStorage();

  void Init(const absl::optional<base::FilePath>& path);

  DIPSState Read(const GURL& url);

  // DIPS Helper Method Impls --------------------------------------------------

  // Record that |url| wrote to storage.
  void RecordStorage(const GURL& url, base::Time time, DIPSCookieMode mode);
  // Record that the user interacted on |url|.
  void RecordInteraction(const GURL& url, base::Time time, DIPSCookieMode mode);

  // Empty method intended for testing use only.
  void DoNothing() {}

  static size_t SetPrepopulateChunkSizeForTesting(size_t size);

  // For each site in |sites|, set the interaction and storage timestamps to
  // |time|. Note this may run asynchronously -- the DB is not guaranteed to be
  // fully prepopulated when this method returns.
  void Prepopulate(base::Time time, std::vector<std::string> sites) {
    PrepopulateChunk(PrepopulateArgs{time, 0, std::move(sites)});
  }

  // Because we keep posting tasks with Prepopulate() with mostly the same
  // arguments (only |offset| changes), group them into a struct that can easily
  // be posted again.
  struct PrepopulateArgs {
    PrepopulateArgs(base::Time time,
                    size_t offset,
                    std::vector<std::string> sites);
    PrepopulateArgs(PrepopulateArgs&&);
    ~PrepopulateArgs();

    base::Time time;
    size_t offset;
    std::vector<std::string> sites;
  };

 private:
  friend class DIPSState;
  DIPSState ReadSite(std::string site);
  void Write(const DIPSState& state);
  // Prepopulate the DB with one chunk of |args.sites|, and schedule another
  // task to continue if more sites remain.
  void PrepopulateChunk(PrepopulateArgs args);

  std::unique_ptr<DIPSDatabase> db_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<DIPSStorage> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DIPS_DIPS_STORAGE_H_

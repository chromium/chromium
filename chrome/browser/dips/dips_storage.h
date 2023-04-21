// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_STORAGE_H_
#define CHROME_BROWSER_DIPS_DIPS_STORAGE_H_

#include <cstddef>
#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/dips/dips_database.h"
#include "chrome/browser/dips/dips_state.h"
#include "chrome/browser/dips/dips_utils.h"
#include "services/network/public/mojom/network_context.mojom.h"

class GURL;

using UrlPredicate = base::RepeatingCallback<bool(const GURL&)>;

// Manages the storage of DIPSState values.
class DIPSStorage {
 public:
  explicit DIPSStorage(const absl::optional<base::FilePath>& path);
  ~DIPSStorage();

  DIPSState Read(const GURL& url);

  void RemoveEvents(base::Time delete_begin,
                    base::Time delete_end,
                    network::mojom::ClearDataFilterPtr filter,
                    const DIPSEventRemovalType type);

  void RemoveRows(const std::vector<std::string>& sites);

  // DIPS Helper Method Impls --------------------------------------------------

  // Record that |url| wrote to storage.
  void RecordStorage(const GURL& url, base::Time time, DIPSCookieMode mode);
  // Record that the user interacted on |url|.
  void RecordInteraction(const GURL& url, base::Time time, DIPSCookieMode mode);
  // Record that |url| redirected the user and whether it was |stateful|,
  // meaning that |url| wrote to storage while redirecting.
  void RecordBounce(const GURL& url, base::Time time, bool stateful);

  // Storage querying Methods --------------------------------------------------

  // Returns the subset of sites in |sites| WITHOUT user interaction recorded.
  std::set<std::string> FilterSitesWithoutInteraction(
      std::set<std::string> sites) const;

  // Returns all sites that did a bounce that aren't protected from DIPS.
  std::vector<std::string> GetSitesThatBounced(
      const base::TimeDelta& grace_period) const;

  // Returns all sites that did a stateful bounce that aren't protected from
  // DIPS.
  std::vector<std::string> GetSitesThatBouncedWithState(
      const base::TimeDelta& grace_period) const;

  // Returns all sites which use storage that aren't protected from DIPS.
  std::vector<std::string> GetSitesThatUsedStorage(
      const base::TimeDelta& grace_period) const;

  // Returns the list of sites that should have their state cleared by DIPS. How
  // these sites are determined is controlled by the value of
  // `dips::kTriggeringAction`. Passing a non-NULL `grace_period` parameter
  // overrides the use of `dips::kGracePeriod` when evaluating sites to clear.
  std::vector<std::string> GetSitesToClear(
      absl::optional<base::TimeDelta> grace_period) const;

  // Utility Methods -----------------------------------------------------------

  static void DeleteDatabaseFiles(base::FilePath path,
                                  base::OnceClosure on_complete);

  static size_t SetPrepopulateChunkSizeForTesting(size_t size);
  void SetClockForTesting(base::Clock* clock) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    db_->SetClockForTesting(clock);
  }

  // Whether the DIPS database has already been prepopulated with
  // SiteEngagement.
  bool IsPrepopulated() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return db_->IsPrepopulated();
  }

  // For each site in |sites|, set the interaction and storage timestamps to
  // |time|. Note this may run asynchronously -- the DB is not guaranteed to be
  // fully prepopulated when this method returns.
  void Prepopulate(base::Time time,
                   std::vector<std::string> sites,
                   base::OnceClosure on_complete) {
    PrepopulateChunk(
        PrepopulateArgs{time, 0, std::move(sites), std::move(on_complete)});
  }

  // Because we keep posting tasks with Prepopulate() with mostly the same
  // arguments (only |offset| changes), group them into a struct that can easily
  // be posted again.
  struct PrepopulateArgs {
    PrepopulateArgs(base::Time time,
                    size_t offset,
                    std::vector<std::string> sites,
                    base::OnceClosure on_complete);
    PrepopulateArgs(PrepopulateArgs&&);
    ~PrepopulateArgs();

    base::Time time;
    size_t offset;
    std::vector<std::string> sites;
    base::OnceClosure on_complete;
  };

 protected:
  void Write(const DIPSState& state);

 private:
  friend class DIPSState;
  DIPSState ReadSite(std::string site);
  // Prepopulate the DB with one chunk of |args.sites|, and schedule another
  // task to continue if more sites remain.
  void PrepopulateChunk(PrepopulateArgs args);

  std::unique_ptr<DIPSDatabase> db_ GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DIPSStorage> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DIPS_DIPS_STORAGE_H_

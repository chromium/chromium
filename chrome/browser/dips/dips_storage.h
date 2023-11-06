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

  absl::optional<PopupsStateValue> ReadPopup(
      const std::string& first_party_site,
      const std::string& tracking_site);

  std::vector<PopupWithTime> ReadRecentPopupsWithInteraction(
      const base::TimeDelta& lookback);

  bool WritePopup(const std::string& first_party_site,
                  const std::string& tracking_site,
                  const uint64_t access_id,
                  const base::Time& popup_time,
                  bool is_current_interaction);

  void RemoveEvents(base::Time delete_begin,
                    base::Time delete_end,
                    network::mojom::ClearDataFilterPtr filter,
                    const DIPSEventRemovalType type);

  // Delete all DB rows for |sites|.
  void RemoveRows(const std::vector<std::string>& sites);
  // Delete all DB rows for |sites| without eligible user interactions.
  void RemoveRowsWithoutInteractionOrWaa(const std::set<std::string>& sites);

  // DIPS Helper Method Impls --------------------------------------------------

  // Record that |url| wrote to storage.
  void RecordStorage(const GURL& url, base::Time time, DIPSCookieMode mode);
  // Record that the user interacted on |url|.
  void RecordInteraction(const GURL& url, base::Time time, DIPSCookieMode mode);
  void RecordWebAuthnAssertion(const GURL& url,
                               base::Time time,
                               DIPSCookieMode mode);
  // Record that |url| redirected the user and whether it was |stateful|,
  // meaning that |url| wrote to storage while redirecting.
  void RecordBounce(const GURL& url, base::Time time, bool stateful);

  // Storage querying Methods --------------------------------------------------

  // Returns the subset of sites in |sites| WITHOUT user interaction or
  // successful web authn assertion recorded.
  std::set<std::string> FilterSitesWithoutInteractionOrWaa(
      std::set<std::string> sites) const;

  // Returns all sites that did a bounce that aren't protected from DIPS.
  std::vector<std::string> GetSitesThatBounced(
      base::TimeDelta grace_period) const;

  // Returns all sites that did a stateful bounce that aren't protected from
  // DIPS.
  std::vector<std::string> GetSitesThatBouncedWithState(
      base::TimeDelta grace_period) const;

  // Returns all sites which use storage that aren't protected from DIPS.
  std::vector<std::string> GetSitesThatUsedStorage(
      base::TimeDelta grace_period) const;

  // Returns the list of sites that should have their state cleared by DIPS. How
  // these sites are determined is controlled by the value of
  // `features::kDIPSTriggeringAction`. Passing a non-NULL `grace_period`
  // parameter overrides the use of `features::kDIPSGracePeriod` when
  // evaluating sites to clear.
  std::vector<std::string> GetSitesToClear(
      absl::optional<base::TimeDelta> grace_period) const;

  // Returns true if `url`'s site has had user interaction since `bound`.
  bool DidSiteHaveInteractionSince(const GURL& url, base::Time bound);

  // Returns the timestamp of the last user interaction time on `url`, or
  // absl::nullopt if there has been no user interaction on `url`.
  absl::optional<base::Time> LastInteractionTime(const GURL& url);

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

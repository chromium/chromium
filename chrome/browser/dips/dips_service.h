// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_SERVICE_H_
#define CHROME_BROWSER_DIPS_DIPS_SERVICE_H_

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/dips/dips_browser_signin_detector.h"
#include "chrome/browser/dips/dips_redirect_info.h"
#include "chrome/browser/dips/dips_storage.h"
#include "chrome/browser/dips/dips_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browsing_data_filter_builder.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace content_settings {
class CookieSettings;
}

namespace signin {
class PersistentRepeatingTimer;
}

class DIPSService : public KeyedService {
 public:
  using RecordBounceCallback = base::RepeatingCallback<void(
      const GURL& url,
      const GURL& initial_url,
      const GURL& final_url,
      base::Time time,
      bool stateful,
      base::RepeatingCallback<void(const GURL&)> content_settings_callback)>;
  using DeletedSitesCallback =
      base::OnceCallback<void(const std::vector<std::string>& sites)>;
  using CheckInteractionCallback = base::OnceCallback<void(bool)>;

  ~DIPSService() override;

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnChainHandled(const DIPSRedirectChainInfoPtr& chain) {}
  };

  static DIPSService* Get(content::BrowserContext* context);

  base::SequenceBound<DIPSStorage>* storage() { return &storage_; }
  void RecordBounceForTesting(
      const GURL& url,
      const GURL& initial_url,
      const GURL& final_url,
      base::Time time,
      bool stateful,
      base::RepeatingCallback<void(const GURL&)> content_settings_callback) {
    RecordBounce(url, initial_url, final_url, time, stateful,
                 content_settings_callback);
  }

  DIPSCookieMode GetCookieMode() const;

  void RemoveEvents(const base::Time& delete_begin,
                    const base::Time& delete_end,
                    network::mojom::ClearDataFilterPtr filter,
                    const DIPSEventRemovalType type);

  // This allows for deletion of state for sites deemed eligible when evaluated
  // with no grace period.
  void DeleteEligibleSitesImmediately(DeletedSitesCallback callback);

  void HandleRedirectChain(
      std::vector<DIPSRedirectInfoPtr> redirects,
      DIPSRedirectChainInfoPtr chain,
      base::RepeatingCallback<void(const GURL&)> content_settings_callback);

  void DidSiteHaveInteractionSince(const GURL& url,
                                   base::Time bound,
                                   CheckInteractionCallback callback) const;

  // This allows unit-testing the metrics emitted by HandleRedirect() without
  // instantiating DIPSService.
  static void HandleRedirectForTesting(const DIPSRedirectInfo& redirect,
                                       const DIPSRedirectChainInfo& chain,
                                       RecordBounceCallback callback) {
    HandleRedirect(redirect, chain, callback,
                   base::BindRepeating([](const GURL& final_url) {}));
  }

  void SetStorageClockForTesting(base::Clock* clock) {
    DCHECK(storage_);
    storage_.AsyncCall(&DIPSStorage::SetClockForTesting).WithArgs(clock);
  }

  void OnTimerFiredForTesting() { OnTimerFired(); }
  void WaitForInitCompleteForTesting() { wait_for_prepopulating_.Run(); }
  void WaitForFileDeletionCompleteForTesting() {
    wait_for_file_deletion_.Run();
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(const Observer* observer);

  void AddOpenSite(const std::string& site) {
    if (open_sites_.contains(site)) {
      open_sites_.at(site)++;
    } else {
      open_sites_.insert({site, 1});
    }
  }

  void RemoveOpenSite(const std::string& site) {
    CHECK(open_sites_.contains(site));
    if (open_sites_.contains(site)) {
      open_sites_.at(site)--;
      if (open_sites_.at(site) == 0) {
        open_sites_.erase(site);
      }
    }
  }

 private:
  // So DIPSServiceFactory::BuildServiceInstanceFor can call the constructor.
  friend class DIPSServiceFactory;
  explicit DIPSService(content::BrowserContext* context);
  std::unique_ptr<signin::PersistentRepeatingTimer> CreateTimer(
      Profile* profile);
  void Shutdown() override;
  bool IsShuttingDown() const { return !cookie_settings_; }

  void GotState(
      std::vector<DIPSRedirectInfoPtr> redirects,
      DIPSRedirectChainInfoPtr chain,
      size_t index,
      base::RepeatingCallback<void(const GURL&)> content_settings_callback,
      const DIPSState url_state);
  void RecordBounce(
      const GURL& url,
      const GURL& initial_url,
      const GURL& final_url,
      base::Time time,
      bool stateful,
      base::RepeatingCallback<void(const GURL&)> content_settings_callback);
  static void HandleRedirect(
      const DIPSRedirectInfo& redirect,
      const DIPSRedirectChainInfo& chain,
      RecordBounceCallback callback,
      base::RepeatingCallback<void(const GURL&)> content_settings_callback);

  scoped_refptr<base::SequencedTaskRunner> CreateTaskRunner();
  void InitializeStorageWithEngagedSites(bool prepopulated);
  // Prepopulates the DIPS database with `sites` having interaction at `time`.
  void InitializeStorage(base::Time time, std::vector<std::string> sites);

  void OnStorageInitialized();
  void OnTimerFired();
  void DeleteDIPSEligibleState(DeletedSitesCallback callback,
                               base::Time deletion_start,
                               std::vector<std::string> sites_to_clear);
  void PostDeletionTaskToUIThread(base::OnceClosure callback,
                                  base::Time deletion_start,
                                  std::vector<std::string> sites_to_clear);
  void RunDeletionTaskOnUIThread(
      std::unique_ptr<content::BrowsingDataFilterBuilder> filter,
      base::OnceClosure callback);

  bool ShouldBlockThirdPartyCookies() const;

  // Checks whether there is an exception allowing |site| to use cookies when
  // embedded by any other site.
  bool Has3PCExceptionAs3P(const std::string& site) const;
  // Checks whether there is an exception allowing all third-parties embedded
  // under |url| to use cookies.
  bool Has3PCExceptionAs1P(const GURL& url) const;

  base::RunLoop wait_for_file_deletion_;
  base::RunLoop wait_for_prepopulating_;
  raw_ptr<content::BrowserContext> browser_context_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  // The return value of CookieSettings::ShouldBlockThirdPartyCookies(), cached
  // by Shutdown() (since we release our CookieSettings but may need the value
  // later).
  absl::optional<bool> cached_should_block_3pcs_;
  // The persisted timer controlling how often incidental state is cleared.
  // This timer is null if the DIPS feature isn't enabled with a valid TimeDelta
  // given for its `timer_delay` parameter.
  // See base/time/time_delta_from_string.h for how that param should be given.
  std::unique_ptr<signin::PersistentRepeatingTimer> repeating_timer_;
  base::SequenceBound<DIPSStorage> storage_;
  base::ObserverList<Observer> observers_;
  absl::optional<DIPSBrowserSigninDetector> dips_browser_signin_detector_;

  std::map<std::string, int> open_sites_;

  base::WeakPtrFactory<DIPSService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DIPS_DIPS_SERVICE_H_

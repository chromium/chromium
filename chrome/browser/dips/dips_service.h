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
#include "base/types/pass_key.h"
#include "chrome/browser/dips/dips_browser_signin_detector.h"
#include "chrome/browser/dips/dips_redirect_info.h"
#include "chrome/browser/dips/dips_storage.h"
#include "chrome/browser/dips/dips_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browsing_data_filter_builder.h"

namespace content {
class BrowserContext;
class DipsDelegate;
}  // namespace content

namespace content_settings {
class CookieSettings;
}

namespace dips {
class PersistentRepeatingTimer;
}

// When DIPS moves to //content, DIPSService will be exposed in the Content API,
// available to embedders such as Chrome.
class DIPSService {
 public:
  using DeletedSitesCallback =
      base::OnceCallback<void(const std::vector<std::string>& sites)>;
  using CheckInteractionCallback = base::OnceCallback<void(bool)>;

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnChainHandled(const DIPSRedirectChainInfoPtr& chain) = 0;
  };

  static DIPSService* Get(content::BrowserContext* context);

  virtual void RecordBrowserSignIn(std::string_view domain) = 0;

  virtual void DeleteEligibleSitesImmediately(
      DeletedSitesCallback callback) = 0;

  virtual void RecordInteractionForTesting(const GURL& url) = 0;

  virtual void DidSiteHaveInteractionSince(
      const GURL& url,
      base::Time bound,
      CheckInteractionCallback callback) const = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(const Observer* observer) = 0;
};

// When DIPS moves to //content, DIPSServiceImpl will *not* be exposed in the
// Content API. Only other code in //content (such as the DIPS implementation)
// will be allowed to access it.
class DIPSServiceImpl : public DIPSService, KeyedService {
 public:
  using RecordBounceCallback = base::RepeatingCallback<void(
      const GURL& url,
      const GURL& initial_url,
      const GURL& final_url,
      base::Time time,
      bool stateful,
      base::RepeatingCallback<void(const GURL&)> content_settings_callback)>;

  ~DIPSServiceImpl() override;

  static DIPSServiceImpl* Get(content::BrowserContext* context);

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
  void DeleteEligibleSitesImmediately(DeletedSitesCallback callback) override;

  void HandleRedirectChain(
      std::vector<DIPSRedirectInfoPtr> redirects,
      DIPSRedirectChainInfoPtr chain,
      base::RepeatingCallback<void(const GURL&)> content_settings_callback);

  void RecordInteractionForTesting(const GURL& url) override;

  void DidSiteHaveInteractionSince(
      const GURL& url,
      base::Time bound,
      CheckInteractionCallback callback) const override;

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
  void WaitForFileDeletionCompleteForTesting() {
    wait_for_file_deletion_.Run();
  }

  void AddObserver(Observer* observer) override;
  void RemoveObserver(const Observer* observer) override;

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
  explicit DIPSServiceImpl(content::BrowserContext* context);
  std::unique_ptr<dips::PersistentRepeatingTimer> CreateTimer();
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

  void OnTimerFired();
  void DeleteDIPSEligibleState(DeletedSitesCallback callback,
                               std::vector<std::string> sites_to_clear);
  void RunDeletionTaskOnUIThread(std::vector<std::string> sites_to_clear,
                                 base::OnceClosure callback);

  // DIPSService overrides:
  void RecordBrowserSignIn(std::string_view domain) override;

  // Checks whether |third_party_url| is allowed to use third-party cookies when
  // embedded under |first_party_url|. Factors the following into account:
  // - Global 3PC setting
  // - Exceptions to allow 3PC for all sites under |first_party_url|
  // - Exceptions to block 3PC for all sites under |first_party url|
  // - Exceptions to allow 3PC for |third_party_url| when embedded by any other
  // site
  // - Granular exceptions to allow 3PC for |third_party_url| when embedded
  // under |first_party_url|
  bool Are3PCAllowed(const GURL& first_party_url,
                     const GURL& third_party_url) const;

  base::RunLoop wait_for_file_deletion_;
  raw_ptr<content::BrowserContext> browser_context_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  // The persisted timer controlling how often incidental state is cleared.
  // This timer is null if the DIPS feature isn't enabled with a valid TimeDelta
  // given for its `timer_delay` parameter.
  // See base/time/time_delta_from_string.h for how that param should be given.
  std::unique_ptr<dips::PersistentRepeatingTimer> repeating_timer_;
  base::SequenceBound<DIPSStorage> storage_;
  base::ObserverList<Observer> observers_;
  std::optional<DIPSBrowserSigninDetector> dips_browser_signin_detector_;
  std::unique_ptr<content::DipsDelegate> dips_delegate_;

  std::map<std::string, int> open_sites_;

  base::WeakPtrFactory<DIPSServiceImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DIPS_DIPS_SERVICE_H_

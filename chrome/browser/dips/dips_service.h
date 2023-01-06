// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_SERVICE_H_
#define CHROME_BROWSER_DIPS_DIPS_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/dips/dips_redirect_info.h"
#include "chrome/browser/dips/dips_storage.h"
#include "chrome/browser/dips/dips_utils.h"
#include "components/keyed_service/core/keyed_service.h"

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
  using RecordBounceCallback = base::RepeatingCallback<
      void(const GURL& url, base::Time time, bool stateful)>;

  ~DIPSService() override;

  static DIPSService* Get(content::BrowserContext* context);

  base::SequenceBound<DIPSStorage>* storage() { return &storage_; }

  DIPSCookieMode GetCookieMode() const;

  void RemoveEvents(const base::Time& delete_begin,
                    const base::Time& delete_end,
                    network::mojom::ClearDataFilterPtr filter,
                    const DIPSEventRemovalType type);

  void HandleRedirectChain(std::vector<DIPSRedirectInfoPtr> redirects,
                           DIPSRedirectChainInfoPtr chain);

  // This allows unit-testing the metrics emitted by HandleRedirect() without
  // instantiating DIPSService.
  static void HandleRedirectForTesting(const DIPSRedirectInfo& redirect,
                                       const DIPSRedirectChainInfo& chain,
                                       RecordBounceCallback callback) {
    HandleRedirect(redirect, chain, callback);
  }

 private:
  // So DIPSServiceFactory::BuildServiceInstanceFor can call the constructor.
  friend class DIPSServiceFactory;
  explicit DIPSService(content::BrowserContext* context);
  std::unique_ptr<signin::PersistentRepeatingTimer> CreateTimer(
      Profile* profile);
  void Shutdown() override;

  void GotState(std::vector<DIPSRedirectInfoPtr> redirects,
                DIPSRedirectChainInfoPtr chain,
                size_t index,
                const DIPSState url_state);
  void RecordBounce(const GURL& url, base::Time time, bool stateful);
  static void HandleRedirect(const DIPSRedirectInfo& redirect,
                             const DIPSRedirectChainInfo& chain,
                             RecordBounceCallback callback);

  scoped_refptr<base::SequencedTaskRunner> CreateTaskRunner();
  void InitializeStorageWithEngagedSites();
  void InitializeStorage(base::Time time, std::vector<std::string> sites);

  void OnTimerFired();

  raw_ptr<content::BrowserContext> browser_context_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  // The persisted timer controlling how often incidental state is cleared.
  // This timer is null if the DIPS feature isn't enabled with a valid TimeDelta
  // given for its `timer_delay` parameter.
  // See base/time/time_delta_from_string.h for how that param should be given.
  std::unique_ptr<signin::PersistentRepeatingTimer> repeating_timer_;
  base::SequenceBound<DIPSStorage> storage_;
  base::WeakPtrFactory<DIPSService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DIPS_DIPS_SERVICE_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_service.h"

#include <set>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/dips/dips_browser_signin_detector.h"
#include "chrome/browser/dips/dips_redirect_info.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/dips/dips_storage.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/signin/public/base/persistent_repeating_timer.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/site_engagement/core/mojom/site_engagement_details.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/common/content_features.h"
#include "content/public/common/dips_utils.h"
#include "net/cookies/cookie_setting_override.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"

namespace {

// Controls whether UKM metrics are collected for DIPS.
BASE_FEATURE(kDipsUkm, "DipsUkm", base::FEATURE_ENABLED_BY_DEFAULT);

std::vector<std::string> GetEngagedSitesInBackground(
    base::Time now,
    scoped_refptr<HostContentSettingsMap> map) {
  std::set<std::string> unique_sites;
  auto details =
      site_engagement::SiteEngagementService::GetAllDetailsInBackground(now,
                                                                        map);
  for (const site_engagement::mojom::SiteEngagementDetails& detail : details) {
    if (!detail.origin.SchemeIsHTTPOrHTTPS()) {
      continue;
    }
    if (!site_engagement::SiteEngagementService::IsEngagementAtLeast(
            detail.total_score, blink::mojom::EngagementLevel::MINIMAL)) {
      continue;
    }
    unique_sites.insert(GetSiteForDIPS(detail.origin));
  }

  return std::vector(unique_sites.begin(), unique_sites.end());
}

RedirectCategory ClassifyRedirect(SiteDataAccessType access,
                                  bool has_interaction) {
  switch (access) {
    case SiteDataAccessType::kUnknown:
      return has_interaction ? RedirectCategory::kUnknownCookies_HasEngagement
                             : RedirectCategory::kUnknownCookies_NoEngagement;
    case SiteDataAccessType::kNone:
      return has_interaction ? RedirectCategory::kNoCookies_HasEngagement
                             : RedirectCategory::kNoCookies_NoEngagement;
    case SiteDataAccessType::kRead:
      return has_interaction ? RedirectCategory::kReadCookies_HasEngagement
                             : RedirectCategory::kReadCookies_NoEngagement;
    case SiteDataAccessType::kWrite:
      return has_interaction ? RedirectCategory::kWriteCookies_HasEngagement
                             : RedirectCategory::kWriteCookies_NoEngagement;
    case SiteDataAccessType::kReadWrite:
      return has_interaction ? RedirectCategory::kReadWriteCookies_HasEngagement
                             : RedirectCategory::kReadWriteCookies_NoEngagement;
  }
}

inline void UmaHistogramBounceCategory(RedirectCategory category,
                                       DIPSCookieMode mode,
                                       DIPSRedirectType type) {
  const std::string histogram_name =
      base::StrCat({"Privacy.DIPS.BounceCategory", GetHistogramPiece(type),
                    GetHistogramSuffix(mode)});
  base::UmaHistogramEnumeration(histogram_name, category);
}

inline void UmaHistogramDeletionLatency(base::Time deletion_start) {
  base::UmaHistogramLongTimes100("Privacy.DIPS.DeletionLatency2",
                                 base::Time::Now() - deletion_start);
}

inline void UmaHistogramClearedSitesCount(DIPSCookieMode mode, int size) {
  base::UmaHistogramCounts1000(base::StrCat({"Privacy.DIPS.ClearedSitesCount",
                                             GetHistogramSuffix(mode)}),
                               size);
}

inline void UmaHistogramDeletion(DIPSCookieMode mode,
                                 DIPSDeletionAction action) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Privacy.DIPS.Deletion", GetHistogramSuffix(mode)}),
      action);
}

void OnDeletionFinished(base::OnceClosure finished_callback,
                        base::Time deletion_start) {
  UmaHistogramDeletionLatency(deletion_start);
  std::move(finished_callback).Run();
}

class StateClearer : public content::BrowsingDataRemover::Observer {
 public:
  StateClearer(const StateClearer&) = delete;
  StateClearer& operator=(const StateClearer&) = delete;

  ~StateClearer() override { remover_->RemoveObserver(this); }

  // Clears state for the sites specified by 'filter'. Runs |callback| once
  // clearing is complete.
  //
  // NOTE: This deletion task removing rows for `sites_to_clear` from the
  // DIPSStorage backend relies on the assumption that rows flagged as DIPS
  // eligible don't have user interaction time values. So even though 'remover'
  // will only clear the storage timestamps, that's sufficient to delete the
  // entire row.
  static void DeleteState(
      content::BrowsingDataRemover* remover,
      std::unique_ptr<content::BrowsingDataFilterBuilder> filter,
      base::OnceClosure callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // StateClearer manages its own lifetime and deletes itself when finished.
    base::Time deletion_start = base::Time::Now();
    auto* state_clearer = new StateClearer(
        remover, base::BindOnce(&OnDeletionFinished, std::move(callback),
                                deletion_start));

    remover->AddObserver(state_clearer);
    remover->RemoveWithFilterAndReply(
        base::Time::Min(), base::Time::Max(),
        chrome_browsing_data_remover::FILTERABLE_DATA_TYPES |
            content::BrowsingDataRemover::DATA_TYPE_AVOID_CLOSING_CONNECTIONS,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
            content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
        std::move(filter), state_clearer);
  }

 private:
  StateClearer(content::BrowsingDataRemover* remover,
               base::OnceClosure callback)
      : remover_(remover), callback_(std::move(callback)) {}

  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override {
    std::move(callback_).Run();
    delete this;  // Matches the new in DeleteState()
  }

  raw_ptr<content::BrowsingDataRemover> remover_;
  base::OnceClosure callback_;
};

}  // namespace

DIPSService::DIPSService(content::BrowserContext* context)
    : browser_context_(context),
      cookie_settings_(CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(context))),
      repeating_timer_(CreateTimer(Profile::FromBrowserContext(context))) {
  DCHECK(base::FeatureList::IsEnabled(features::kDIPS));
  absl::optional<base::FilePath> path_to_use;
  base::FilePath dips_path = GetDIPSFilePath(browser_context_);

  if (browser_context_->IsOffTheRecord()) {
    // OTR profiles should have no existing DIPS database file to be cleaned up.
    // In fact, attempting to delete one at the path associated with the OTR
    // profile would delete the DIPS database for the underlying regular
    // profile.
    wait_for_file_deletion_.Quit();
  } else {
    if (features::kDIPSPersistedDatabaseEnabled.Get()) {
      path_to_use = dips_path;
      // Existing database files won't be deleted, so quit the
      // `wait_for_file_deletion_` RunLoop.
      wait_for_file_deletion_.Quit();
    } else {
      // If opening in-memory, delete any database files that may exist.
      DIPSStorage::DeleteDatabaseFiles(dips_path,
                                       wait_for_file_deletion_.QuitClosure());
    }
  }

  storage_ = base::SequenceBound<DIPSStorage>(CreateTaskRunner(), path_to_use);

  storage_.AsyncCall(&DIPSStorage::IsPrepopulated)
      .Then(base::BindOnce(&DIPSService::InitializeStorageWithEngagedSites,
                           weak_factory_.GetWeakPtr()));
  if (repeating_timer_) {
    repeating_timer_->Start();
  }

  if (auto* identity_manager = IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(context))) {
    dips_browser_signin_detector_.emplace(this, identity_manager);
  }
}

std::unique_ptr<signin::PersistentRepeatingTimer> DIPSService::CreateTimer(
    Profile* profile) {
  DCHECK(profile);
  // base::Unretained(this) is safe here since the timer that is created has the
  // same lifetime as this service.
  return std::make_unique<signin::PersistentRepeatingTimer>(
      profile->GetPrefs(), prefs::kDIPSTimerLastUpdate,
      features::kDIPSTimerDelay.Get(),
      base::BindRepeating(&DIPSService::OnTimerFired, base::Unretained(this)));
}

DIPSService::~DIPSService() = default;

/* static */
DIPSService* DIPSService::Get(content::BrowserContext* context) {
  return DIPSServiceFactory::GetForBrowserContext(context);
}

void DIPSService::Shutdown() {
  cookie_settings_.reset();
}

scoped_refptr<base::SequencedTaskRunner> DIPSService::CreateTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::ThreadPolicy::PREFER_BACKGROUND});
}

bool DIPSService::Are3PCAllowed(const GURL& first_party_url,
                                const GURL& third_party_url) const {
  DCHECK(!IsShuttingDown());

  return cookie_settings_->IsFullCookieAccessAllowed(
      third_party_url, net::SiteForCookies::FromUrl(first_party_url),
      url::Origin::Create(first_party_url),
      net::CookieSettingOverrides(
          {net::CookieSettingOverride::kStorageAccessGrantEligible,
           net::CookieSettingOverride::kTopLevelStorageAccessGrantEligible}));
}

DIPSCookieMode DIPSService::GetCookieMode() const {
  return GetDIPSCookieMode(browser_context_->IsOffTheRecord());
}

void DIPSService::RemoveEvents(const base::Time& delete_begin,
                               const base::Time& delete_end,
                               network::mojom::ClearDataFilterPtr filter,
                               DIPSEventRemovalType type) {
  // Storage init should be finished by now, so no need to delay until then.
  storage_.AsyncCall(&DIPSStorage::RemoveEvents)
      .WithArgs(delete_begin, delete_end, std::move(filter), type);
}

void DIPSService::InitializeStorageWithEngagedSites(bool prepopulated) {
  if (prepopulated) {
    wait_for_prepopulating_.Quit();
    return;
  }
  base::Time now = base::Time::Now();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &GetEngagedSitesInBackground, now,
          base::WrapRefCounted(
              HostContentSettingsMapFactory::GetForProfile(browser_context_))),
      base::BindOnce(&DIPSService::InitializeStorage,
                     weak_factory_.GetWeakPtr(), now));
}

void DIPSService::InitializeStorage(base::Time time,
                                    std::vector<std::string> sites) {
  storage_.AsyncCall(&DIPSStorage::Prepopulate)
      .WithArgs(time, sites, wait_for_prepopulating_.QuitClosure());
}

void DIPSService::HandleRedirectChain(
    std::vector<DIPSRedirectInfoPtr> redirects,
    DIPSRedirectChainInfoPtr chain,
    base::RepeatingCallback<void(const GURL&)> content_settings_callback) {
  if (redirects.empty()) {
    DCHECK(!chain->is_partial_chain);
    for (auto& observer : observers_) {
      observer.OnChainHandled(chain);
    }
    return;
  }

  chain->cookie_mode = GetCookieMode();
  // Copy the URL out before |redirects| is moved, to avoid use-after-move.
  GURL url = redirects[0]->url;
  storage_.AsyncCall(&DIPSStorage::Read)
      .WithArgs(url)
      .Then(base::BindOnce(&DIPSService::GotState, weak_factory_.GetWeakPtr(),
                           std::move(redirects), std::move(chain), 0,
                           content_settings_callback));
}

void DIPSService::DidSiteHaveInteractionSince(
    const GURL& url,
    base::Time bound,
    CheckInteractionCallback callback) const {
  storage_.AsyncCall(&DIPSStorage::DidSiteHaveInteractionSince)
      .WithArgs(url, bound)
      .Then(std::move(callback));
}

void DIPSService::GotState(
    std::vector<DIPSRedirectInfoPtr> redirects,
    DIPSRedirectChainInfoPtr chain,
    size_t index,
    base::RepeatingCallback<void(const GURL&)> content_settings_callback,
    const DIPSState url_state) {
  DCHECK_LT(index, redirects.size());

  DIPSRedirectInfo* redirect = redirects[index].get();
  // If there's any user interaction recorded in the DIPS DB, that's engagement.
  redirect->has_interaction = url_state.user_interaction_times().has_value();
  HandleRedirect(
      *redirect, *chain,
      base::BindRepeating(&DIPSService::RecordBounce, base::Unretained(this)),
      content_settings_callback);

  if (index + 1 >= redirects.size()) {
    // All redirects handled.
    if (!chain->is_partial_chain) {
      for (auto& observer : observers_) {
        observer.OnChainHandled(chain);
      }
    }
    return;
  }

  // Copy the URL out before `redirects` is moved, to avoid use-after-move.
  GURL url = redirects[index + 1]->url;
  storage_.AsyncCall(&DIPSStorage::Read)
      .WithArgs(url)
      .Then(base::BindOnce(&DIPSService::GotState, weak_factory_.GetWeakPtr(),
                           std::move(redirects), std::move(chain), index + 1,
                           content_settings_callback));
}

void DIPSService::RecordBounce(
    const GURL& url,
    const GURL& initial_url,
    const GURL& final_url,
    base::Time time,
    bool stateful,
    base::RepeatingCallback<void(const GURL&)> content_settings_callback) {
  // If the bounced URL has a 3PC exception when embedded under the initial or
  // final URL in the redirect,then clear the tracking site from the DIPS DB, to
  // avoid deleting its storage. The exception overrides any bounces from
  // non-excepted sites.
  if (Are3PCAllowed(initial_url, url) || Are3PCAllowed(final_url, url)) {
    // These records indicate sites that could've had their state deleted
    // provided their grace period expired. But are at the moment excepted
    // following `Are3PCAllowed()` of either `initial_url` or `final_url`.
    bool would_be_cleared = false;
    switch (features::kDIPSTriggeringAction.Get()) {
      case content::DIPSTriggeringAction::kNone: {
        would_be_cleared = false;
        break;
      }
      case content::DIPSTriggeringAction::kStorage: {
        would_be_cleared = false;
        break;
      }
      case content::DIPSTriggeringAction::kBounce: {
        would_be_cleared = true;
        break;
      }
      case content::DIPSTriggeringAction::kStatefulBounce: {
        would_be_cleared = stateful;
        break;
      }
    }
    if (would_be_cleared) {
      // TODO(crbug.com/1447035): Investigate and fix the presence of empty
      // site(s) in the `site_to_clear` list. Once this is fixed remove this
      // escape.
      if (url.is_empty()) {
        UmaHistogramDeletion(GetCookieMode(), DIPSDeletionAction::kIgnored);
      } else {
        UmaHistogramDeletion(GetCookieMode(), DIPSDeletionAction::kExcepted);
      }
    }

    const std::set<std::string> site_to_clear{GetSiteForDIPS(url)};
    // Don't clear the row if the tracker has interaction history, since we
    // should preserve that context for future bounces.
    storage_.AsyncCall(&DIPSStorage::RemoveRowsWithoutInteractionOrWaa)
        .WithArgs(site_to_clear);

    return;
  }

  // If the bounce is stateful and not excepted by cookie settings, increment
  // the bounce counter in PageSpecificContentSettings.
  if (stateful) {
    content_settings_callback.Run(final_url);
  }

  storage_.AsyncCall(&DIPSStorage::RecordBounce).WithArgs(url, time, stateful);
}

/*static*/
void DIPSService::HandleRedirect(
    const DIPSRedirectInfo& redirect,
    const DIPSRedirectChainInfo& chain,
    RecordBounceCallback record_bounce,
    base::RepeatingCallback<void(const GURL&)> content_settings_callback) {
  const std::string site = GetSiteForDIPS(redirect.url);
  bool initial_site_same = (site == chain.initial_site);
  bool final_site_same = (site == chain.final_site);
  DCHECK_LT(redirect.chain_index, chain.length);

  if (base::FeatureList::IsEnabled(kDipsUkm)) {
    ukm::builders::DIPS_Redirect(redirect.source_id)
        .SetSiteEngagementLevel(redirect.has_interaction.value() ? 1 : 0)
        .SetRedirectType(static_cast<int64_t>(redirect.redirect_type))
        .SetCookieAccessType(static_cast<int64_t>(redirect.access_type))
        .SetRedirectAndInitialSiteSame(initial_site_same)
        .SetRedirectAndFinalSiteSame(final_site_same)
        .SetInitialAndFinalSitesSame(chain.initial_and_final_sites_same)
        .SetRedirectChainIndex(redirect.chain_index)
        .SetRedirectChainLength(chain.length)
        .SetIsPartialRedirectChain(chain.is_partial_chain)
        .SetClientBounceDelay(
            BucketizeBounceDelay(redirect.client_bounce_delay))
        .SetHasStickyActivation(redirect.has_sticky_activation)
        .SetWebAuthnAssertionRequestSucceeded(
            redirect.web_authn_assertion_request_succeeded)
        .Record(ukm::UkmRecorder::Get());
  }

  if (initial_site_same || final_site_same) {
    // Don't record UMA metrics for same-site redirects.
    return;
  }

  // Record this bounce in the DIPS database.
  if (redirect.access_type != SiteDataAccessType::kUnknown) {
    record_bounce.Run(
        redirect.url, chain.initial_url, chain.final_url, redirect.time,
        /*stateful=*/redirect.access_type > SiteDataAccessType::kRead,
        content_settings_callback);
  }

  RedirectCategory category =
      ClassifyRedirect(redirect.access_type, redirect.has_interaction.value());
  UmaHistogramBounceCategory(category, chain.cookie_mode.value(),
                             redirect.redirect_type);
}

void DIPSService::OnTimerFired() {
  // Storage init should be finished by now, so no need to delay until then.
  storage_.AsyncCall(&DIPSStorage::GetSitesToClear)
      .WithArgs(absl::nullopt)
      .Then(base::BindOnce(&DIPSService::DeleteDIPSEligibleState,
                           weak_factory_.GetWeakPtr(), base::DoNothing()));
}

void DIPSService::DeleteEligibleSitesImmediately(
    DeletedSitesCallback callback) {
  // Storage init should be finished by now, so no need to delay until then.
  storage_.AsyncCall(&DIPSStorage::GetSitesToClear)
      .WithArgs(base::Seconds(0))
      .Then(base::BindOnce(&DIPSService::DeleteDIPSEligibleState,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void DIPSService::DeleteDIPSEligibleState(
    DeletedSitesCallback callback,
    std::vector<std::string> sites_to_clear) {
  // Do not clear sites from currently open tabs.
  for (const std::pair<std::string, int> site_ctr : open_sites_) {
    CHECK(site_ctr.second > 0);
    sites_to_clear.erase(std::remove(sites_to_clear.begin(),
                                     sites_to_clear.end(), site_ctr.first),
                         sites_to_clear.end());
  }

  if (sites_to_clear.empty()) {
    UmaHistogramClearedSitesCount(GetCookieMode(), sites_to_clear.size());
    std::move(callback).Run(std::vector<std::string>());
    return;
  }

  if (IsShuttingDown()) {
    return;
  }

  UmaHistogramClearedSitesCount(GetCookieMode(), sites_to_clear.size());

  for (const auto& site : sites_to_clear) {
    // TODO(crbug.com/1447035): Investigate and fix the presence of empty
    // site(s) in the `site_to_clear` list. Once this is fixed remove this loop
    // escape.
    if (site.empty()) {
      continue;
    }
    const ukm::SourceId source_id = ukm::UkmRecorder::GetSourceIdForDipsSite(
        base::PassKey<DIPSService>(), site);
    ukm::builders::DIPS_Deletion(source_id)
        // These settings are checked at bounce time, before logging the bounce.
        // At this time, we guarantee that 3PC are blocked and this site is not
        // excepted (provided the user hasn't changed their settings in the
        // meantime).
        .SetShouldBlockThirdPartyCookies(true)
        .SetHasCookieException(false)
        .SetIsDeletionEnabled(features::kDIPSDeletionEnabled.Get())
        .Record(ukm::UkmRecorder::Get());
  }

  if (features::kDIPSDeletionEnabled.Get()) {
    std::vector<std::string> filtered_sites_to_clear;

    for (const auto& site : sites_to_clear) {
      // TODO(crbug.com/1447035): Investigate and fix the presence of empty
      // site(s) in the `site_to_clear` list. Once this is fixed remove this
      // loop escape.
      if (site.empty()) {
        UmaHistogramDeletion(GetCookieMode(), DIPSDeletionAction::kIgnored);
        continue;
      }
      UmaHistogramDeletion(GetCookieMode(), DIPSDeletionAction::kEnforced);
      filtered_sites_to_clear.push_back(site);
    }

    base::OnceClosure finish_callback = base::BindOnce(
        std::move(callback), std::vector<std::string>(filtered_sites_to_clear));
    if (filtered_sites_to_clear.empty()) {
      std::move(finish_callback).Run();
      return;
    }

    // Perform state deletion on the filtered list of sites.
    PostDeletionTaskToUIThread(std::move(finish_callback),
                               std::move(filtered_sites_to_clear));
  } else {
    for (auto it = sites_to_clear.begin(); it != sites_to_clear.end(); it++) {
      // TODO(crbug.com/1447035): Investigate and fix the presence of empty
      // site(s) in the `site_to_clear` list. Once this is fixed remove this
      // loop escape.
      if (it->empty()) {
        UmaHistogramDeletion(GetCookieMode(), DIPSDeletionAction::kIgnored);
        continue;
      }
      UmaHistogramDeletion(GetCookieMode(), DIPSDeletionAction::kDisallowed);
    }

    base::Time deletion_start = base::Time::Now();
    // Storage init should be finished by now, so no need to delay until then.
    storage_.AsyncCall(&DIPSStorage::RemoveRows)
        .WithArgs(std::move(sites_to_clear))
        .Then(base::BindOnce(
            &OnDeletionFinished,
            base::BindOnce(std::move(callback), std::vector<std::string>()),
            deletion_start));
  }
}

void DIPSService::PostDeletionTaskToUIThread(base::OnceClosure callback,
                                             std::vector<std::string> sites) {
  std::unique_ptr<content::BrowsingDataFilterBuilder> filter =
      content::BrowsingDataFilterBuilder::Create(
          content::BrowsingDataFilterBuilder::Mode::kDelete);
  for (const auto& site : sites) {
    filter->AddRegisterableDomain(site);
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&DIPSService::RunDeletionTaskOnUIThread,
                                weak_factory_.GetWeakPtr(), std::move(filter),
                                std::move(callback)));
}

void DIPSService::RunDeletionTaskOnUIThread(
    std::unique_ptr<content::BrowsingDataFilterBuilder> filter,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  StateClearer::DeleteState(browser_context_->GetBrowsingDataRemover(),
                            std::move(filter), std::move(callback));
}

void DIPSService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DIPSService::RemoveObserver(const Observer* observer) {
  observers_.RemoveObserver(observer);
}

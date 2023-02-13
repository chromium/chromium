// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_service.h"

#include <set>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/dips/dips_features.h"
#include "chrome/browser/dips/dips_redirect_info.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/dips/dips_storage.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/signin/public/base/persistent_repeating_timer.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/site_engagement/core/mojom/site_engagement_details.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

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

RedirectCategory ClassifyRedirect(CookieAccessType access,
                                  bool has_interaction) {
  switch (access) {
    case CookieAccessType::kUnknown:
      return has_interaction ? RedirectCategory::kUnknownCookies_HasEngagement
                             : RedirectCategory::kUnknownCookies_NoEngagement;
    case CookieAccessType::kNone:
      return has_interaction ? RedirectCategory::kNoCookies_HasEngagement
                             : RedirectCategory::kNoCookies_NoEngagement;
    case CookieAccessType::kRead:
      return has_interaction ? RedirectCategory::kReadCookies_HasEngagement
                             : RedirectCategory::kReadCookies_NoEngagement;
    case CookieAccessType::kWrite:
      return has_interaction ? RedirectCategory::kWriteCookies_HasEngagement
                             : RedirectCategory::kWriteCookies_NoEngagement;
    case CookieAccessType::kReadWrite:
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
  base::UmaHistogramLongTimes100("Privacy.DIPS.DeletionLatency",
                                 base::Time::Now() - deletion_start);
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
    auto* state_clearer = new StateClearer(remover, std::move(callback));

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
  DCHECK(base::FeatureList::IsEnabled(dips::kFeature));
  absl::optional<base::FilePath> path;

  if (dips::kPersistedDatabaseEnabled.Get() &&
      !browser_context_->IsOffTheRecord()) {
    path = browser_context_->GetPath().Append(kDIPSFilename);
  }
  storage_ = base::SequenceBound<DIPSStorage>(CreateTaskRunner(), path);

  // TODO: Prevent use of the DB until prepopulation starts.
  InitializeStorageWithEngagedSites();
  if (repeating_timer_) {
    repeating_timer_->Start();
  }
}

std::unique_ptr<signin::PersistentRepeatingTimer> DIPSService::CreateTimer(
    Profile* profile) {
  DCHECK(profile);
  // base::Unretained(this) is safe here since the timer that is created has the
  // same lifetime as this service.
  return std::make_unique<signin::PersistentRepeatingTimer>(
      profile->GetPrefs(), prefs::kDIPSTimerLastUpdate, dips::kTimerDelay.Get(),
      base::BindRepeating(&DIPSService::OnTimerFired, base::Unretained(this)));
}

DIPSService::~DIPSService() = default;

/* static */
DIPSService* DIPSService::Get(content::BrowserContext* context) {
  return DIPSServiceFactory::GetForBrowserContext(context);
}

void DIPSService::Shutdown() {
  cached_should_block_3pcs_ = cookie_settings_->ShouldBlockThirdPartyCookies();
  cookie_settings_.reset();
}

scoped_refptr<base::SequencedTaskRunner> DIPSService::CreateTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::ThreadPolicy::PREFER_BACKGROUND});
}

bool DIPSService::ShouldBlockThirdPartyCookies() const {
  if (IsShuttingDown()) {
    return cached_should_block_3pcs_.value();
  }

  return cookie_settings_->ShouldBlockThirdPartyCookies();
}

bool DIPSService::HasCookieException(const std::string& site) const {
  DCHECK(!IsShuttingDown());
  GURL url("https://" + site);

  // Checks whether there is an exception allowing all third-parties embedded
  // under |site| to use cookies.
  if (cookie_settings_->IsFullCookieAccessAllowed(
          GURL(), net::SiteForCookies::FromUrl(url), url::Origin::Create(url),
          net::CookieSettingOverrides(),
          content_settings::CookieSettingsBase::QueryReason::kCookies)) {
    return true;
  }

  // Checks whether there is an exception allowing |site| to use cookies when
  // embedded by any other site.
  if (cookie_settings_->IsFullCookieAccessAllowed(
          url, net::SiteForCookies(), absl::nullopt,
          net::CookieSettingOverrides(),
          content_settings::CookieSettingsBase::QueryReason::kCookies)) {
    return true;
  }

  return false;
}

DIPSCookieMode DIPSService::GetCookieMode() const {
  return GetDIPSCookieMode(browser_context_->IsOffTheRecord(),
                           ShouldBlockThirdPartyCookies());
}

void DIPSService::RemoveEvents(const base::Time& delete_begin,
                               const base::Time& delete_end,
                               network::mojom::ClearDataFilterPtr filter,
                               DIPSEventRemovalType type) {
  storage_.AsyncCall(&DIPSStorage::RemoveEvents)
      .WithArgs(delete_begin, delete_end, std::move(filter), type);
}

void DIPSService::InitializeStorageWithEngagedSites() {
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
  storage_.AsyncCall(&DIPSStorage::Prepopulate).WithArgs(time, sites);
}

void DIPSService::HandleRedirectChain(
    std::vector<DIPSRedirectInfoPtr> redirects,
    DIPSRedirectChainInfoPtr chain) {
  if (redirects.empty()) {
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
                           std::move(redirects), std::move(chain), 0));
}

void DIPSService::GotState(std::vector<DIPSRedirectInfoPtr> redirects,
                           DIPSRedirectChainInfoPtr chain,
                           size_t index,
                           const DIPSState url_state) {
  DCHECK_LT(index, redirects.size());

  DIPSRedirectInfo* redirect = redirects[index].get();
  // If there's any user interaction recorded in the DIPS DB, that's engagement.
  redirect->has_interaction = url_state.user_interaction_times().has_value();
  HandleRedirect(
      *redirect, *chain,
      base::BindRepeating(&DIPSService::RecordBounce, base::Unretained(this)));

  if (index + 1 >= redirects.size()) {
    // All redirects handled.
    for (auto& observer : observers_) {
      observer.OnChainHandled(chain);
    }
    return;
  }

  // Copy the URL out before `redirects` is moved, to avoid use-after-move.
  GURL url = redirects[index + 1]->url;
  storage_.AsyncCall(&DIPSStorage::Read)
      .WithArgs(url)
      .Then(base::BindOnce(&DIPSService::GotState, weak_factory_.GetWeakPtr(),
                           std::move(redirects), std::move(chain), index + 1));
}

void DIPSService::RecordBounce(const GURL& url,
                               base::Time time,
                               bool stateful) {
  storage_.AsyncCall(&DIPSStorage::RecordBounce).WithArgs(url, time, stateful);
}

/*static*/
void DIPSService::HandleRedirect(const DIPSRedirectInfo& redirect,
                                 const DIPSRedirectChainInfo& chain,
                                 RecordBounceCallback record_bounce) {
  const std::string site = GetSiteForDIPS(redirect.url);
  bool initial_site_same = (site == chain.initial_site);
  bool final_site_same = (site == chain.final_site);
  DCHECK_LE(0, redirect.index);
  DCHECK_LT(redirect.index, chain.length);

  if (base::FeatureList::IsEnabled(kDipsUkm)) {
    ukm::builders::DIPS_Redirect(redirect.source_id)
        .SetSiteEngagementLevel(redirect.has_interaction.value() ? 1 : 0)
        .SetRedirectType(static_cast<int64_t>(redirect.redirect_type))
        .SetCookieAccessType(static_cast<int64_t>(redirect.access_type))
        .SetRedirectAndInitialSiteSame(initial_site_same)
        .SetRedirectAndFinalSiteSame(final_site_same)
        .SetInitialAndFinalSitesSame(chain.initial_and_final_sites_same)
        .SetRedirectChainIndex(redirect.index)
        .SetRedirectChainLength(chain.length)
        .SetClientBounceDelay(
            BucketizeBounceDelay(redirect.client_bounce_delay))
        .SetHasStickyActivation(redirect.has_sticky_activation)
        .Record(ukm::UkmRecorder::Get());
  }

  if (initial_site_same || final_site_same) {
    // Don't record UMA metrics for same-site redirects.
    return;
  }

  // Record this bounce in the DIPS database.
  if (redirect.access_type != CookieAccessType::kUnknown) {
    record_bounce.Run(
        redirect.url, redirect.time,
        /*stateful=*/redirect.access_type > CookieAccessType::kRead);
  }

  RedirectCategory category =
      ClassifyRedirect(redirect.access_type, redirect.has_interaction.value());
  UmaHistogramBounceCategory(category, chain.cookie_mode.value(),
                             redirect.redirect_type);
}

void DIPSService::OnTimerFired() {
  base::Time start = base::Time::Now();
  storage_.AsyncCall(&DIPSStorage::GetSitesToClear)
      .Then(base::BindOnce(&DIPSService::DeleteDIPSEligibleState,
                           weak_factory_.GetWeakPtr(), start));
}

void DIPSService::DeleteDIPSEligibleState(
    base::Time deletion_start,
    std::vector<std::string> sites_to_clear) {
  base::UmaHistogramCounts1000(
      base::StrCat({"Privacy.DIPS.ClearedSitesCount",
                    GetHistogramSuffix(GetCookieMode())}),
      sites_to_clear.size());

  if (sites_to_clear.empty()) {
    return;
  }

  for (const auto& site : sites_to_clear) {
    const ukm::SourceId source_id = ukm::UkmRecorder::GetSourceIdForDipsSite(
        base::PassKey<DIPSService>(), site);
    ukm::builders::DIPS_Deletion(source_id).SetDetected(true).Record(
        ukm::UkmRecorder::Get());
  }

  if (ShouldBlockThirdPartyCookies() && dips::kDeletionEnabled.Get()) {
    if (IsShuttingDown()) {
      return;
    }

    std::vector<std::string> excepted_sites;
    std::vector<std::string> non_excepted_sites;

    for (const auto& site : sites_to_clear) {
      if (HasCookieException(site)) {
        excepted_sites.push_back(site);
      } else {
        non_excepted_sites.push_back(site);
      }
    }

    if (excepted_sites.empty()) {
      PostDeletionTaskToUIThread(deletion_start, std::move(non_excepted_sites));
    } else {
      storage_.AsyncCall(&DIPSStorage::RemoveRows)
          .WithArgs(std::move(excepted_sites))
          .Then(base::BindOnce(&DIPSService::PostDeletionTaskToUIThread,
                               weak_factory_.GetWeakPtr(), deletion_start,
                               std::move(non_excepted_sites)));
    }
  } else {
    storage_.AsyncCall(&DIPSStorage::RemoveRows)
        .WithArgs(std::move(sites_to_clear))
        .Then(base::BindOnce(&UmaHistogramDeletionLatency, deletion_start));
  }
}

void DIPSService::PostDeletionTaskToUIThread(base::Time deletion_start,
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
                                base::BindOnce(&UmaHistogramDeletionLatency,
                                               deletion_start)));
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

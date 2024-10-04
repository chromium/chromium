// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_service_impl.h"

#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <vector>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/dips/chrome_dips_delegate.h"
#include "chrome/browser/dips/dips_redirect_info.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/dips/dips_storage.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/browser/dips/persistent_repeating_timer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/dips_utils.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "net/cookies/cookie_setting_override.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "url/origin.h"

namespace {

// Controls whether UKM metrics are collected for DIPS.
BASE_FEATURE(kDipsUkm, "DipsUkm", base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the database requests are executed on a foreground sequence.
BASE_FEATURE(kDipsOnForegroundSequence,
             "DipsOnForegroundSequence",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

inline void UmaHistogramBounceDelay(base::TimeDelta sample) {
  base::UmaHistogramTimes("Privacy.DIPS.ServerBounceDelay", sample);
}

inline void UmaHistogramBounceChainDelay(base::TimeDelta sample) {
  base::UmaHistogramTimes("Privacy.DIPS.ServerBounceChainDelay", sample);
}

inline void UmaHistogramBounceStatusCode(int response_code, bool cached) {
  base::UmaHistogramSparse(cached ? "Privacy.DIPS.BounceStatusCode.Cached"
                                  : "Privacy.DIPS.BounceStatusCode.NoCache",
                           response_code);
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

net::CookiePartitionKeyCollection CookiePartitionKeyCollectionForSites(
    const std::vector<std::string>& sites) {
  std::vector<net::CookiePartitionKey> keys;
  for (const auto& site : sites) {
    for (const auto& [scheme, port] :
         {std::make_pair("http", 80), std::make_pair("https", 443)}) {
      auto key = net::CookiePartitionKey::FromStorageKeyComponents(
          net::SchemefulSite(
              url::Origin::CreateFromNormalizedTuple(scheme, site, port)),
          net::CookiePartitionKey::AncestorChainBit::kCrossSite,
          /*nonce=*/std::nullopt);
      if (key.has_value()) {
        keys.push_back(*key);
      }
    }
  }
  return net::CookiePartitionKeyCollection(keys);
}

class StateClearer : public content::BrowsingDataRemover::Observer {
 public:
  StateClearer(const StateClearer&) = delete;
  StateClearer& operator=(const StateClearer&) = delete;

  ~StateClearer() override { remover_->RemoveObserver(this); }

  // Clears state for the sites in `sites_to_clear`. Runs `callback` once
  // clearing is complete.
  //
  // NOTE: This deletion task removing rows for `sites_to_clear` from the
  // DIPSStorage backend relies on the assumption that rows flagged as DIPS
  // eligible don't have user interaction time values. So even though 'remover'
  // will only clear the storage timestamps, that's sufficient to delete the
  // entire row.
  static void DeleteState(content::BrowsingDataRemover* remover,
                          std::vector<std::string> sites_to_clear,
                          base::OnceClosure callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // This filter will match unpartitioned cookies and storage, as well as
    // storage (but not cookies) that is partitioned under tracking domains.
    std::unique_ptr<content::BrowsingDataFilterBuilder> filter =
        content::BrowsingDataFilterBuilder::Create(
            content::BrowsingDataFilterBuilder::Mode::kDelete);
    for (const auto& site : sites_to_clear) {
      filter->AddRegisterableDomain(site);
    }
    // Don't delete CHIPS partitioned under non-tracking sites.
    filter->SetCookiePartitionKeyCollection(
        net::CookiePartitionKeyCollection());

    // This filter will match cookies partitioned under tracking domains.
    std::unique_ptr<content::BrowsingDataFilterBuilder>
        partitioned_cookie_filter = content::BrowsingDataFilterBuilder::Create(
            content::BrowsingDataFilterBuilder::Mode::kPreserve);
    partitioned_cookie_filter->SetCookiePartitionKeyCollection(
        CookiePartitionKeyCollectionForSites(sites_to_clear));
    partitioned_cookie_filter->SetPartitionedCookiesOnly(true);
    // We don't add any domains to this filter, so with mode=kPreserve it will
    // delete everything partitioned under the sites.

    // StateClearer manages its own lifetime and deletes itself when finished.
    StateClearer* clearer =
        new StateClearer(remover, /*callback_count=*/2, std::move(callback));
    chrome_browsing_data_remover::DataType remove_mask =
        chrome_browsing_data_remover::FILTERABLE_DATA_TYPES;
    if (base::FeatureList::IsEnabled(features::kDIPSPreservePSData)) {
      remove_mask &= ~content::BrowsingDataRemover::DATA_TYPE_PRIVACY_SANDBOX;
    }
    remover->RemoveWithFilterAndReply(
        base::Time::Min(), base::Time::Max(),
        remove_mask |
            content::BrowsingDataRemover::DATA_TYPE_AVOID_CLOSING_CONNECTIONS,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
            content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
        std::move(filter), clearer);
    remover->RemoveWithFilterAndReply(
        base::Time::Min(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_COOKIES,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
            content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
        std::move(partitioned_cookie_filter), clearer);
  }

 private:
  // StateClearer will run `callback` and delete itself after
  // OnBrowsingDataRemoverDone() is called `callback_count` times.
  StateClearer(content::BrowsingDataRemover* remover,
               int callback_count,
               base::OnceClosure callback)
      : remover_(remover),
        deletion_start_(base::Time::Now()),
        expected_callback_count_(callback_count),
        callback_(std::move(callback)) {
    remover_->AddObserver(this);
  }

  // BrowsingDataRemover::Observer overrides:
  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override {
    CHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (++callback_count_ == expected_callback_count_) {
      UmaHistogramDeletionLatency(deletion_start_);
      std::move(callback_).Run();
      delete this;  // Matches the new in DeleteState()
    }
  }

  raw_ptr<content::BrowsingDataRemover> remover_;
  const base::Time deletion_start_;
  const int expected_callback_count_;
  int callback_count_ = 0;
  base::OnceClosure callback_;
};

class DipsTimerStorage : public dips::PersistentRepeatingTimer::Storage {
 public:
  explicit DipsTimerStorage(base::SequenceBound<DIPSStorage>* dips_storage);
  ~DipsTimerStorage() override;

  // Reads the timestamp from the DIPS DB.
  void GetLastFired(TimeCallback callback) const override {
    dips_storage_->AsyncCall(&DIPSStorage::GetTimerLastFired)
        .Then(std::move(callback));
  }
  // Write the timestamp to the DIPS DB.
  void SetLastFired(base::Time time) override {
    dips_storage_
        ->AsyncCall(base::IgnoreResult(&DIPSStorage::SetTimerLastFired))
        .WithArgs(time);
  }

 private:
  raw_ref<base::SequenceBound<DIPSStorage>> dips_storage_;
};

DipsTimerStorage::DipsTimerStorage(
    base::SequenceBound<DIPSStorage>* dips_storage)
    : dips_storage_(CHECK_DEREF(dips_storage)) {}

DipsTimerStorage::~DipsTimerStorage() = default;

}  // namespace

/* static */
DIPSService* DIPSService::Get(content::BrowserContext* context) {
  return DIPSServiceImpl::Get(context);
}

DIPSServiceImpl::DIPSServiceImpl(base::PassKey<DIPSServiceFactory>,
                                 content::BrowserContext* context)
    : browser_context_(context), dips_delegate_(ChromeDipsDelegate::Create()) {
  DCHECK(base::FeatureList::IsEnabled(features::kDIPS));
  std::optional<base::FilePath> path_to_use;
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

  repeating_timer_ = CreateTimer();
  repeating_timer_->Start();
}

std::unique_ptr<dips::PersistentRepeatingTimer> DIPSServiceImpl::CreateTimer() {
  CHECK(!storage_.is_null());
  // base::Unretained(this) is safe here since the timer that is created has the
  // same lifetime as this service.
  return std::make_unique<dips::PersistentRepeatingTimer>(
      std::make_unique<DipsTimerStorage>(&storage_),
      features::kDIPSTimerDelay.Get(),
      base::BindRepeating(&DIPSServiceImpl::OnTimerFired,
                          base::Unretained(this)));
}

DIPSServiceImpl::~DIPSServiceImpl() {
  // Some UserData may interact with `this` during their destruction. Delete
  // them now, before it's too late. If we don't delete them manually,
  // ~SupportsUserData() will, but `this` will be invalid at that time.
  //
  // Note that we can't put this call in ~DIPSService() either, even though
  // DIPSService is the class that directly inherits from SupportsUserData.
  // Because when ~DIPSService() is called, it's undefined behavior to call
  // pure virtual functions like DIPSService::RemoveObserver().
  ClearAllUserData();
}

/* static */
DIPSServiceImpl* DIPSServiceImpl::Get(content::BrowserContext* context) {
  return DIPSServiceFactory::GetForBrowserContext(context);
}

scoped_refptr<base::SequencedTaskRunner> DIPSServiceImpl::CreateTaskRunner() {
  if (base::FeatureList::IsEnabled(kDipsOnForegroundSequence)) {
    return base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  }
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::ThreadPolicy::PREFER_BACKGROUND});
}

DIPSCookieMode DIPSServiceImpl::GetCookieMode() const {
  return GetDIPSCookieMode(browser_context_->IsOffTheRecord());
}

void DIPSServiceImpl::RemoveEvents(const base::Time& delete_begin,
                                   const base::Time& delete_end,
                                   network::mojom::ClearDataFilterPtr filter,
                                   DIPSEventRemovalType type) {
  // Storage init should be finished by now, so no need to delay until then.
  storage_.AsyncCall(&DIPSStorage::RemoveEvents)
      .WithArgs(delete_begin, delete_end, std::move(filter), type);
}

void DIPSServiceImpl::HandleRedirectChain(
    std::vector<DIPSRedirectInfoPtr> redirects,
    DIPSRedirectChainInfoPtr chain,
    base::RepeatingCallback<void(const GURL&)> stateful_bounce_callback) {
  DCHECK_LE(redirects.size(), chain->length);

  if (redirects.empty()) {
    DCHECK(!chain->is_partial_chain);
    for (auto& observer : observers_) {
      observer.OnChainHandled(chain);
    }
    return;
  }

  if (base::FeatureList::IsEnabled(kDipsUkm)) {
    if (chain->initial_url.source_id != ukm::kInvalidSourceId) {
      ukm::builders::DIPS_ChainBegin(chain->initial_url.source_id)
          .SetChainId(chain->chain_id)
          .SetInitialAndFinalSitesSame(chain->initial_and_final_sites_same)
          .Record(ukm::UkmRecorder::Get());
    }

    if (chain->final_url.source_id != ukm::kInvalidSourceId) {
      ukm::builders::DIPS_ChainEnd(chain->final_url.source_id)
          .SetChainId(chain->chain_id)
          .SetInitialAndFinalSitesSame(chain->initial_and_final_sites_same)
          .Record(ukm::UkmRecorder::Get());
    }
  }

  base::TimeDelta total_server_bounce_delay;
  for (const auto& redirect : redirects) {
    if (redirect->redirect_type == DIPSRedirectType::kServer) {
      total_server_bounce_delay += redirect->server_bounce_delay;
    }
  }
  UmaHistogramBounceChainDelay(total_server_bounce_delay);

  chain->cookie_mode = GetCookieMode();
  // Copy the URL out before |redirects| is moved, to avoid use-after-move.
  GURL url = redirects[0]->url.url;
  storage_.AsyncCall(&DIPSStorage::Read)
      .WithArgs(url)
      .Then(base::BindOnce(&DIPSServiceImpl::GotState,
                           weak_factory_.GetWeakPtr(), std::move(redirects),
                           std::move(chain), 0, stateful_bounce_callback));
}

void DIPSServiceImpl::RecordInteractionForTesting(const GURL& url) {
  storage_.AsyncCall(&DIPSStorage::RecordInteraction)
      .WithArgs(url, base::Time::Now(), GetCookieMode());
}

void DIPSServiceImpl::DidSiteHaveInteractionSince(
    const GURL& url,
    base::Time bound,
    CheckInteractionCallback callback) const {
  storage_.AsyncCall(&DIPSStorage::DidSiteHaveInteractionSince)
      .WithArgs(url, bound)
      .Then(std::move(callback));
}

void DIPSServiceImpl::GotState(
    std::vector<DIPSRedirectInfoPtr> redirects,
    DIPSRedirectChainInfoPtr chain,
    size_t index,
    base::RepeatingCallback<void(const GURL&)> stateful_bounce_callback,
    const DIPSState url_state) {
  DCHECK_LT(index, redirects.size());

  DIPSRedirectInfo* redirect = redirects[index].get();
  // If there's any user interaction recorded in the DIPS DB, that's engagement.
  DCHECK(!redirect->has_interaction.has_value());
  redirect->has_interaction = url_state.user_interaction_times().has_value();
  DCHECK(!redirect->chain_id.has_value());
  redirect->chain_id = chain->chain_id;
  DCHECK(!redirect->chain_index.has_value());
  // If the chain was too long, some redirects may have been trimmed already,
  // which would make `index` not the "true" index of the redirect in the whole
  // chain. `chain->length` is accurate though. `chain->length -
  // redirects.size()` is then the number of trimmed redirects; so add that to
  // `index` to get the "true" index to report in our metrics.
  redirect->chain_index = chain->length - redirects.size() + index;
  HandleRedirect(*redirect, *chain,
                 base::BindRepeating(&DIPSServiceImpl::RecordBounce,
                                     base::Unretained(this)),
                 stateful_bounce_callback);

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
  GURL url = redirects[index + 1]->url.url;
  storage_.AsyncCall(&DIPSStorage::Read)
      .WithArgs(url)
      .Then(base::BindOnce(&DIPSServiceImpl::GotState,
                           weak_factory_.GetWeakPtr(), std::move(redirects),
                           std::move(chain), index + 1,
                           stateful_bounce_callback));
}

void DIPSServiceImpl::RecordBounce(
    const GURL& url,
    bool has_3pc_exception,
    const GURL& final_url,
    base::Time time,
    bool stateful,
    base::RepeatingCallback<void(const GURL&)> stateful_bounce_callback) {
  // If the bounced URL has a 3PC exception when embedded under the initial or
  // final URL in the redirect,then clear the tracking site from the DIPS DB, to
  // avoid deleting its storage. The exception overrides any bounces from
  // non-excepted sites.
  if (has_3pc_exception) {
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
      // TODO(crbug.com/40268849): Investigate and fix the presence of empty
      // site(s) in the `site_to_clear` list. Once this is fixed remove this
      // escape.
      if (url.is_empty()) {
        UmaHistogramDeletion(GetCookieMode(), DIPSDeletionAction::kIgnored);
      } else {
        UmaHistogramDeletion(GetCookieMode(), DIPSDeletionAction::kExcepted);
      }
    }

    const std::set<std::string> site_to_clear{GetSiteForDIPS(url)};
    // Don't clear the row if the tracker has history indicating that we
    // should preserve that context for future bounces.
    storage_.AsyncCall(&DIPSStorage::RemoveRowsWithoutProtectiveEvent)
        .WithArgs(site_to_clear);

    return;
  }

  // If the bounce is stateful and not excepted by cookie settings, run the
  // callback.
  if (stateful) {
    stateful_bounce_callback.Run(final_url);
  }

  storage_.AsyncCall(&DIPSStorage::RecordBounce).WithArgs(url, time, stateful);
}

/*static*/
void DIPSServiceImpl::HandleRedirect(
    const DIPSRedirectInfo& redirect,
    const DIPSRedirectChainInfo& chain,
    RecordBounceCallback record_bounce,
    base::RepeatingCallback<void(const GURL&)> stateful_bounce_callback) {
  bool initial_site_same = (redirect.site == chain.initial_site);
  bool final_site_same = (redirect.site == chain.final_site);
  DCHECK_LT(redirect.chain_index.value(), chain.length);

  if (base::FeatureList::IsEnabled(kDipsUkm)) {
    ukm::builders::DIPS_Redirect(redirect.url.source_id)
        .SetSiteEngagementLevel(redirect.has_interaction.value() ? 1 : 0)
        .SetRedirectType(static_cast<int64_t>(redirect.redirect_type))
        .SetCookieAccessType(static_cast<int64_t>(redirect.access_type))
        .SetRedirectAndInitialSiteSame(initial_site_same)
        .SetRedirectAndFinalSiteSame(final_site_same)
        .SetInitialAndFinalSitesSame(chain.initial_and_final_sites_same)
        .SetRedirectChainIndex(redirect.chain_index.value())
        .SetRedirectChainLength(chain.length)
        .SetIsPartialRedirectChain(chain.is_partial_chain)
        .SetClientBounceDelay(
            BucketizeBounceDelay(redirect.client_bounce_delay))
        .SetHasStickyActivation(redirect.has_sticky_activation)
        .SetWebAuthnAssertionRequestSucceeded(
            redirect.web_authn_assertion_request_succeeded)
        .SetChainId(redirect.chain_id.value())
        .Record(ukm::UkmRecorder::Get());
  }

  if (initial_site_same || final_site_same) {
    // Don't record UMA metrics for same-site redirects.
    return;
  }

  // Record this bounce in the DIPS database.
  if (redirect.access_type != SiteDataAccessType::kUnknown) {
    record_bounce.Run(
        redirect.url.url, redirect.has_3pc_exception.value(),
        chain.final_url.url, redirect.time,
        /*stateful=*/redirect.access_type > SiteDataAccessType::kRead,
        stateful_bounce_callback);
  }

  RedirectCategory category =
      ClassifyRedirect(redirect.access_type, redirect.has_interaction.value());
  UmaHistogramBounceCategory(category, chain.cookie_mode.value(),
                             redirect.redirect_type);

  if (redirect.redirect_type == DIPSRedirectType::kServer) {
    UmaHistogramBounceDelay(redirect.server_bounce_delay);
    UmaHistogramBounceStatusCode(redirect.response_code,
                                 redirect.was_response_cached);
  }
}

void DIPSServiceImpl::OnTimerFired() {
  // Storage init should be finished by now, so no need to delay until then.
  storage_.AsyncCall(&DIPSStorage::GetSitesToClear)
      .WithArgs(std::nullopt)
      .Then(base::BindOnce(&DIPSServiceImpl::DeleteDIPSEligibleState,
                           weak_factory_.GetWeakPtr(), base::DoNothing()));
}

void DIPSServiceImpl::DeleteEligibleSitesImmediately(
    DeletedSitesCallback callback) {
  // Storage init should be finished by now, so no need to delay until then.
  storage_.AsyncCall(&DIPSStorage::GetSitesToClear)
      .WithArgs(base::Seconds(0))
      .Then(base::BindOnce(&DIPSServiceImpl::DeleteDIPSEligibleState,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void DIPSServiceImpl::DeleteDIPSEligibleState(
    DeletedSitesCallback callback,
    std::vector<std::string> sites_to_clear) {
  // Do not clear sites from currently open tabs.
  for (const std::pair<std::string, int> site_ctr : open_sites_) {
    CHECK(site_ctr.second > 0);
    std::erase(sites_to_clear, site_ctr.first);
  }

  if (sites_to_clear.empty()) {
    UmaHistogramClearedSitesCount(GetCookieMode(), sites_to_clear.size());
    std::move(callback).Run(std::vector<std::string>());
    return;
  }

  UmaHistogramClearedSitesCount(GetCookieMode(), sites_to_clear.size());

  for (const auto& site : sites_to_clear) {
    // TODO(crbug.com/40268849): Investigate and fix the presence of empty
    // site(s) in the `site_to_clear` list. Once this is fixed remove this loop
    // escape.
    if (site.empty()) {
      continue;
    }
    const ukm::SourceId source_id = ukm::UkmRecorder::GetSourceIdForDipsSite(
        base::PassKey<DIPSServiceImpl>(), site);
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
      // TODO(crbug.com/40268849): Investigate and fix the presence of empty
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
    RunDeletionTaskOnUIThread(std::move(filtered_sites_to_clear),
                              std::move(finish_callback));
  } else {
    for (const auto& site : sites_to_clear) {
      // TODO(crbug.com/40268849): Investigate and fix the presence of empty
      // site(s) in the `site_to_clear` list. Once this is fixed remove this
      // loop escape.
      if (site.empty()) {
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

void DIPSServiceImpl::RunDeletionTaskOnUIThread(std::vector<std::string> sites,
                                                base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  StateClearer::DeleteState(browser_context_->GetBrowsingDataRemover(),
                            std::move(sites), std::move(callback));
}

void DIPSServiceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DIPSServiceImpl::RemoveObserver(const Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DIPSServiceImpl::RecordBrowserSignIn(std::string_view domain) {
  storage()
      ->AsyncCall(&DIPSStorage::RecordInteraction)
      .WithArgs(url::SchemeHostPort("http", domain, 80).GetURL(),
                base::Time::Now(), GetCookieMode());
}

void DIPSServiceImpl::MaybeNotifyCreated(base::PassKey<DIPSServiceFactory>) {
  if (delegate_notified_) {
    return;
  }

  delegate_notified_ = true;  // Set this first to prevent infinite recursion.
  ChromeDipsDelegate::Create()->OnDipsServiceCreated(browser_context_, this);
}

void DIPSServiceImpl::NotifyStatefulBounce(content::WebContents* web_contents) {
  for (auto& observer : observers_) {
    observer.OnStatefulBounce(web_contents);
  }
}

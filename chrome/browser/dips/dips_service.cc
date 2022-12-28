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
#include "base/task/thread_pool.h"
#include "base/time/time.h"
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
  if (repeating_timer_)
    repeating_timer_->Start();
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
  cookie_settings_.reset();
}

scoped_refptr<base::SequencedTaskRunner> DIPSService::CreateTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::ThreadPolicy::PREFER_BACKGROUND});
}

DIPSCookieMode DIPSService::GetCookieMode() const {
  return GetDIPSCookieMode(browser_context_->IsOffTheRecord(),
                           cookie_settings_->ShouldBlockThirdPartyCookies());
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
  redirect->has_interaction =
      url_state.user_interaction_times().last.has_value();
  HandleRedirect(
      *redirect, *chain,
      base::BindRepeating(&DIPSService::RecordBounce, base::Unretained(this)));

  if (index + 1 >= redirects.size()) {
    // All redirects handled.
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
  storage_.AsyncCall(&DIPSStorage::DeleteDIPSEligibleState)
      .WithArgs(GetCookieMode())
      .Then(base::BindOnce(
          [](base::Time deletion_start) {
            base::UmaHistogramLongTimes100("Privacy.DIPS.DeletionLatency",
                                           base::Time::Now() - deletion_start);
          },
          start));
}

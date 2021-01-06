// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_service.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "components/blocklist/opt_out_blocklist/opt_out_store.h"
#include "components/blocklist/opt_out_blocklist/sql/opt_out_store_sql.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/previews/content/previews_decider_impl.h"
#include "components/previews/content/previews_optimization_guide.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_logger.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Returns the list of regular expressions that need to be matched against the
// webpage URL. If there is a partial match, then the webpage is ineligible for
// DeferAllScript preview.
// TODO(tbansal): Consider detecting form elements within Chrome and
// automatically reloading the webpage without preview when a form element is
// detected.
std::unique_ptr<previews::RegexpList> GetDenylistRegexpsForDeferAllScript() {
  if (!previews::params::IsDeferAllScriptPreviewsEnabled())
    return nullptr;

  std::unique_ptr<previews::RegexpList> regexps =
      std::make_unique<previews::RegexpList>();
  // Regexes of webpages for which previews are generally not shown. Taken from
  // http://shortn/_bGb5REgTFD.
  regexps->emplace_back(
      std::make_unique<re2::RE2>("(?i)(log|sign)[-_]?(in|out)"));
  regexps->emplace_back(std::make_unique<re2::RE2>("(?i)/banking"));
  DCHECK(regexps->back()->ok());

  return regexps;
}

// Returns true if previews can be shown for |type|.
bool IsPreviewsTypeEnabled(previews::PreviewsType type) {
  switch (type) {
    case previews::PreviewsType::DEPRECATED_OFFLINE:
      return false;
    case previews::PreviewsType::DEPRECATED_LOFI:
      return false;
    case previews::PreviewsType::DEPRECATED_LITE_PAGE_REDIRECT:
      return false;
    case previews::PreviewsType::DEPRECATED_LITE_PAGE:
      return false;
    case previews::PreviewsType::NOSCRIPT:
      return false;
    case previews::PreviewsType::RESOURCE_LOADING_HINTS:
      return false;
    case previews::PreviewsType::DEFER_ALL_SCRIPT:
      return previews::params::IsDeferAllScriptPreviewsEnabled();
    case previews::PreviewsType::DEPRECATED_AMP_REDIRECTION:
      return false;
    case previews::PreviewsType::UNSPECIFIED:
      // Not a real previews type so treat as false.
      return false;
    case previews::PreviewsType::NONE:
    case previews::PreviewsType::LAST:
      break;
  }
  NOTREACHED();
  return false;
}

// Returns the version of preview treatment |type|. Defaults to 0 if not
// specified in field trial config.
int GetPreviewsTypeVersion(previews::PreviewsType type) {
  switch (type) {
    case previews::PreviewsType::NOSCRIPT:
      return -1;
    case previews::PreviewsType::RESOURCE_LOADING_HINTS:
      return -1;
    case previews::PreviewsType::DEFER_ALL_SCRIPT:
      return previews::params::DeferAllScriptPreviewsVersion();
    case previews::PreviewsType::NONE:
    case previews::PreviewsType::UNSPECIFIED:
    case previews::PreviewsType::LAST:
    case previews::PreviewsType::DEPRECATED_AMP_REDIRECTION:
    case previews::PreviewsType::DEPRECATED_LITE_PAGE:
    case previews::PreviewsType::DEPRECATED_LITE_PAGE_REDIRECT:
    case previews::PreviewsType::DEPRECATED_LOFI:
    case previews::PreviewsType::DEPRECATED_OFFLINE:
      break;
  }
  NOTREACHED();
  return -1;
}

}  // namespace

// static
bool PreviewsService::HasURLRedirectCycle(
    const GURL& start_url,
    const base::MRUCache<GURL, GURL>& redirect_history) {
  // Using an ordered set since using an unordered set requires defining
  // comparator operator for GURL.
  std::set<GURL> urls_seen_so_far;
  GURL current_url = start_url;

  while (true) {
    urls_seen_so_far.insert(current_url);

    // Check if |current_url| redirects to another URL that is already visited.
    auto it = redirect_history.Peek(current_url);
    if (it == redirect_history.end())
      return false;

    GURL redirect_target = it->second;
    if (urls_seen_so_far.find(redirect_target) != urls_seen_so_far.end())
      return true;
    current_url = redirect_target;
  }

  NOTREACHED();
  return false;
}

// static
blocklist::BlocklistData::AllowedTypesAndVersions
PreviewsService::GetAllowedPreviews() {
  blocklist::BlocklistData::AllowedTypesAndVersions enabled_previews;

  // Loop across all previews types (relies on sequential enum values).
  for (int i = static_cast<int>(previews::PreviewsType::NONE) + 1;
       i < static_cast<int>(previews::PreviewsType::LAST); ++i) {
    previews::PreviewsType type = static_cast<previews::PreviewsType>(i);
    if (IsPreviewsTypeEnabled(type))
      enabled_previews.insert({i, GetPreviewsTypeVersion(type)});
  }
  return enabled_previews;
}

PreviewsService::PreviewsService(content::BrowserContext* browser_context)
    : previews_https_notification_infobar_decider_(
          std::make_unique<PreviewsHTTPSNotificationInfoBarDecider>(
              browser_context)),
      browser_context_(browser_context),
      // Set cache size to 25 entries.  This should be sufficient since the
      // redirect loop cache is needed for only one navigation.
      redirect_history_(25u),
      defer_all_script_denylist_regexps_(
          GetDenylistRegexpsForDeferAllScript()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!browser_context->IsOffTheRecord());
}

PreviewsService::~PreviewsService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void PreviewsService::Initialize(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
    const base::FilePath& profile_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<previews::PreviewsDeciderImpl> previews_decider_impl =
      std::make_unique<previews::PreviewsDeciderImpl>(
          base::DefaultClock::GetInstance());

  // Get the background thread to run SQLite on.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  Profile* profile = Profile::FromBrowserContext(browser_context_);

  std::unique_ptr<previews::PreviewsOptimizationGuide> previews_opt_guide;
  OptimizationGuideKeyedService* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (optimization_guide_keyed_service &&
      previews::params::ArePreviewsAllowed() &&
      data_reduction_proxy::DataReductionProxySettings::
          IsDataSaverEnabledByUser(profile->IsOffTheRecord(),
                                   profile->GetPrefs())) {
    previews_opt_guide = std::make_unique<previews::PreviewsOptimizationGuide>(
        optimization_guide_keyed_service);
  }

  previews_ui_service_ = std::make_unique<previews::PreviewsUIService>(
      std::move(previews_decider_impl),
      std::make_unique<blocklist::OptOutStoreSQL>(
          ui_task_runner, background_task_runner,
          profile_path.Append(chrome::kPreviewsOptOutDBFilename)),
      std::move(previews_opt_guide), base::Bind(&IsPreviewsTypeEnabled),
      std::make_unique<previews::PreviewsLogger>(), GetAllowedPreviews(),
      g_browser_process->network_quality_tracker());
}

void PreviewsService::Shutdown() {
  if (previews_https_notification_infobar_decider_)
    previews_https_notification_infobar_decider_->Shutdown();
}

void PreviewsService::ClearBlockList(base::Time begin_time,
                                     base::Time end_time) {
  if (previews_ui_service_)
    previews_ui_service_->ClearBlockList(begin_time, end_time);
}

void PreviewsService::ReportObservedRedirectWithDeferAllScriptPreview(
    const GURL& start_url,
    const GURL& end_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(previews::params::IsDeferAllScriptPreviewsEnabled());

  // If |start_url| has been previously marked as ineligible for the preview,
  // then do not update the existing entry since existing entry might cause
  // |start_url| to be no longer marked as ineligible for the preview. This may
  // happen if marking the URL as ineligible for preview resulted in breakage of
  // the redirect loop.
  if (!IsUrlEligibleForDeferAllScriptPreview(start_url))
    return;

  redirect_history_.Put(start_url, end_url);
}

bool PreviewsService::IsUrlEligibleForDeferAllScriptPreview(
    const GURL& url) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(previews::params::IsDeferAllScriptPreviewsEnabled());

  return !HasURLRedirectCycle(url, redirect_history_);
}

bool PreviewsService::MatchesDeferAllScriptDenyListRegexp(
    const GURL& url) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!defer_all_script_denylist_regexps_)
    return false;

  if (!url.is_valid())
    return false;

  std::string clean_url = base::ToLowerASCII(url.GetAsReferrer().spec());
  for (auto& regexp : *defer_all_script_denylist_regexps_) {
    if (re2::RE2::PartialMatch(clean_url, *regexp))
      return true;
  }

  return false;
}

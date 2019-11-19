// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/browser_feature_extractor.h"

#include <stddef.h>

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/browser_features.h"
#include "chrome/browser/safe_browsing/client_side_detection_host.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/safe_browsing/db/database_manager.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::NavigationController;
using content::NavigationEntry;
using content::ResourceType;
using content::WebContents;

namespace safe_browsing {

namespace {

const int kMaxMalwareIPPerRequest = 5;

void FilterBenignIpsOnIOThread(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    IPUrlMap* ips) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (auto it = ips->begin(); it != ips->end();) {
    if (!database_manager.get() ||
        !database_manager->MatchMalwareIP(it->first)) {
      // it++ here returns a copy of the old iterator and passes it to erase.
      ips->erase(it++);
    } else {
      ++it;
    }
  }
}
}  // namespace

IPUrlInfo::IPUrlInfo(const std::string& url,
                     const std::string& method,
                     const std::string& referrer,
                     const ResourceType& resource_type)
      : url(url),
        method(method),
        referrer(referrer),
        resource_type(resource_type) {
}

IPUrlInfo::IPUrlInfo(const IPUrlInfo& other) = default;

IPUrlInfo::~IPUrlInfo() {}

BrowseInfo::BrowseInfo() : http_status_code(0) {}

BrowseInfo::~BrowseInfo() {}

static void AddFeature(const std::string& feature_name,
                       double feature_value,
                       ClientPhishingRequest* request) {
  DCHECK(request);
  ClientPhishingRequest::Feature* feature =
      request->add_non_model_feature_map();
  feature->set_name(feature_name);
  feature->set_value(feature_value);
  DVLOG(2) << "Browser feature: " << feature->name() << " " << feature->value();
}

static void AddMalwareIpUrlInfo(const std::string& ip,
                                const std::vector<IPUrlInfo>& meta_infos,
                                ClientMalwareRequest* request) {
  DCHECK(request);
  for (auto it = meta_infos.begin(); it != meta_infos.end(); ++it) {
    ClientMalwareRequest::UrlInfo* urlinfo =
        request->add_bad_ip_url_info();
    // We add the information about url on the bad ip.
    urlinfo->set_ip(ip);
    urlinfo->set_url(it->url);
    urlinfo->set_method(it->method);
    urlinfo->set_referrer(it->referrer);
    urlinfo->set_resource_type(static_cast<int>(it->resource_type));
  }
  DVLOG(2) << "Added url info for bad ip: " << ip;
}

static void AddNavigationFeatures(const std::string& feature_prefix,
                                  NavigationController* controller,
                                  int index,
                                  const std::vector<GURL>& redirect_chain,
                                  ClientPhishingRequest* request) {
  NavigationEntry* entry = controller->GetEntryAtIndex(index);
  bool is_secure_referrer = entry->GetReferrer().url.SchemeIsCryptographic();
  if (!is_secure_referrer) {
    AddFeature(base::StringPrintf("%s%s=%s", feature_prefix.c_str(), kReferrer,
                                  entry->GetReferrer().url.spec().c_str()),
               1.0, request);
  }
  AddFeature(feature_prefix + kHasSSLReferrer, is_secure_referrer ? 1.0 : 0.0,
             request);
  AddFeature(feature_prefix + kPageTransitionType,
             static_cast<double>(
                 ui::PageTransitionStripQualifier(entry->GetTransitionType())),
             request);
  AddFeature(feature_prefix + kIsFirstNavigation, index == 0 ? 1.0 : 0.0,
             request);
  // Redirect chain should always be at least of size one, as the rendered
  // url is the last element in the chain.
  if (redirect_chain.empty()) {
    NOTREACHED();
    return;
  }
  if (redirect_chain.back() != entry->GetURL()) {
    // I originally had this as a DCHECK but I saw a failure once that I
    // can't reproduce. It looks like it might be related to the
    // navigation controller only keeping a limited number of navigation
    // events. For now we'll just attach a feature specifying that this is
    // a mismatch and try and figure out what to do with it on the server.
    DLOG(WARNING) << "Expected:" << entry->GetURL()
                 << " Actual:" << redirect_chain.back();
    AddFeature(feature_prefix + kRedirectUrlMismatch, 1.0, request);
    return;
  }
  // We skip the last element since it should just be the current url.
  for (size_t i = 0; i < redirect_chain.size() - 1; i++) {
    std::string printable_redirect = redirect_chain[i].spec();
    if (redirect_chain[i].SchemeIsCryptographic()) {
      printable_redirect = kSecureRedirectValue;
    }
    AddFeature(base::StringPrintf("%s%s[%" PRIuS "]=%s", feature_prefix.c_str(),
                                  kRedirect, i, printable_redirect.c_str()),
               1.0, request);
  }
}

BrowserFeatureExtractor::BrowserFeatureExtractor(WebContents* tab,
                                                 ClientSideDetectionHost* host)
    : tab_(tab), host_(host) {
  DCHECK(tab);
}

BrowserFeatureExtractor::~BrowserFeatureExtractor() {
  weak_factory_.InvalidateWeakPtrs();
}

void BrowserFeatureExtractor::ExtractFeatures(
    const BrowseInfo* info,
    std::unique_ptr<ClientPhishingRequest> request,
    DoneCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(request);
  DCHECK(info);
  DCHECK(GURL(request->url()).SchemeIsHTTPOrHTTPS());
  DCHECK(!callback.is_null());
  // Extract features pertaining to this navigation.
  NavigationController& controller = tab_->GetController();
  int url_index = -1;
  int first_host_index = -1;

  GURL request_url(request->url());
  int index = controller.GetCurrentEntryIndex();
  // The url that we are extracting features for should already be commited.
  DCHECK_NE(index, -1);
  for (; index >= 0; index--) {
    NavigationEntry* entry = controller.GetEntryAtIndex(index);
    if (url_index == -1 && entry->GetURL() == request_url) {
      // It's possible that we've been on the on the possibly phishy url before
      // in this tab, so make sure that we use the latest navigation for
      // features.
      // Note that it's possible that the url_index should always be the
      // latest entry, but I'm worried about possible races during a navigation
      // and transient entries (i.e. interstiatials) so for now we will just
      // be cautious.
      url_index = index;
    } else if (index < url_index) {
      if (entry->GetURL().host_piece() == request_url.host_piece()) {
        first_host_index = index;
      } else {
        // We have found the possibly phishing url, but we are no longer on the
        // host. No reason to look back any further.
        break;
      }
    }
  }

  // Add features pertaining to how we got to
  //   1) The candidate url
  //   2) The first url on the same host as the candidate url (assuming that
  //      it's different from the candidate url).
  if (url_index != -1) {
    AddNavigationFeatures(std::string(), &controller, url_index,
                          info->url_redirects, request.get());
  }
  if (first_host_index != -1) {
    AddNavigationFeatures(kHostPrefix, &controller, first_host_index,
                          info->host_redirects, request.get());
  }

  ExtractBrowseInfoFeatures(*info, request.get());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&BrowserFeatureExtractor::StartExtractFeatures,
                                weak_factory_.GetWeakPtr(), std::move(request),
                                std::move(callback)));
}

void BrowserFeatureExtractor::ExtractMalwareFeatures(
    BrowseInfo* info,
    std::unique_ptr<ClientMalwareRequest> request,
    MalwareDoneCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  // Grab the IPs because they might go away before we're done
  // checking them against the IP blacklist on the IO thread.
  std::unique_ptr<IPUrlMap> ips(new IPUrlMap);
  ips->swap(info->ips);

  IPUrlMap* ips_ptr = ips.get();

  // IP blacklist lookups have to happen on the IO thread.
  base::PostTaskAndReply(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&FilterBenignIpsOnIOThread, host_->database_manager(),
                     ips_ptr),
      base::BindOnce(&BrowserFeatureExtractor::FinishExtractMalwareFeatures,
                     weak_factory_.GetWeakPtr(), std::move(ips),
                     std::move(callback), std::move(request)));
}

void BrowserFeatureExtractor::ExtractBrowseInfoFeatures(
    const BrowseInfo& info,
    ClientPhishingRequest* request) {
  if (info.unsafe_resource.get()) {
    // A SafeBrowsing interstitial was shown for the current URL.
    AddFeature(kSafeBrowsingMaliciousUrl + info.unsafe_resource->url.spec(),
               1.0, request);
    AddFeature(
        kSafeBrowsingOriginalUrl + info.unsafe_resource->original_url.spec(),
        1.0, request);
    AddFeature(kSafeBrowsingIsSubresource,
               info.unsafe_resource->is_subresource ? 1.0 : 0.0, request);
    AddFeature(kSafeBrowsingThreatType,
               static_cast<double>(info.unsafe_resource->threat_type), request);
  }
  if (info.http_status_code != 0) {
    AddFeature(kHttpStatusCode, info.http_status_code, request);
  }
}

void BrowserFeatureExtractor::StartExtractFeatures(
    std::unique_ptr<ClientPhishingRequest> request,
    DoneCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  history::HistoryService* history;
  if (!request || !request->IsInitialized() || !GetHistoryService(&history)) {
    std::move(callback).Run(/*feature_extraction_succeeded=*/false,
                            std::move(request));
    return;
  }
  GURL request_url(request->url());
  history->QueryURL(
      request_url, true /* wants_visits */,
      base::BindOnce(&BrowserFeatureExtractor::QueryUrlHistoryDone,
                     base::Unretained(this), std::move(request),
                     std::move(callback)),
      &cancelable_task_tracker_);
}

void BrowserFeatureExtractor::QueryUrlHistoryDone(
    std::unique_ptr<ClientPhishingRequest> request,
    DoneCallback callback,
    history::QueryURLResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(request);
  DCHECK(!callback.is_null());
  if (!result.success) {
    // URL is not found in the history.  In practice this should not
    // happen (unless there is a real error) because we just visited
    // that URL.
    std::move(callback).Run(/*feature_extraction_succeeded=*/false,
                            std::move(request));
    return;
  }
  AddFeature(kUrlHistoryVisitCount,
             static_cast<double>(result.row.visit_count()), request.get());

  base::Time threshold = base::Time::Now() - base::TimeDelta::FromDays(1);
  int num_visits_24h_ago = 0;
  int num_visits_typed = 0;
  int num_visits_link = 0;
  for (auto& visit : result.visits) {
    if (!ui::PageTransitionIsMainFrame(visit.transition)) {
      continue;
    }
    if (visit.visit_time < threshold) {
      ++num_visits_24h_ago;
    }
    if (ui::PageTransitionCoreTypeIs(visit.transition,
                                     ui::PAGE_TRANSITION_TYPED)) {
      ++num_visits_typed;
    } else if (ui::PageTransitionCoreTypeIs(visit.transition,
                                            ui::PAGE_TRANSITION_LINK)) {
      ++num_visits_link;
    }
  }
  AddFeature(kUrlHistoryVisitCountMoreThan24hAgo,
             static_cast<double>(num_visits_24h_ago), request.get());
  AddFeature(kUrlHistoryTypedCount, static_cast<double>(num_visits_typed),
             request.get());
  AddFeature(kUrlHistoryLinkCount, static_cast<double>(num_visits_link),
             request.get());

  // Issue next history lookup for host visits.
  history::HistoryService* history;
  if (!GetHistoryService(&history)) {
    std::move(callback).Run(/*feature_extraction_succeeded=*/false,
                            std::move(request));
    return;
  }
  GURL::Replacements rep;
  rep.SetSchemeStr(url::kHttpScheme);
  GURL http_url = GURL(request->url()).ReplaceComponents(rep);
  history->GetVisibleVisitCountToHost(
      http_url,
      base::BindOnce(&BrowserFeatureExtractor::QueryHttpHostVisitsDone,
                     base::Unretained(this), std::move(request),
                     std::move(callback)),
      &cancelable_task_tracker_);
}

void BrowserFeatureExtractor::QueryHttpHostVisitsDone(
    std::unique_ptr<ClientPhishingRequest> request,
    DoneCallback callback,
    history::VisibleVisitCountToHostResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(request);
  DCHECK(!callback.is_null());
  if (!result.success) {
    std::move(callback).Run(/*feature_extraction_succeeded=*/false,
                            std::move(request));
    return;
  }
  SetHostVisitsFeatures(result.count, result.first_visit, true, request.get());

  // Same lookup but for the HTTPS URL.
  history::HistoryService* history;
  if (!GetHistoryService(&history)) {
    std::move(callback).Run(/*feature_extraction_succeeded=*/false,
                            std::move(request));
    return;
  }
  GURL::Replacements rep;
  rep.SetSchemeStr(url::kHttpsScheme);
  GURL https_url = GURL(request->url()).ReplaceComponents(rep);
  history->GetVisibleVisitCountToHost(
      https_url,
      base::BindOnce(&BrowserFeatureExtractor::QueryHttpsHostVisitsDone,
                     base::Unretained(this), std::move(request),
                     std::move(callback)),
      &cancelable_task_tracker_);
}

void BrowserFeatureExtractor::QueryHttpsHostVisitsDone(
    std::unique_ptr<ClientPhishingRequest> request,
    DoneCallback callback,
    history::VisibleVisitCountToHostResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(request);
  DCHECK(!callback.is_null());
  if (!result.success) {
    std::move(callback).Run(/*feature_extraction_succeeded=*/false,
                            std::move(request));
    return;
  }
  SetHostVisitsFeatures(result.count, result.first_visit, false, request.get());
  std::move(callback).Run(/*feature_extraction_succeeded=*/true,
                          std::move(request));
}

void BrowserFeatureExtractor::SetHostVisitsFeatures(
    int num_visits,
    base::Time first_visit,
    bool is_http_query,
    ClientPhishingRequest* request) {
  DCHECK(request);
  AddFeature(is_http_query ? kHttpHostVisitCount : kHttpsHostVisitCount,
             static_cast<double>(num_visits), request);
  if (num_visits > 0) {
    AddFeature(
        is_http_query ? kFirstHttpHostVisitMoreThan24hAgo
                      : kFirstHttpsHostVisitMoreThan24hAgo,
        (first_visit < (base::Time::Now() - base::TimeDelta::FromDays(1)))
            ? 1.0
            : 0.0,
        request);
  }
}

bool BrowserFeatureExtractor::GetHistoryService(
    history::HistoryService** history) {
  *history = NULL;
  if (tab_ && tab_->GetBrowserContext()) {
    Profile* profile = Profile::FromBrowserContext(tab_->GetBrowserContext());
    *history = HistoryServiceFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
    if (*history) {
      return true;
    }
  }
  DVLOG(2) << "Unable to query history.  No history service available.";
  return false;
}

void BrowserFeatureExtractor::FinishExtractMalwareFeatures(
    std::unique_ptr<IPUrlMap> bad_ips,
    MalwareDoneCallback callback,
    std::unique_ptr<ClientMalwareRequest> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  int matched_bad_ips = 0;
  for (IPUrlMap::const_iterator it = bad_ips->begin();
       it != bad_ips->end(); ++it) {
    AddMalwareIpUrlInfo(it->first, it->second, request.get());
    ++matched_bad_ips;
    // Limit the number of matched bad IPs in one request to control
    // the request's size
    if (matched_bad_ips >= kMaxMalwareIPPerRequest) {
      break;
    }
  }
  std::move(callback).Run(/*feature_extraction_succeeded=*/true,
                          std::move(request));
}

}  // namespace safe_browsing

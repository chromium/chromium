// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_host.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/browser_feature_extractor.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing.mojom-shared.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/db/allowlist_checker_client.h"
#include "components/safe_browsing/db/database_manager.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/resource_load_info.mojom.h"
#include "content/public/common/url_constants.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_response_headers.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/referrer.mojom.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::NavigationEntry;
using content::ResourceType;
using content::WebContents;

namespace safe_browsing {

const size_t ClientSideDetectionHost::kMaxUrlsPerIP = 20;
const size_t ClientSideDetectionHost::kMaxIPsPerBrowse = 200;

typedef base::Callback<void(bool)> ShouldClassifyUrlCallback;

// This class is instantiated each time a new toplevel URL loads, and
// asynchronously checks whether the malware and phishing classifiers should run
// for this URL.  If so, it notifies the host class by calling the provided
// callback form the UI thread.  Objects of this class are ref-counted and will
// be destroyed once nobody uses it anymore.  If |web_contents|, |csd_service|
// or |host| go away you need to call Cancel().  We keep the |database_manager|
// alive in a ref pointer for as long as it takes.
class ClientSideDetectionHost::ShouldClassifyUrlRequest
    : public base::RefCountedThreadSafe<
          ClientSideDetectionHost::ShouldClassifyUrlRequest> {
 public:
  ShouldClassifyUrlRequest(
      content::NavigationHandle* navigation_handle,
      const ShouldClassifyUrlCallback& start_phishing_classification,
      const ShouldClassifyUrlCallback& start_malware_classification,
      WebContents* web_contents,
      ClientSideDetectionService* csd_service,
      SafeBrowsingDatabaseManager* database_manager,
      ClientSideDetectionHost* host)
      : web_contents_(web_contents),
        csd_service_(csd_service),
        database_manager_(database_manager),
        host_(host),
        start_phishing_classification_cb_(start_phishing_classification),
        start_malware_classification_cb_(start_malware_classification) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(web_contents_);
    DCHECK(csd_service_);
    DCHECK(database_manager_.get());
    DCHECK(host_);
    url_ = navigation_handle->GetURL();
    if (navigation_handle->GetResponseHeaders())
      navigation_handle->GetResponseHeaders()->GetMimeType(&mime_type_);
    remote_endpoint_ = navigation_handle->GetSocketAddress();
  }

  void Start() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // We start by doing some simple checks that can run on the UI thread.
    UMA_HISTOGRAM_BOOLEAN("SBClientPhishing.ClassificationStart", 1);
    UMA_HISTOGRAM_BOOLEAN("SBClientMalware.ClassificationStart", 1);

    // Only classify [X]HTML documents.
    if (mime_type_ != "text/html" && mime_type_ != "application/xhtml+xml") {
      DVLOG(1) << "Skipping phishing classification for URL: " << url_
               << " because it has an unsupported MIME type: "
               << mime_type_;
      DontClassifyForPhishing(NO_CLASSIFY_UNSUPPORTED_MIME_TYPE);
    }

    if (csd_service_->IsPrivateIPAddress(
            remote_endpoint_.ToStringWithoutPort())) {
      DVLOG(1) << "Skipping phishing classification for URL: " << url_
               << " because of hosting on private IP: "
               << remote_endpoint_.ToStringWithoutPort();
      DontClassifyForPhishing(NO_CLASSIFY_PRIVATE_IP);
      DontClassifyForMalware(NO_CLASSIFY_PRIVATE_IP);
    }

    // For phishing we only classify HTTP or HTTPS pages.
    if (!url_.SchemeIsHTTPOrHTTPS()) {
      DVLOG(1) << "Skipping phishing classification for URL: " << url_
               << " because it is not HTTP or HTTPS: "
               << remote_endpoint_.ToStringWithoutPort();
      DontClassifyForPhishing(NO_CLASSIFY_SCHEME_NOT_SUPPORTED);
    }

    // Don't run any classifier if the tab is incognito.
    if (web_contents_->GetBrowserContext()->IsOffTheRecord()) {
      DVLOG(1) << "Skipping phishing and malware classification for URL: "
               << url_ << " because we're browsing incognito.";
      DontClassifyForPhishing(NO_CLASSIFY_OFF_THE_RECORD);
      DontClassifyForMalware(NO_CLASSIFY_OFF_THE_RECORD);
    }

    // Don't start classification if |url_| is whitelisted by enterprise policy.
    Profile* profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext());
    if (profile && IsURLWhitelistedByPolicy(url_, *profile->GetPrefs())) {
      DontClassifyForPhishing(NO_CLASSIFY_WHITELISTED_BY_POLICY);
      DontClassifyForMalware(NO_CLASSIFY_WHITELISTED_BY_POLICY);
    }

    // We lookup the csd-whitelist before we lookup the cache because
    // a URL may have recently been whitelisted.  If the URL matches
    // the csd-whitelist we won't start phishing classification.  The
    // csd-whitelist check has to be done on the IO thread because it
    // uses the SafeBrowsing service class.
    if (ShouldClassifyForPhishing() || ShouldClassifyForMalware()) {
      base::PostTask(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(&ShouldClassifyUrlRequest::CheckSafeBrowsingDatabase,
                         this, url_));
    }
  }

  void Cancel() {
    DontClassifyForPhishing(NO_CLASSIFY_CANCEL);
    DontClassifyForMalware(NO_CLASSIFY_CANCEL);
    // Just to make sure we don't do anything stupid we reset all these
    // pointers except for the safebrowsing service class which may be
    // accessed by CheckSafeBrowsingDatabase().
    web_contents_ = NULL;
    csd_service_ = NULL;
    host_ = NULL;
  }

 private:
  friend class base::RefCountedThreadSafe<
      ClientSideDetectionHost::ShouldClassifyUrlRequest>;

  // Enum used to keep stats about why the pre-classification check failed.
  enum PreClassificationCheckFailures {
    OBSOLETE_NO_CLASSIFY_PROXY_FETCH = 0,
    NO_CLASSIFY_PRIVATE_IP = 1,
    NO_CLASSIFY_OFF_THE_RECORD = 2,
    NO_CLASSIFY_MATCH_CSD_WHITELIST = 3,
    NO_CLASSIFY_TOO_MANY_REPORTS = 4,
    NO_CLASSIFY_UNSUPPORTED_MIME_TYPE = 5,
    NO_CLASSIFY_NO_DATABASE_MANAGER = 6,
    NO_CLASSIFY_KILLSWITCH = 7,
    NO_CLASSIFY_CANCEL = 8,
    NO_CLASSIFY_RESULT_FROM_CACHE = 9,
    DEPRECATED_NO_CLASSIFY_NOT_HTTP_URL = 10,
    NO_CLASSIFY_SCHEME_NOT_SUPPORTED = 11,
    NO_CLASSIFY_WHITELISTED_BY_POLICY = 12,

    NO_CLASSIFY_MAX  // Always add new values before this one.
  };

  // The destructor can be called either from the UI or the IO thread.
  virtual ~ShouldClassifyUrlRequest() { }

  bool ShouldClassifyForPhishing() const {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return !start_phishing_classification_cb_.is_null();
  }

  bool ShouldClassifyForMalware() const {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return !start_malware_classification_cb_.is_null();
  }

  void DontClassifyForPhishing(PreClassificationCheckFailures reason) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (ShouldClassifyForPhishing()) {
      // Track the first reason why we stopped classifying for phishing.
      UMA_HISTOGRAM_ENUMERATION("SBClientPhishing.PreClassificationCheckFail",
                                reason, NO_CLASSIFY_MAX);
      DVLOG(2) << "Failed phishing pre-classification checks.  Reason: "
               << reason;
      start_phishing_classification_cb_.Run(false);
    }
    start_phishing_classification_cb_.Reset();
  }

  void DontClassifyForMalware(PreClassificationCheckFailures reason) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (ShouldClassifyForMalware()) {
      // Track the first reason why we stopped classifying for malware.
      UMA_HISTOGRAM_ENUMERATION("SBClientMalware.PreClassificationCheckFail",
                                reason, NO_CLASSIFY_MAX);
      DVLOG(2) << "Failed malware pre-classification checks.  Reason: "
               << reason;
      start_malware_classification_cb_.Run(false);
    }
    start_malware_classification_cb_.Reset();
  }

  void CheckSafeBrowsingDatabase(const GURL& url) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    PreClassificationCheckFailures phishing_reason = NO_CLASSIFY_MAX;
    PreClassificationCheckFailures malware_reason = NO_CLASSIFY_MAX;
    if (!database_manager_.get()) {
      // We cannot check the Safe Browsing whitelists so we stop here
      // for safety.
      OnWhitelistCheckDoneOnIO(url, NO_CLASSIFY_NO_DATABASE_MANAGER,
                               NO_CLASSIFY_NO_DATABASE_MANAGER,
                               /*match_whitelist=*/false);
      return;
    }

    // Query the CSD Whitelist asynchronously. We're already on the IO thread so
    // can call AllowlistCheckerClient directly.
    base::Callback<void(bool)> result_callback =
        base::Bind(&ClientSideDetectionHost::ShouldClassifyUrlRequest::
                       OnWhitelistCheckDoneOnIO,
                   this, url, phishing_reason, malware_reason);
    AllowlistCheckerClient::StartCheckCsdWhitelist(database_manager_, url,
                                                   result_callback);
  }

  void OnWhitelistCheckDoneOnIO(const GURL& url,
                                PreClassificationCheckFailures phishing_reason,
                                PreClassificationCheckFailures malware_reason,
                                bool match_whitelist) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    // We don't want to call the classification callbacks from the IO
    // thread so we simply pass the results of this method to CheckCache()
    // which is called on the UI thread;
    if (match_whitelist) {
      DVLOG(1) << "Skipping phishing classification for URL: " << url
               << " because it matches the csd whitelist";
      phishing_reason = NO_CLASSIFY_MATCH_CSD_WHITELIST;
    }
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&ShouldClassifyUrlRequest::CheckCache, this,
                                  phishing_reason, malware_reason));
  }

  void CheckCache(PreClassificationCheckFailures phishing_reason,
                  PreClassificationCheckFailures malware_reason) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (phishing_reason != NO_CLASSIFY_MAX)
      DontClassifyForPhishing(phishing_reason);
    if (malware_reason != NO_CLASSIFY_MAX)
      DontClassifyForMalware(malware_reason);
    if (!ShouldClassifyForMalware() && !ShouldClassifyForPhishing()) {
      return;  // No point in doing anything else.
    }
    // If result is cached, we don't want to run classification again.
    // In that case we're just trying to show the warning.
    bool is_phishing;
    if (csd_service_->GetValidCachedResult(url_, &is_phishing)) {
      DVLOG(1) << "Satisfying request for " << url_ << " from cache";
      UMA_HISTOGRAM_BOOLEAN("SBClientPhishing.RequestSatisfiedFromCache", 1);
      // Since we are already on the UI thread, this is safe.
      host_->MaybeShowPhishingWarning(url_, is_phishing);
      DontClassifyForPhishing(NO_CLASSIFY_RESULT_FROM_CACHE);
    }

    // We want to limit the number of requests, though we will ignore the
    // limit for urls in the cache.  We don't want to start classifying
    // too many pages as phishing, but for those that we already think are
    // phishing we want to send a request to the server to give ourselves
    // a chance to fix misclassifications.
    if (csd_service_->IsInCache(url_)) {
      DVLOG(1) << "Reporting limit skipped for " << url_
               << " as it was in the cache.";
      UMA_HISTOGRAM_BOOLEAN("SBClientPhishing.ReportLimitSkipped", 1);
    } else if (csd_service_->OverPhishingReportLimit()) {
      DVLOG(1) << "Too many report phishing requests sent recently, "
               << "not running classification for " << url_;
      DontClassifyForPhishing(NO_CLASSIFY_TOO_MANY_REPORTS);
    }
    if (csd_service_->OverMalwareReportLimit()) {
      DontClassifyForMalware(NO_CLASSIFY_TOO_MANY_REPORTS);
    }

    // Everything checks out, so start classification.
    // |web_contents_| is safe to call as we will be destructed
    // before it is.
    if (ShouldClassifyForPhishing()) {
      start_phishing_classification_cb_.Run(true);
      // Reset the callback to make sure ShouldClassifyForPhishing()
      // returns false.
      start_phishing_classification_cb_.Reset();
    }
    if (ShouldClassifyForMalware()) {
      start_malware_classification_cb_.Run(true);
      // Reset the callback to make sure ShouldClassifyForMalware()
      // returns false.
      start_malware_classification_cb_.Reset();
    }
  }

  GURL url_;
  std::string mime_type_;
  net::IPEndPoint remote_endpoint_;
  WebContents* web_contents_;
  ClientSideDetectionService* csd_service_;
  // We keep a ref pointer here just to make sure the safe browsing
  // database manager stays alive long enough.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  ClientSideDetectionHost* host_;

  ShouldClassifyUrlCallback start_phishing_classification_cb_;
  ShouldClassifyUrlCallback start_malware_classification_cb_;

  DISALLOW_COPY_AND_ASSIGN(ShouldClassifyUrlRequest);
};

// static
std::unique_ptr<ClientSideDetectionHost> ClientSideDetectionHost::Create(
    WebContents* tab) {
  return base::WrapUnique(new ClientSideDetectionHost(tab));
}

ClientSideDetectionHost::ClientSideDetectionHost(WebContents* tab)
    : content::WebContentsObserver(tab),
      csd_service_(nullptr),
      classification_request_(nullptr),
      should_extract_malware_features_(true),
      should_classify_for_malware_(false),
      pageload_complete_(false),
      unsafe_unique_page_id_(-1) {
  DCHECK(tab);
  // Note: csd_service_ and sb_service will be NULL here in testing.
  csd_service_ = g_browser_process->safe_browsing_detection_service();
  feature_extractor_.reset(new BrowserFeatureExtractor(tab, this));

  scoped_refptr<SafeBrowsingService> sb_service =
      g_browser_process->safe_browsing_service();
  if (sb_service.get()) {
    ui_manager_ = sb_service->ui_manager();
    database_manager_ = sb_service->database_manager();
    ui_manager_->AddObserver(this);
  }
}

ClientSideDetectionHost::~ClientSideDetectionHost() {
  if (ui_manager_.get())
    ui_manager_->RemoveObserver(this);
}

void ClientSideDetectionHost::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (browse_info_.get() && should_extract_malware_features_ &&
      navigation_handle->HasCommitted() && !navigation_handle->IsDownload() &&
      !navigation_handle->IsSameDocument()) {
    content::ResourceType resource_type =
        navigation_handle->IsInMainFrame() ? content::ResourceType::kMainFrame
                                           : content::ResourceType::kSubFrame;
    UpdateIPUrlMap(
        navigation_handle->GetSocketAddress().ToStringWithoutPort() /* ip */,
        navigation_handle->GetURL().spec() /* url */,
        navigation_handle->IsPost() ? "POST" : "GET",
        navigation_handle->GetReferrer().url.spec(), resource_type);
  }

  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  // TODO(noelutz): move this DCHECK to WebContents and fix all the unit tests
  // that don't call this method on the UI thread.
  // DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (navigation_handle->IsSameDocument()) {
    // If the navigation is within the same document, the user isn't really
    // navigating away.  We don't need to cancel a pending callback or
    // begin a new classification.
    return;
  }
  // Cancel any pending classification request.
  if (classification_request_.get()) {
    classification_request_->Cancel();
  }
  // If we navigate away and there currently is a pending phishing
  // report request we have to cancel it to make sure we don't display
  // an interstitial for the wrong page.  Note that this won't cancel
  // the server ping back but only cancel the showing of the
  // interstitial.
  weak_factory_.InvalidateWeakPtrs();

  if (!csd_service_) {
    return;
  }
  browse_info_.reset(new BrowseInfo);

  // Store redirect chain information.
  if (navigation_handle->GetURL().host() != cur_host_) {
    cur_host_ = navigation_handle->GetURL().host();
    cur_host_redirects_ = navigation_handle->GetRedirectChain();
  }
  browse_info_->url = navigation_handle->GetURL();
  browse_info_->host_redirects = cur_host_redirects_;
  browse_info_->url_redirects = navigation_handle->GetRedirectChain();
  browse_info_->referrer = navigation_handle->GetReferrer().url;
  if (navigation_handle->GetResponseHeaders()) {
    browse_info_->http_status_code =
        navigation_handle->GetResponseHeaders()->response_code();
  }

  should_extract_malware_features_ = true;
  should_classify_for_malware_ = false;
  pageload_complete_ = false;

  // Check whether we can cassify the current URL for phishing or malware.
  classification_request_ = new ShouldClassifyUrlRequest(
      navigation_handle,
      base::Bind(&ClientSideDetectionHost::OnPhishingPreClassificationDone,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ClientSideDetectionHost::OnMalwarePreClassificationDone,
                 weak_factory_.GetWeakPtr()),
      web_contents(), csd_service_, database_manager_.get(), this);
  classification_request_->Start();
}

void ClientSideDetectionHost::ResourceLoadComplete(
    content::RenderFrameHost* render_frame_host,
    const content::GlobalRequestID& request_id,
    const content::mojom::ResourceLoadInfo& resource_load_info) {
  if (!content::IsResourceTypeFrame(resource_load_info.resource_type) &&
      browse_info_.get() && should_extract_malware_features_ &&
      resource_load_info.url.is_valid() &&
      resource_load_info.network_info->remote_endpoint.has_value()) {
    UpdateIPUrlMap(
        resource_load_info.network_info->remote_endpoint->ToStringWithoutPort(),
        resource_load_info.url.spec(), resource_load_info.method,
        resource_load_info.referrer.spec(), resource_load_info.resource_type);
  }
}

void ClientSideDetectionHost::OnSafeBrowsingHit(
    const security_interstitials::UnsafeResource& resource) {
  if (!web_contents())
    return;

  // Check that the hit is either malware or phishing.
  if (resource.threat_type != SB_THREAT_TYPE_URL_PHISHING &&
      resource.threat_type != SB_THREAT_TYPE_URL_MALWARE)
    return;

  // Check that this notification is really for us.
  if (web_contents() != resource.web_contents_getter.Run())
    return;

  NavigationEntry* entry = resource.GetNavigationEntryForResource();
  if (!entry)
    return;

  // Store the unique page ID for later.
  unsafe_unique_page_id_ = entry->GetUniqueID();

  // We also keep the resource around in order to be able to send the
  // malicious URL to the server.
  unsafe_resource_.reset(new security_interstitials::UnsafeResource(resource));
  unsafe_resource_->callback.Reset();  // Don't do anything stupid.
}

scoped_refptr<SafeBrowsingDatabaseManager>
ClientSideDetectionHost::database_manager() {
  return database_manager_;
}

void ClientSideDetectionHost::WebContentsDestroyed() {
  // Tell any pending classification request that it is being canceled.
  if (classification_request_.get()) {
    classification_request_->Cancel();
  }
  // Cancel all pending feature extractions.
  feature_extractor_.reset();
}

void ClientSideDetectionHost::OnPhishingPreClassificationDone(
    bool should_classify) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (browse_info_.get() && should_classify) {
    DVLOG(1) << "Instruct renderer to start phishing detection for URL: "
             << browse_info_->url;
    content::RenderFrameHost* rfh = web_contents()->GetMainFrame();
    phishing_detector_.reset();
    rfh->GetRemoteInterfaces()->GetInterface(
        phishing_detector_.BindNewPipeAndPassReceiver());
    phishing_detector_->StartPhishingDetection(
        browse_info_->url,
        base::BindRepeating(&ClientSideDetectionHost::PhishingDetectionDone,
                            weak_factory_.GetWeakPtr()));
  }
}

void ClientSideDetectionHost::OnMalwarePreClassificationDone(
    bool should_classify) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If classification checks failed we should stop extracting malware features.
  DVLOG(2) << "Malware pre-classification checks done. Should classify: "
           << should_classify;
  should_extract_malware_features_ = should_classify;
  should_classify_for_malware_ = should_classify;
  MaybeStartMalwareFeatureExtraction();
}

void ClientSideDetectionHost::DidStopLoading() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!csd_service_ || !browse_info_.get())
    return;
  DVLOG(2) << "Page finished loading.";
  pageload_complete_ = true;
  MaybeStartMalwareFeatureExtraction();
}

void ClientSideDetectionHost::MaybeStartMalwareFeatureExtraction() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (csd_service_ && browse_info_.get() &&
      should_classify_for_malware_ &&
      pageload_complete_) {
    std::unique_ptr<ClientMalwareRequest> malware_request(
        new ClientMalwareRequest);
    // Start browser-side malware feature extraction.  Once we're done it will
    // send the malware client verdict request.
    malware_request->set_url(browse_info_->url.spec());
    const GURL& referrer = browse_info_->referrer;
    if (referrer.SchemeIs("http")) {  // Only send http urls.
      malware_request->set_referrer_url(referrer.spec());
    }
    // This function doesn't expect browse_info_ to stay around after this
    // function returns.
    feature_extractor_->ExtractMalwareFeatures(
        browse_info_.get(), std::move(malware_request),
        base::Bind(&ClientSideDetectionHost::MalwareFeatureExtractionDone,
                   weak_factory_.GetWeakPtr()));
    should_classify_for_malware_ = false;
  }
}

void ClientSideDetectionHost::PhishingDetectionDone(
    mojom::PhishingDetectorResult result,
    const std::string& verdict_str) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // There is something seriously wrong if there is no service class but
  // this method is called.  The renderer should not start phishing detection
  // if there isn't any service class in the browser.
  DCHECK(csd_service_);
  DCHECK(browse_info_.get());

  UMA_HISTOGRAM_ENUMERATION("SBClientPhishing.PhishingDetectorResult", result);
  if (result != mojom::PhishingDetectorResult::SUCCESS)
    return;

  // We parse the protocol buffer here.  If we're unable to parse it we won't
  // send the verdict further.
  std::unique_ptr<ClientPhishingRequest> verdict(new ClientPhishingRequest);
  if (csd_service_ &&
      browse_info_.get() &&
      verdict->ParseFromString(verdict_str) &&
      verdict->IsInitialized()) {
    // We only send phishing verdict to the server if the verdict is phishing or
    // if a SafeBrowsing interstitial was already shown for this site.  E.g., a
    // malware or phishing interstitial was shown but the user clicked
    // through.
    if (verdict->is_phishing() || DidShowSBInterstitial()) {
      if (DidShowSBInterstitial()) {
        browse_info_->unsafe_resource = std::move(unsafe_resource_);
      }
      // Start browser-side feature extraction.  Once we're done it will send
      // the client verdict request.
      feature_extractor_->ExtractFeatures(
          browse_info_.get(), std::move(verdict),
          base::Bind(&ClientSideDetectionHost::FeatureExtractionDone,
                     weak_factory_.GetWeakPtr()));
    }
  }
}

void ClientSideDetectionHost::MaybeShowPhishingWarning(GURL phishing_url,
                                                       bool is_phishing) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (is_phishing) {
    DCHECK(web_contents());
    if (ui_manager_.get()) {
      security_interstitials::UnsafeResource resource;
      resource.url = phishing_url;
      resource.original_url = phishing_url;
      resource.is_subresource = false;
      resource.threat_type = SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING;
      resource.threat_source =
          safe_browsing::ThreatSource::CLIENT_SIDE_DETECTION;
      resource.web_contents_getter = safe_browsing::SafeBrowsingUIManager::
          UnsafeResource::GetWebContentsGetter(
              web_contents()->GetMainFrame()->GetProcess()->GetID(),
              web_contents()->GetMainFrame()->GetRoutingID());
      if (!ui_manager_->IsWhitelisted(resource)) {
        // We need to stop any pending navigations, otherwise the interstitial
        // might not get created properly.
        web_contents()->GetController().DiscardNonCommittedEntries();
      }
      ui_manager_->DisplayBlockingPage(resource);
    }
    // If there is true phishing verdict, invalidate weakptr so that no longer
    // consider the malware vedict.
    weak_factory_.InvalidateWeakPtrs();
  }
}

void ClientSideDetectionHost::MaybeShowMalwareWarning(GURL original_url,
                                                      GURL malware_url,
                                                      bool is_malware) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(2) << "Received server malawre IP verdict for URL:" << malware_url
           << " is_malware:" << is_malware;
  UMA_HISTOGRAM_BOOLEAN(
      "SBClientMalware.ServerDeterminesMalware",
      is_malware);
  if (is_malware && malware_url.is_valid() && original_url.is_valid()) {
    DCHECK(web_contents());
    if (ui_manager_.get()) {
      security_interstitials::UnsafeResource resource;
      resource.url = malware_url;
      resource.original_url = original_url;
      resource.is_subresource = (malware_url.host() != original_url.host());
      resource.threat_type = SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE;
      resource.threat_source =
          safe_browsing::ThreatSource::CLIENT_SIDE_DETECTION;
      resource.web_contents_getter = safe_browsing::SafeBrowsingUIManager::
          UnsafeResource::GetWebContentsGetter(
              web_contents()->GetMainFrame()->GetProcess()->GetID(),
              web_contents()->GetMainFrame()->GetRoutingID());

      if (!ui_manager_->IsWhitelisted(resource)) {
        // We need to stop any pending navigations, otherwise the interstitial
        // might not get created properly.
        web_contents()->GetController().DiscardNonCommittedEntries();
      }
      ui_manager_->DisplayBlockingPage(resource);
    }
    // If there is true malware verdict, invalidate weakptr so that no longer
    // consider the phishing vedict.
    weak_factory_.InvalidateWeakPtrs();
  }
}

void ClientSideDetectionHost::FeatureExtractionDone(
    bool success,
    std::unique_ptr<ClientPhishingRequest> request) {
  DCHECK(request);
  DVLOG(2) << "Feature extraction done (success:" << success << ") for URL: "
           << request->url() << ". Start sending client phishing request.";
  ClientSideDetectionService::ClientReportPhishingRequestCallback callback;
  // If the client-side verdict isn't phishing we don't care about the server
  // response because we aren't going to display a warning.
  if (request->is_phishing()) {
    callback = base::Bind(&ClientSideDetectionHost::MaybeShowPhishingWarning,
                          weak_factory_.GetWeakPtr());
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  // Send ping even if the browser feature extraction failed.
  csd_service_->SendClientReportPhishingRequest(
      request.release(),  // The service takes ownership of the request object.
      IsExtendedReportingEnabled(*profile->GetPrefs()), callback);
}

void ClientSideDetectionHost::MalwareFeatureExtractionDone(
    bool feature_extraction_success,
    std::unique_ptr<ClientMalwareRequest> request) {
  DCHECK(request.get());
  DVLOG(2) << "Malware Feature extraction done for URL: " << request->url()
           << ", with badip url count:" << request->bad_ip_url_info_size();
  UMA_HISTOGRAM_BOOLEAN(
      "SBClientMalware.ResourceUrlMatchedBadIp",
      request->bad_ip_url_info_size() > 0);
  // Send ping if there is matching features.
  if (feature_extraction_success && request->bad_ip_url_info_size() > 0) {
    DVLOG(1) << "Start sending client malware request.";
    ClientSideDetectionService::ClientReportMalwareRequestCallback callback;
    callback = base::Bind(&ClientSideDetectionHost::MaybeShowMalwareWarning,
                          weak_factory_.GetWeakPtr());
    csd_service_->SendClientReportMalwareRequest(request.release(), callback);
  }
}

void ClientSideDetectionHost::UpdateIPUrlMap(const std::string& ip,
                                             const std::string& url,
                                             const std::string& method,
                                             const std::string& referrer,
                                             const ResourceType resource_type) {
  if (ip.empty() || url.empty())
    return;

  auto it = browse_info_->ips.find(ip);
  if (it == browse_info_->ips.end()) {
    if (browse_info_->ips.size() < kMaxIPsPerBrowse) {
      std::vector<IPUrlInfo> url_infos;
      url_infos.push_back(IPUrlInfo(url, method, referrer, resource_type));
      browse_info_->ips.insert(make_pair(ip, url_infos));
    }
  } else if (it->second.size() < kMaxUrlsPerIP) {
    it->second.push_back(IPUrlInfo(url, method, referrer, resource_type));
  }
}

bool ClientSideDetectionHost::DidShowSBInterstitial() const {
  if (unsafe_unique_page_id_ <= 0 || !web_contents()) {
    return false;
  }
  // DidShowSBInterstitial is called after client side detection is finished to
  // see if a SB interstitial was shown on the same page. Client Side Detection
  // only runs on the currently committed page, so an unconditional
  // GetLastCommittedEntry is correct here. GetNavigationEntryForResource cannot
  // be used since it may no longer be valid (eg, if the UnsafeResource was for
  // a blocking main page load which was then proceeded through).
  NavigationEntry* nav_entry =
      web_contents()->GetController().GetLastCommittedEntry();
  return (nav_entry && nav_entry->GetUniqueID() == unsafe_unique_page_id_);
}

void ClientSideDetectionHost::set_client_side_detection_service(
    ClientSideDetectionService* service) {
  csd_service_ = service;
}

void ClientSideDetectionHost::set_safe_browsing_managers(
    SafeBrowsingUIManager* ui_manager,
    SafeBrowsingDatabaseManager* database_manager) {
  if (ui_manager_.get())
    ui_manager_->RemoveObserver(this);

  ui_manager_ = ui_manager;
  if (ui_manager)
    ui_manager_->AddObserver(this);

  database_manager_ = database_manager;
}

}  // namespace safe_browsing

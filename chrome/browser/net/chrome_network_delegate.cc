// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include <stddef.h>
#include <stdlib.h>

#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/net/chrome_extensions_network_delegate.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/data_usage/core/data_use_aggregator.h"
#include "components/domain_reliability/monitor.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/resource_type.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/url_request/url_request.h"

#if defined(OS_ANDROID)
#include "base/android/path_utils.h"
#include "chrome/browser/io_thread.h"

#include "chrome/browser/android/adblock/adblock_bridge.h"
#include "AdblockPlus.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#include "chrome/common/chrome_switches.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

using content::BrowserThread;
using content::RenderViewHost;
using content::ResourceRequestInfo;

namespace {

bool g_access_to_all_files_enabled = false;

// Gets called when the extensions finish work on the URL. If the extensions
// did not do a redirect (so |new_url| is empty) then we enforce the
// SafeSearch parameters. Otherwise we will get called again after the
// redirect and we enforce SafeSearch then.
void ForceGoogleSafeSearchCallbackWrapper(
    const net::CompletionCallback& callback,
    net::URLRequest* request,
    GURL* new_url,
    int rv) {
  if (rv == net::OK && new_url->is_empty())
    safe_search_util::ForceGoogleSafeSearch(request, new_url);
  callback.Run(rv);
}

void ReportInvalidReferrerSendOnUI() {
  base::RecordAction(
      base::UserMetricsAction("Net.URLRequest_StartJob_InvalidReferrer"));
}

void ReportInvalidReferrerSend(const GURL& target_url,
                               const GURL& referrer_url) {
  LOG(ERROR) << "Cancelling request to " << target_url
             << " with invalid referrer " << referrer_url;
  // Record information to help debug http://crbug.com/422871
  if (!target_url.SchemeIsHTTPOrHTTPS())
    return;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(&ReportInvalidReferrerSendOnUI));
  base::debug::DumpWithoutCrashing();
  NOTREACHED();
}

// Record network errors that HTTP requests complete with, including OK and
// ABORTED.
void RecordNetworkErrorHistograms(const net::URLRequest* request,
                                  int net_error) {
  if (request->url().SchemeIs("http")) {
    base::UmaHistogramSparse("Net.HttpRequestCompletionErrorCodes",
                             std::abs(net_error));

    if (request->load_flags() & net::LOAD_MAIN_FRAME_DEPRECATED) {
      base::UmaHistogramSparse("Net.HttpRequestCompletionErrorCodes.MainFrame",
                               std::abs(net_error));
    }
  }
}

bool IsAccessAllowedInternal(const base::FilePath& path,
                             const base::FilePath& profile_path) {
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  return true;
#else

  std::vector<base::FilePath> whitelist;
#if defined(OS_CHROMEOS)
  // Use a whitelist to only allow access to files residing in the list of
  // directories below.
  static const base::FilePath::CharType* const kLocalAccessWhiteList[] = {
      "/home/chronos/user/Downloads",
      "/home/chronos/user/log",
      "/home/chronos/user/WebRTC Logs",
      "/media",
      "/opt/oem",
      "/usr/share/chromeos-assets",
      "/var/log",
  };

  base::FilePath temp_dir;
  if (base::PathService::Get(base::DIR_TEMP, &temp_dir))
    whitelist.push_back(temp_dir);

  // The actual location of "/home/chronos/user/Xyz" is the Xyz directory under
  // the profile path ("/home/chronos/user' is a hard link to current primary
  // logged in profile.) For the support of multi-profile sessions, we are
  // switching to use explicit "$PROFILE_PATH/Xyz" path and here whitelist such
  // access.
  if (!profile_path.empty()) {
    const base::FilePath downloads = profile_path.AppendASCII("Downloads");
    whitelist.push_back(downloads);
    const base::FilePath webrtc_logs = profile_path.AppendASCII("WebRTC Logs");
    whitelist.push_back(webrtc_logs);
  }
#elif defined(OS_ANDROID)
  // Access to files in external storage is allowed.
  base::FilePath external_storage_path;
  base::PathService::Get(base::DIR_ANDROID_EXTERNAL_STORAGE,
                         &external_storage_path);
  if (external_storage_path.IsParent(path))
    return true;

  auto all_download_dirs = base::android::GetAllPrivateDownloadsDirectories();
  for (const auto& dir : all_download_dirs)
    whitelist.push_back(dir);

  // Whitelist of other allowed directories.
  static const base::FilePath::CharType* const kLocalAccessWhiteList[] = {
      "/sdcard", "/mnt/sdcard",
  };
#endif

  for (const auto* whitelisted_path : kLocalAccessWhiteList)
    whitelist.push_back(base::FilePath(whitelisted_path));

  for (const auto& whitelisted_path : whitelist) {
    // base::FilePath::operator== should probably handle trailing separators.
    if (whitelisted_path == path.StripTrailingSeparators() ||
        whitelisted_path.IsParent(path)) {
      return true;
    }
  }

  DVLOG(1) << "File access denied - " << path.value().c_str();
  return false;
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
}

}  // namespace

ChromeNetworkDelegate::ChromeNetworkDelegate(
    extensions::EventRouterForwarder* event_router,
    BooleanPrefMember* enable_referrers)
    : profile_(nullptr),
      enable_referrers_(enable_referrers),
      enable_adblock_(nullptr),
      adblock_whitelisted_domains_(nullptr),
      force_google_safe_search_(nullptr),
      force_youtube_restrict_(nullptr),
      allowed_domains_for_apps_(nullptr),
      experimental_web_platform_features_enabled_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableExperimentalWebPlatformFeatures)),
      data_use_aggregator_(nullptr),
      is_data_usage_off_the_record_(true) {
  DCHECK(enable_referrers);
  extensions_delegate_.reset(
      ChromeExtensionsNetworkDelegate::Create(event_router));
}

ChromeNetworkDelegate::~ChromeNetworkDelegate() {}

void ChromeNetworkDelegate::set_extension_info_map(
    extensions::InfoMap* extension_info_map) {
  extensions_delegate_->set_extension_info_map(extension_info_map);
}

void ChromeNetworkDelegate::set_profile(void* profile) {
  profile_ = profile;
  extensions_delegate_->set_profile(profile);
}

void ChromeNetworkDelegate::set_cookie_settings(
    content_settings::CookieSettings* cookie_settings) {
  cookie_settings_ = cookie_settings;
}

void ChromeNetworkDelegate::set_data_use_aggregator(
    data_usage::DataUseAggregator* data_use_aggregator,
    bool is_data_usage_off_the_record) {
  data_use_aggregator_ = data_use_aggregator;
  is_data_usage_off_the_record_ = is_data_usage_off_the_record;
}

// static
void ChromeNetworkDelegate::InitializePrefsOnUIThread(
    BooleanPrefMember* enable_referrers,
    BooleanPrefMember* enable_adblock,
    StringListPrefMember* adblock_whitelisted_domains,
    BooleanPrefMember* force_google_safe_search,
    IntegerPrefMember* force_youtube_restrict,
    StringPrefMember* allowed_domains_for_apps,
    PrefService* pref_service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  enable_referrers->Init(prefs::kEnableReferrers, pref_service);
  enable_referrers->MoveToThread(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));

  if (enable_adblock) {
    enable_adblock->Init(prefs::kEnableAdblock, pref_service);
    enable_adblock->MoveToThread(
        BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));
  }

  if (adblock_whitelisted_domains) {
    adblock_whitelisted_domains->Init(prefs::kAdblockWhitelistedDomains, pref_service);
    adblock_whitelisted_domains->MoveToThread(
        BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));
  }

  if (force_google_safe_search) {
    force_google_safe_search->Init(prefs::kForceGoogleSafeSearch, pref_service);
    force_google_safe_search->MoveToThread(
        BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));
  }
  if (force_youtube_restrict) {
    force_youtube_restrict->Init(prefs::kForceYouTubeRestrict, pref_service);
    force_youtube_restrict->MoveToThread(
        BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));
  }
  if (allowed_domains_for_apps) {
    allowed_domains_for_apps->Init(prefs::kAllowedDomainsForApps, pref_service);
    allowed_domains_for_apps->MoveToThread(
        BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));
  }
}

int ChromeNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url) {

  extensions_delegate_->ForwardStartRequestStatus(request);

  if (!enable_referrers_->GetValue())
    request->SetReferrer(std::string());

  bool force_safe_search =
      (force_google_safe_search_ && force_google_safe_search_->GetValue());

  net::CompletionCallback wrapped_callback = callback;
  if (force_safe_search) {
    wrapped_callback = base::Bind(&ForceGoogleSafeSearchCallbackWrapper,
                                  callback,
                                  base::Unretained(request),
                                  base::Unretained(new_url));
  }

  int rv = extensions_delegate_->OnBeforeURLRequest(
      request, wrapped_callback, new_url);

  if (force_safe_search && rv == net::OK && new_url->is_empty())
    safe_search_util::ForceGoogleSafeSearch(request, new_url);

  if (allowed_domains_for_apps_ &&
      !allowed_domains_for_apps_->GetValue().empty() &&
      request->url().DomainIs("google.com")) {
    request->SetExtraRequestHeaderByName("X-GoogApps-Allowed-Domains",
                                         allowed_domains_for_apps_->GetValue(),
                                         true);
  }

  return rv;
}

int ChromeNetworkDelegate::OnBeforeStartTransaction(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpRequestHeaders* headers) {
  if (force_youtube_restrict_) {
    int value = force_youtube_restrict_->GetValue();
    static_assert(safe_search_util::YOUTUBE_RESTRICT_OFF == 0,
                  "OFF must be first");
    if (value > safe_search_util::YOUTUBE_RESTRICT_OFF &&
        value < safe_search_util::YOUTUBE_RESTRICT_COUNT) {
      safe_search_util::ForceYouTubeRestrict(request, headers,
          static_cast<safe_search_util::YouTubeRestrictMode>(value));
    }
  }

  // -----------------------------------------------------------------------

  LOG(WARNING) << "Adblock: OnBeforeStartTransaction";

  // check settings

  bool is_adblock_enabled = true;
  if (enable_adblock_) {
    is_adblock_enabled = enable_adblock_->GetValue();
  }

  LOG(WARNING) << "Adblock: isAdBlockEnabled = "
               << (is_adblock_enabled ? "true" : "false")
               << ", FilterEngine ptr = " << AdblockBridge::getFilterEnginePtr();

  const std::string filename = request->url().ExtractFileName();
  const std::string url = request->url().spec();
  LOG(WARNING) << "Adblock: loading url " << url;

  if (is_adblock_enabled && AdblockBridge::getFilterEnginePtr()) {
    // retain local filter engine to prevent usage of released instance if it's released on android/java side
    AdblockPlus::FilterEnginePtr* extFilterEngine =
      reinterpret_cast<AdblockPlus::FilterEnginePtr*>(AdblockBridge::getFilterEnginePtr());
    AdblockPlus::FilterEnginePtr filterEngine(*extFilterEngine);

    const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);

    LOG(WARNING) << "Adblock: casted to AdblockPlus::FilterEnginePtr, "
                 << "use_count = " << filterEngine.use_count();

    content::ResourceType resource_type;
    if (info) {
      resource_type = info->GetResourceType();
     bool isResourceTypeImage = (resource_type == content::RESOURCE_TYPE_IMAGE);
      LOG(WARNING) << "Adblock: resource type of " << url << " is " << info->GetResourceType()
                   << " (isImage = " << (isResourceTypeImage ? "true" : "false") << ")";
    } else {
      LOG(WARNING) << "Adblock: No resourceRequestInfo";

    }

    // check referrer (required for proper ad blocking)
    std::vector<std::string> documentUrls;
    std::string referrer;
    if (headers->GetHeader("Referer", &referrer)) {
      LOG(WARNING) << "Adblock: Referrer = " << referrer;
      documentUrls.push_back(referrer);
    } else {
      LOG(WARNING) << "Adblock: No referer";
    }

    if (info && info->IsMainFrame()
        && resource_type == content::RESOURCE_TYPE_MAIN_FRAME
        && documentUrls.size() == 0) {
      // pop-ups have also `content::RESOURCE_TYPE_MAIN_FRAME` resource type
      // but have referrer in contrast to truly main frames
      LOG(WARNING) << "Adblock: " << url << " is main frame, allow loading";
    } else {
      LOG(WARNING) << "Adblock: invoking IsDocumentWhitelisted(" << url << ")";
      if (filterEngine->IsDocumentWhitelisted(url, documentUrls)) {
        LOG(WARNING) << "Adblock: document whitelisted " << url;
      } else {
        AdblockPlus::FilterEngine::ContentType adblock_content_type;

        switch (resource_type) {
          case content::RESOURCE_TYPE_MAIN_FRAME:
            adblock_content_type = AdblockPlus::FilterEngine::ContentType::CONTENT_TYPE_GENERICBLOCK;
            break;

          case content::RESOURCE_TYPE_IMAGE:
          case content::RESOURCE_TYPE_FAVICON:
            adblock_content_type = AdblockPlus::FilterEngine::ContentType::CONTENT_TYPE_IMAGE;
            break;
            
          case content::RESOURCE_TYPE_XHR:
            adblock_content_type = AdblockPlus::FilterEngine::ContentType::CONTENT_TYPE_XMLHTTPREQUEST;
            break;

          case content::RESOURCE_TYPE_STYLESHEET:
            adblock_content_type = AdblockPlus::FilterEngine::ContentType::CONTENT_TYPE_STYLESHEET;
            break;

          case content::RESOURCE_TYPE_SCRIPT:
            adblock_content_type = AdblockPlus::FilterEngine::ContentType::CONTENT_TYPE_SCRIPT;
            break;

          case content::RESOURCE_TYPE_FONT_RESOURCE:
            adblock_content_type = AdblockPlus::FilterEngine::ContentType::CONTENT_TYPE_FONT;
            break;

          case content::RESOURCE_TYPE_OBJECT:
            adblock_content_type = AdblockPlus::FilterEngine::ContentType::CONTENT_TYPE_OBJECT;
            break;

          case content::RESOURCE_TYPE_SUB_FRAME:
            adblock_content_type = AdblockPlus::FilterEngine::ContentType::CONTENT_TYPE_SUBDOCUMENT;
            break;

         case content::RESOURCE_TYPE_MEDIA:
            adblock_content_type = AdblockPlus::FilterEngine::ContentType::CONTENT_TYPE_MEDIA;
            break;

          default:
            adblock_content_type = AdblockPlus::FilterEngine::ContentType::CONTENT_TYPE_OTHER;
        }

        LOG(WARNING) << "Adblock: mapped to adblock content type " << adblock_content_type;

        AdblockPlus::FilterPtr filterPtr = filterEngine->Matches(url, adblock_content_type, documentUrls);
        if (filterPtr && filterPtr->GetType() != AdblockPlus::Filter::TYPE_EXCEPTION) {
          LOG(ERROR) << "Adblock: !!! Prevented loading " << url;

          // URL access blocked by Adblock Plus.
          request->net_log().AddEvent(
            net::NetLogEventType::CHROME_POLICY_ABORTED_REQUEST,
            net::NetLog::StringCallback("url", &request->url().possibly_invalid_spec()));

          return net::ERR_BLOCKED_BY_ADMINISTRATOR;
        }
      }
    }
  }

  LOG(WARNING) << "Adblock: exiting OnBeforeStartTransaction()";

  // -----------------------------------------------------------------------

  return extensions_delegate_->OnBeforeStartTransaction(request, callback,
                                                        headers);
}

void ChromeNetworkDelegate::OnStartTransaction(
    net::URLRequest* request,
    const net::HttpRequestHeaders& headers) {
  extensions_delegate_->OnStartTransaction(request, headers);
}

int ChromeNetworkDelegate::OnHeadersReceived(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  return extensions_delegate_->OnHeadersReceived(
      request,
      callback,
      original_response_headers,
      override_response_headers,
      allowed_unsafe_redirect_url);
}

void ChromeNetworkDelegate::OnBeforeRedirect(net::URLRequest* request,
                                             const GURL& new_location) {
  if (domain_reliability_monitor_)
    domain_reliability_monitor_->OnBeforeRedirect(request);
  extensions_delegate_->OnBeforeRedirect(request, new_location);
  variations::StripVariationHeaderIfNeeded(new_location, request);
}

void ChromeNetworkDelegate::OnResponseStarted(net::URLRequest* request,
                                              int net_error) {
  extensions_delegate_->OnResponseStarted(request, net_error);
}

void ChromeNetworkDelegate::OnNetworkBytesReceived(net::URLRequest* request,
                                                   int64_t bytes_received) {
#if !defined(OS_ANDROID)
  // Note: Currently, OnNetworkBytesReceived is only implemented for HTTP jobs,
  // not FTP or other types, so those kinds of bytes will not be reported here.
  task_manager::TaskManagerInterface::OnRawBytesRead(*request, bytes_received);
#endif  // !defined(OS_ANDROID)

  ReportDataUsageStats(request, 0 /* tx_bytes */, bytes_received);
}

void ChromeNetworkDelegate::OnNetworkBytesSent(net::URLRequest* request,
                                               int64_t bytes_sent) {
#if !defined(OS_ANDROID)
  // Note: Currently, OnNetworkBytesSent is only implemented for HTTP jobs,
  // not FTP or other types, so those kinds of bytes will not be reported here.
  task_manager::TaskManagerInterface::OnRawBytesSent(*request, bytes_sent);
#endif  // !defined(OS_ANDROID)

  ReportDataUsageStats(request, bytes_sent, 0 /* rx_bytes */);
}

void ChromeNetworkDelegate::OnCompleted(net::URLRequest* request,
                                        bool started,
                                        int net_error) {
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  // TODO(amohammadkhan): Verify that there is no double recording in data use
  // of redirected requests.
  RecordNetworkErrorHistograms(request, net_error);

  extensions_delegate_->OnCompleted(request, started, net_error);
  if (domain_reliability_monitor_)
    domain_reliability_monitor_->OnCompleted(request, started);
  extensions_delegate_->ForwardProxyErrors(request, net_error);
  extensions_delegate_->ForwardDoneRequestStatus(request);
}

void ChromeNetworkDelegate::OnURLRequestDestroyed(net::URLRequest* request) {
  extensions_delegate_->OnURLRequestDestroyed(request);
}

void ChromeNetworkDelegate::OnPACScriptError(int line_number,
                                             const base::string16& error) {
  extensions_delegate_->OnPACScriptError(line_number, error);
}

net::NetworkDelegate::AuthRequiredResponse
ChromeNetworkDelegate::OnAuthRequired(
    net::URLRequest* request,
    const net::AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    net::AuthCredentials* credentials) {
  return extensions_delegate_->OnAuthRequired(
      request, auth_info, callback, credentials);
}

bool ChromeNetworkDelegate::OnCanGetCookies(
    const net::URLRequest& request,
    const net::CookieList& cookie_list) {
  // nullptr during tests, or when we're running in the system context.
  if (!cookie_settings_.get())
    return true;

  bool allow = cookie_settings_->IsCookieAccessAllowed(
      request.url(), request.site_for_cookies());

  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(&request);
  if (info) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&TabSpecificContentSettings::CookiesRead,
                       info->GetWebContentsGetterForRequest(), request.url(),
                       request.site_for_cookies(), cookie_list, !allow));
  }

  return allow;
}

bool ChromeNetworkDelegate::OnCanSetCookie(const net::URLRequest& request,
                                           const net::CanonicalCookie& cookie,
                                           net::CookieOptions* options) {
  // nullptr during tests, or when we're running in the system context.
  if (!cookie_settings_.get())
    return true;

  bool allow = cookie_settings_->IsCookieAccessAllowed(
      request.url(), request.site_for_cookies());

  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(&request);
  if (info) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&TabSpecificContentSettings::CookieChanged,
                       info->GetWebContentsGetterForRequest(), request.url(),
                       request.site_for_cookies(), cookie, *options, !allow));
  }

  return allow;
}

bool ChromeNetworkDelegate::OnCanAccessFile(
    const net::URLRequest& request,
    const base::FilePath& original_path,
    const base::FilePath& absolute_path) const {
  if (g_access_to_all_files_enabled)
    return true;
  return IsAccessAllowed(original_path, absolute_path, profile_path_);
}

// static
bool ChromeNetworkDelegate::IsAccessAllowed(
    const base::FilePath& path,
    const base::FilePath& profile_path) {
  return IsAccessAllowedInternal(path, profile_path);
}

// static
bool ChromeNetworkDelegate::IsAccessAllowed(
    const base::FilePath& path,
    const base::FilePath& absolute_path,
    const base::FilePath& profile_path) {
#if defined(OS_ANDROID)
  // Android's whitelist relies on symbolic links (ex. /sdcard is whitelisted
  // and commonly a symbolic link), thus do not check absolute paths.
  return IsAccessAllowedInternal(path, profile_path);
#else
  return (IsAccessAllowedInternal(path, profile_path) &&
          IsAccessAllowedInternal(absolute_path, profile_path));
#endif
}

// static
void ChromeNetworkDelegate::EnableAccessToAllFilesForTesting(bool enabled) {
  g_access_to_all_files_enabled = enabled;
}

bool ChromeNetworkDelegate::OnCanEnablePrivacyMode(
    const GURL& url,
    const GURL& site_for_cookies) const {
  // nullptr during tests, or when we're running in the system context.
  if (!cookie_settings_.get())
    return false;

  return !cookie_settings_->IsCookieAccessAllowed(url, site_for_cookies);
}

bool ChromeNetworkDelegate::OnAreExperimentalCookieFeaturesEnabled() const {
  return experimental_web_platform_features_enabled_;
}

bool ChromeNetworkDelegate::OnCancelURLRequestWithPolicyViolatingReferrerHeader(
    const net::URLRequest& request,
    const GURL& target_url,
    const GURL& referrer_url) const {
  ReportInvalidReferrerSend(target_url, referrer_url);
  return true;
}

bool ChromeNetworkDelegate::OnCanQueueReportingReport(
    const url::Origin& origin) const {
  if (!cookie_settings_)
    return false;

  return cookie_settings_->IsCookieAccessAllowed(origin.GetURL(),
                                                 origin.GetURL());
}

void ChromeNetworkDelegate::OnCanSendReportingReports(
    std::set<url::Origin> origins,
    base::OnceCallback<void(std::set<url::Origin>)> result_callback) const {
  if (!reporting_permissions_checker_) {
    origins.clear();
    std::move(result_callback).Run(std::move(origins));
    return;
  }

  reporting_permissions_checker_->FilterReportingOrigins(
      std::move(origins), std::move(result_callback));
}

bool ChromeNetworkDelegate::OnCanSetReportingClient(
    const url::Origin& origin,
    const GURL& endpoint) const {
  if (!cookie_settings_)
    return false;

  return cookie_settings_->IsCookieAccessAllowed(endpoint, origin.GetURL());
}

bool ChromeNetworkDelegate::OnCanUseReportingClient(
    const url::Origin& origin,
    const GURL& endpoint) const {
  if (!cookie_settings_)
    return false;

  return cookie_settings_->IsCookieAccessAllowed(endpoint, origin.GetURL());
}

void ChromeNetworkDelegate::ReportDataUsageStats(net::URLRequest* request,
                                                 int64_t tx_bytes,
                                                 int64_t rx_bytes) {
  if (!data_use_aggregator_)
    return;

  if (is_data_usage_off_the_record_) {
    data_use_aggregator_->ReportOffTheRecordDataUse(tx_bytes, rx_bytes);
    return;
  }

  data_use_aggregator_->ReportDataUse(request, tx_bytes, rx_bytes);
}

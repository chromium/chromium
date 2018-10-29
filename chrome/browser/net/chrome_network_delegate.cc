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
#include "base/metrics/user_metrics.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
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
#include "components/domain_reliability/monitor.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/resource_type.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/host_port_pair.h"
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

bool IsAccessAllowedInternal(const base::FilePath& path,
                             const base::FilePath& profile_path) {
  if (g_access_to_all_files_enabled)
    return true;

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
      "/run/arc/sdcard/write/emulated/0",
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

#if defined(OS_CHROMEOS)
  // Allow access to DriveFS logs. These reside in
  // $PROFILE_PATH/GCache/v2/<opaque id>/Logs.
  base::FilePath path_within_gcache_v2;
  if (profile_path.Append("GCache/v2")
          .AppendRelativePath(path, &path_within_gcache_v2)) {
    std::vector<std::string> components;
    path_within_gcache_v2.GetComponents(&components);
    if (components.size() > 1 && components[1] == "Logs") {
      return true;
    }
  }
#endif  // defined(OS_CHROMEOS)

  DVLOG(1) << "File access denied - " << path.value().c_str();
  return false;
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
}

}  // namespace

ChromeNetworkDelegate::ChromeNetworkDelegate(
    extensions::EventRouterForwarder* event_router)
    : extensions_delegate_(
          ChromeExtensionsNetworkDelegate::Create(event_router)),
      profile_(nullptr),
      experimental_web_platform_features_enabled_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableExperimentalWebPlatformFeatures)) {}

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

int ChromeNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    net::CompletionOnceCallback callback,
    GURL* new_url) {
  extensions_delegate_->ForwardStartRequestStatus(request);
  return extensions_delegate_->NotifyBeforeURLRequest(
      request, std::move(callback), new_url);
}

int ChromeNetworkDelegate::OnBeforeStartTransaction(
    net::URLRequest* request,
    net::CompletionOnceCallback callback,
    net::HttpRequestHeaders* headers) {
  return extensions_delegate_->NotifyBeforeStartTransaction(
      request, std::move(callback), headers);
}

void ChromeNetworkDelegate::OnStartTransaction(
    net::URLRequest* request,
    const net::HttpRequestHeaders& headers) {
  extensions_delegate_->NotifyStartTransaction(request, headers);
}

int ChromeNetworkDelegate::OnHeadersReceived(
    net::URLRequest* request,
    net::CompletionOnceCallback callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  return extensions_delegate_->NotifyHeadersReceived(
      request, std::move(callback), original_response_headers,
      override_response_headers, allowed_unsafe_redirect_url);
}

void ChromeNetworkDelegate::OnBeforeRedirect(net::URLRequest* request,
                                             const GURL& new_location) {
  if (domain_reliability_monitor_)
    domain_reliability_monitor_->OnBeforeRedirect(request);
  extensions_delegate_->NotifyBeforeRedirect(request, new_location);
  variations::StripVariationHeaderIfNeeded(new_location, request);
}

void ChromeNetworkDelegate::OnResponseStarted(net::URLRequest* request,
                                              int net_error) {
  extensions_delegate_->NotifyResponseStarted(request, net_error);
}

void ChromeNetworkDelegate::OnNetworkBytesReceived(net::URLRequest* request,
                                                   int64_t bytes_received) {
#if !defined(OS_ANDROID)
  // Note: Currently, OnNetworkBytesReceived is only implemented for HTTP jobs,
  // not FTP or other types, so those kinds of bytes will not be reported here.
  task_manager::TaskManagerInterface::OnRawBytesRead(*request, bytes_received);
#endif  // !defined(OS_ANDROID)
}

void ChromeNetworkDelegate::OnNetworkBytesSent(net::URLRequest* request,
                                               int64_t bytes_sent) {
#if !defined(OS_ANDROID)
  // Note: Currently, OnNetworkBytesSent is only implemented for HTTP jobs,
  // not FTP or other types, so those kinds of bytes will not be reported here.
  task_manager::TaskManagerInterface::OnRawBytesSent(*request, bytes_sent);
#endif  // !defined(OS_ANDROID)
}

void ChromeNetworkDelegate::OnCompleted(net::URLRequest* request,
                                        bool started,
                                        int net_error) {
  extensions_delegate_->NotifyCompleted(request, started, net_error);
  if (domain_reliability_monitor_)
    domain_reliability_monitor_->OnCompleted(request, started);
  extensions_delegate_->ForwardDoneRequestStatus(request);
}

void ChromeNetworkDelegate::OnURLRequestDestroyed(net::URLRequest* request) {
  extensions_delegate_->NotifyURLRequestDestroyed(request);
}

net::NetworkDelegate::AuthRequiredResponse
ChromeNetworkDelegate::OnAuthRequired(net::URLRequest* request,
                                      const net::AuthChallengeInfo& auth_info,
                                      AuthCallback callback,
                                      net::AuthCredentials* credentials) {
  return extensions_delegate_->NotifyAuthRequired(
      request, auth_info, std::move(callback), credentials);
}

bool ChromeNetworkDelegate::OnCanGetCookies(const net::URLRequest& request,
                                            const net::CookieList& cookie_list,
                                            bool allowed_from_caller) {
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(&request);
  if (info) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&TabSpecificContentSettings::CookiesRead,
                       info->GetWebContentsGetterForRequest(), request.url(),
                       request.site_for_cookies(), cookie_list,
                       !allowed_from_caller));
  }
  return allowed_from_caller;
}

bool ChromeNetworkDelegate::OnCanSetCookie(const net::URLRequest& request,
                                           const net::CanonicalCookie& cookie,
                                           net::CookieOptions* options,
                                           bool allowed_from_caller) {
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(&request);
  if (info) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&TabSpecificContentSettings::CookieChanged,
                       info->GetWebContentsGetterForRequest(), request.url(),
                       request.site_for_cookies(), cookie,
                       !allowed_from_caller));
  }
  return allowed_from_caller;
}

bool ChromeNetworkDelegate::OnCanAccessFile(
    const net::URLRequest& request,
    const base::FilePath& original_path,
    const base::FilePath& absolute_path) const {
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

bool ChromeNetworkDelegate::OnCancelURLRequestWithPolicyViolatingReferrerHeader(
    const net::URLRequest& request,
    const GURL& target_url,
    const GURL& referrer_url) const {
  // These errors should be handled by the NetworkDelegate wrapper created by
  // the owning NetworkContext.
  NOTREACHED();
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

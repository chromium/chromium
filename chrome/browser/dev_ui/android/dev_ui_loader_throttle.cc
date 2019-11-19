// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dev_ui/android/dev_ui_loader_throttle.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "chrome/android/modules/dev_ui/provider/dev_ui_module_provider.h"
#include "chrome/browser/dev_ui/android/dev_ui_loader_error_page.h"
#include "chrome/common/webui_url_constants.h"
#include "components/safe_browsing/web_ui/constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace {

// Decides whether WebUI |host| (assumed for chrome://) is in the DevUI DFM.
bool IsWebUiHostInDevUiDfm(const std::string& host) {
  // Each WebUI host (including synonyms) in the DevUI DFM must have an entry.
  // Assume linear search is fast enough. Can optimize later if needed.
  return host == chrome::kChromeUIAccessibilityHost ||
         host == chrome::kChromeUIAutofillInternalsHost ||
         host == chrome::kChromeUIBluetoothInternalsHost ||
         host == chrome::kChromeUIComponentsHost ||
         host == chrome::kChromeUICrashesHost ||
         host == chrome::kChromeUIDeviceLogHost ||
         host == chrome::kChromeUIDomainReliabilityInternalsHost ||
         host == chrome::kChromeUIDownloadInternalsHost ||
         host == chrome::kChromeUIGCMInternalsHost ||
         host == chrome::kChromeUIInterstitialHost ||
         host == chrome::kChromeUIInterventionsInternalsHost ||
         host == chrome::kChromeUIInvalidationsHost ||
         host == chrome::kChromeUILocalStateHost ||
         host == chrome::kChromeUIMediaEngagementHost ||
         host == chrome::kChromeUIMemoryInternalsHost ||
         host == chrome::kChromeUINTPTilesInternalsHost ||
         host == chrome::kChromeUINetExportHost ||
         host == chrome::kChromeUINetInternalsHost ||
         host == chrome::kChromeUINotificationsInternalsHost ||
         host == chrome::kChromeUIOmniboxHost ||
         host == chrome::kChromeUIPasswordManagerInternalsHost ||
         host == chrome::kChromeUIPolicyHost ||
         host == chrome::kChromeUIPredictorsHost ||
         host == chrome::kChromeUIQuotaInternalsHost ||
         host == chrome::kChromeUISandboxHost ||
         host == chrome::kChromeUISignInInternalsHost ||
         host == chrome::kChromeUISiteEngagementHost ||
         host == chrome::kChromeUISnippetsInternalsHost ||
         host == chrome::kChromeUISuggestionsHost ||
         host == chrome::kChromeUISupervisedUserInternalsHost ||
         host == chrome::kChromeUISyncInternalsHost ||
         host == chrome::kChromeUITranslateInternalsHost ||
         host == chrome::kChromeUIUkmHost ||
         host == chrome::kChromeUIUsbInternalsHost ||
         host == chrome::kChromeUIUserActionsHost ||
         host == chrome::kChromeUIWebApksHost ||
         host == chrome::kChromeUIWebRtcLogsHost ||
         host == content::kChromeUIAppCacheInternalsHost ||
         host == content::kChromeUIBlobInternalsHost ||
         host == content::kChromeUIGpuHost ||
         host == content::kChromeUIHistogramHost ||
         host == content::kChromeUIIndexedDBInternalsHost ||
         host == content::kChromeUIMediaInternalsHost ||
         host == content::kChromeUINetworkErrorsListingHost ||
         host == content::kChromeUIProcessInternalsHost ||
         host == content::kChromeUIServiceWorkerInternalsHost ||
         host == content::kChromeUIWebRTCInternalsHost ||
         host == safe_browsing::kChromeUISafeBrowsingHost;
}

}  // namespace

namespace dev_ui {

// static
bool DevUiLoaderThrottle::ShouldInstallDevUiDfm(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         IsWebUiHostInDevUiDfm(url.host());
}

// static
std::unique_ptr<content::NavigationThrottle>
DevUiLoaderThrottle::MaybeCreateThrottleFor(content::NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(handle);
  if (!handle->IsInMainFrame())
    return nullptr;

  if (!ShouldInstallDevUiDfm(handle->GetURL()))
    return nullptr;

  // If module is already installed, ensure that it is loaded.
  if (dev_ui::DevUiModuleProvider::GetInstance()->ModuleInstalled()) {
    // Synchronously load module (if not already loaded).
    dev_ui::DevUiModuleProvider::GetInstance()->LoadModule();
    return nullptr;
  }

  return std::make_unique<DevUiLoaderThrottle>(handle);
}

DevUiLoaderThrottle::DevUiLoaderThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

DevUiLoaderThrottle::~DevUiLoaderThrottle() = default;

const char* DevUiLoaderThrottle::GetNameForLogging() {
  return "DevUiLoaderThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
DevUiLoaderThrottle::WillStartRequest() {
  if (!dev_ui::DevUiModuleProvider::GetInstance()->ModuleInstalled()) {
    // Can handle multiple install requests.
    dev_ui::DevUiModuleProvider::GetInstance()->InstallModule(
        base::BindOnce(&DevUiLoaderThrottle::OnDevUiDfmInstallWithStatus,
                       weak_ptr_factory_.GetWeakPtr()));
    return DEFER;
  }
  return PROCEED;
}

void DevUiLoaderThrottle::OnDevUiDfmInstallWithStatus(bool success) {
  if (success) {
    dev_ui::DevUiModuleProvider::GetInstance()->LoadModule();
    Resume();
  } else {
    std::string html = BuildErrorPageHtml();
    CancelDeferredNavigation({CANCEL, net::ERR_CONNECTION_FAILED, html});
  }
}

}  // namespace dev_ui

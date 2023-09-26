// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dev_ui/android/dev_ui_loader_throttle.h"

#include <string>
#include <utility>

#include "base/check_op.h"
#include "build/chromeos_buildflags.h"
#include "chrome/android/modules/dev_ui/provider/dev_ui_module_provider.h"
#include "chrome/browser/dev_ui/android/dev_ui_loader_error_page.h"
#include "chrome/common/webui_url_constants.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/history_clusters/history_clusters_internals/webui/url_constants.h"
#include "components/optimization_guide/optimization_guide_internals/webui/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/url_constants.h"
#include "device/vr/buildflags/buildflags.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace {

// Decides whether WebUI |host| (assumed for chrome://) is in the DevUI DFM.
// These should exclude WebUI hosts that are used outside of Chrome, e.g., those
// used by Android WebView (see AwWebUIControllerFactory). The test
// DevUiLoaderThrottleTest.PreventAccidentalInclusion safeguards against the
// inclusion of WebUI hosts that are purposefully left in the base module (i.e.,
// installed by default, and not in the DevUI DFM).
bool IsWebUiHostInDevUiDfm(const std::string& host) {
  // Each WebUI host (including synonyms) in the DevUI DFM must have an entry.
  // Assume linear search is fast enough. Can optimize later if needed.
  return host == chrome::kChromeUIAccessibilityHost ||
         host == chrome::kChromeUIAutofillInternalsHost ||
         host == chrome::kChromeUIBluetoothInternalsHost ||
         host == chrome::kChromeUIBrowsingTopicsInternalsHost ||
         host == chrome::kChromeUIComponentsHost ||
         host == chrome::kChromeUICrashesHost ||
         host == chrome::kChromeUIDeviceLogHost ||
         host == chrome::kChromeUIDownloadInternalsHost ||
         host == chrome::kChromeUIFamilyLinkUserInternalsHost ||
         host == chrome::kChromeUIGCMInternalsHost ||
         host == chrome::kChromeUIInternalsHost ||
         host == chrome::kChromeUIInterstitialHost ||
         host == chrome::kChromeUILocalStateHost ||
         host == chrome::kChromeUIMediaEngagementHost ||
         host == chrome::kChromeUIMemoryInternalsHost ||
         host == chrome::kChromeUIMetricsInternalsHost ||
         host == chrome::kChromeUINTPTilesInternalsHost ||
         host == chrome::kChromeUINetExportHost ||
         host == chrome::kChromeUINetInternalsHost ||
         host == chrome::kChromeUIOmniboxHost ||
         host == chrome::kChromeUIPasswordManagerInternalsHost ||
         host == chrome::kChromeUIPolicyHost ||
         host == chrome::kChromeUIPredictorsHost ||
         host == chrome::kChromeUISandboxHost ||
         host == chrome::kChromeUISignInInternalsHost ||
         host == chrome::kChromeUISiteEngagementHost ||
         host == chrome::kChromeUISnippetsInternalsHost ||
         host == chrome::kChromeUISyncInternalsHost ||
         host == chrome::kChromeUITranslateInternalsHost ||
         host == chrome::kChromeUIUsbInternalsHost ||
         host == chrome::kChromeUIUserActionsHost ||
         host == chrome::kChromeUIWebApksHost ||
         host == chrome::kChromeUIWebRtcLogsHost ||
         host == commerce::kChromeUICommerceInternalsHost ||
         host == content::kChromeUIPrivateAggregationInternalsHost ||
         host == content::kChromeUIAttributionInternalsHost ||
         host == content::kChromeUIBlobInternalsHost ||
         host == content::kChromeUIGpuHost ||
         host == content::kChromeUIHistogramHost ||
         host == content::kChromeUIIndexedDBInternalsHost ||
         host == content::kChromeUIMediaInternalsHost ||
         host == content::kChromeUINetworkErrorsListingHost ||
         host == content::kChromeUIProcessInternalsHost ||
         host == content::kChromeUIQuotaInternalsHost ||
         host == content::kChromeUIServiceWorkerInternalsHost ||
         host == content::kChromeUIUkmHost ||
         host == content::kChromeUIWebRTCInternalsHost ||
#if BUILDFLAG(ENABLE_VR)
         host == content::kChromeUIWebXrInternalsHost ||
#endif
         host == history_clusters_internals::
                     kChromeUIHistoryClustersInternalsHost ||
         host == optimization_guide_internals::
                     kChromeUIOptimizationGuideInternalsHost;
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
  if (!handle->IsInPrimaryMainFrame())
    return nullptr;

  if (!ShouldInstallDevUiDfm(handle->GetURL()))
    return nullptr;

  if (dev_ui::DevUiModuleProvider::GetInstance()->ModuleInstalled()) {
    dev_ui::DevUiModuleProvider::GetInstance()->EnsureLoaded();
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
    dev_ui::DevUiModuleProvider::GetInstance()->EnsureLoaded();
    Resume();
  } else {
    std::string html = BuildErrorPageHtml();
    CancelDeferredNavigation({CANCEL, net::ERR_CONNECTION_FAILED, html});
  }
}

}  // namespace dev_ui

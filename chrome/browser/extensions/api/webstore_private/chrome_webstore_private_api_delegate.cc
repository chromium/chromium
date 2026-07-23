// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webstore_private/chrome_webstore_private_api_delegate.h"

#include <memory>

#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/extensions/extension_allowlist_factory.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/policy/policy_ui_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "extensions/browser/extension_allowlist.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#endif

namespace extensions {

ChromeWebstorePrivateAPIDelegate::ChromeWebstorePrivateAPIDelegate() = default;

ChromeWebstorePrivateAPIDelegate::~ChromeWebstorePrivateAPIDelegate() = default;

std::vector<KeyedServiceBaseFactory*>
ChromeWebstorePrivateAPIDelegate::GetWebStoreAPIFactoryDependencies() {
  std::vector<KeyedServiceBaseFactory*> dependencies;
  dependencies.push_back(ExtensionAllowlistFactory::GetInstance());
  dependencies.push_back(IdentityManagerFactory::GetInstance());
  dependencies.push_back(InstallTrackerFactory::GetInstance());
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  dependencies.push_back(
      safe_browsing::SafeBrowsingMetricsCollectorFactory::GetInstance());
  dependencies.push_back(
      safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
          GetInstance());
#endif
  return dependencies;
}

ExtensionAllowlist* ChromeWebstorePrivateAPIDelegate::GetExtensionAllowlist(
    content::BrowserContext* context) {
  return ExtensionAllowlistFactory::GetForBrowserContext(context);
}

signin::IdentityManager* ChromeWebstorePrivateAPIDelegate::GetIdentityManager(
    content::BrowserContext* context) {
  return IdentityManagerFactory::GetForProfile(
      Profile::FromBrowserContext(context));
}

void ChromeWebstorePrivateAPIDelegate::ShowExtensionInstallBlockedDialog(
    content::WebContents* web_contents,
    const Extension* extension,
    const std::u16string& custom_error_message,
    const gfx::ImageSkia& icon,
    base::OnceClosure done_callback) {
  ::extensions::ShowExtensionInstallBlockedDialog(
      extension->id(), extension->name(), custom_error_message, icon,
      web_contents, std::move(done_callback));
}

void ChromeWebstorePrivateAPIDelegate::ShowExtensionInstallFrictionDialog(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> callback) {
  ::extensions::ShowExtensionInstallFrictionDialog(web_contents,
                                                   std::move(callback));
}

#if BUILDFLAG(IS_ANDROID)
void ChromeWebstorePrivateAPIDelegate::ShowExtensionInstallAskParentDialog(
    content::WebContents* web_contents,
    base::OnceClosure cancel_callback,
    base::OnceClosure approve_callback) {
  ::extensions::ShowExtensionInstallAskParentDialog(
      web_contents, std::move(cancel_callback), std::move(approve_callback));
}
#endif  // BUILDFLAG(IS_ANDROID)

void ChromeWebstorePrivateAPIDelegate::ReportFrictionAcceptedEvent(
    content::BrowserContext* context) {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  auto* metrics_collector =
      safe_browsing::SafeBrowsingMetricsCollectorFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  // `metrics_collector` can be null in incognito.
  if (metrics_collector) {
    metrics_collector->AddSafeBrowsingEventToPref(
        safe_browsing::SafeBrowsingMetricsCollector::EventType::
            EXTENSION_ALLOWLIST_INSTALL_BYPASS);
  }
#endif
}

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
bool ChromeWebstorePrivateAPIDelegate::IsSafeBrowsingEnabledAndReady(
    content::BrowserContext* context) {
  PrefService* prefs = Profile::FromBrowserContext(context)->GetPrefs();
  return safe_browsing::SafeBrowsingNavigationObserverManager::
      IsEnabledAndReady(prefs, g_browser_process->safe_browsing_service());
}

safe_browsing::SafeBrowsingNavigationObserverManager*
ChromeWebstorePrivateAPIDelegate::GetSafeBrowsingNavigationObserverManager(
    content::BrowserContext* context) {
  return safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
      GetForBrowserContext(context);
}
#endif

std::unique_ptr<enterprise_promotion::PromotionEligibilityChecker>
ChromeWebstorePrivateAPIDelegate::CreatePromotionEligibilityChecker(
    content::BrowserContext* context,
    bool dismissed_banner_pref,
    bool feature_enabled) {
  return policy::CreatePromotionEligibilityChecker(
      Profile::FromBrowserContext(context), dismissed_banner_pref,
      feature_enabled);
}

}  // namespace extensions

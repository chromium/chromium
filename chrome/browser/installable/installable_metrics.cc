// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installable_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#endif

// static
void InstallableMetrics::TrackInstallEvent(WebappInstallSource source) {
  DCHECK(IsReportableInstallSource(source));
  UMA_HISTOGRAM_ENUMERATION("Webapp.Install.InstallEvent", source,
                            WebappInstallSource::COUNT);
}

// static
bool InstallableMetrics::IsReportableInstallSource(WebappInstallSource source) {
  return source == WebappInstallSource::MENU_BROWSER_TAB ||
         source == WebappInstallSource::MENU_CUSTOM_TAB ||
         source == WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB ||
         source == WebappInstallSource::AUTOMATIC_PROMPT_CUSTOM_TAB ||
         source == WebappInstallSource::API_BROWSER_TAB ||
         source == WebappInstallSource::API_CUSTOM_TAB ||
         source == WebappInstallSource::DEVTOOLS ||
         source == WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB ||
         source == WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB ||
         source == WebappInstallSource::ARC ||
         source == WebappInstallSource::INTERNAL_DEFAULT ||
         source == WebappInstallSource::EXTERNAL_DEFAULT ||
         source == WebappInstallSource::EXTERNAL_POLICY ||
         source == WebappInstallSource::SYSTEM_DEFAULT ||
         source == WebappInstallSource::OMNIBOX_INSTALL_ICON ||
         source == WebappInstallSource::MENU_CREATE_SHORTCUT;
}

// static
WebappInstallSource InstallableMetrics::GetInstallSource(
    content::WebContents* web_contents,
    InstallTrigger trigger) {
  bool is_custom_tab = false;
#if defined(OS_ANDROID)
  auto* delegate = static_cast<android::TabWebContentsDelegateAndroid*>(
      web_contents->GetDelegate());
  is_custom_tab = delegate->IsCustomTab();
#endif

  switch (trigger) {
    case InstallTrigger::AMBIENT_BADGE:
      return is_custom_tab ? WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB
                           : WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB;
    case InstallTrigger::API:
      return is_custom_tab ? WebappInstallSource::API_CUSTOM_TAB
                           : WebappInstallSource::API_BROWSER_TAB;
    case InstallTrigger::AUTOMATIC_PROMPT:
      return is_custom_tab ? WebappInstallSource::AUTOMATIC_PROMPT_CUSTOM_TAB
                           : WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB;
    case InstallTrigger::MENU:
      return is_custom_tab ? WebappInstallSource::MENU_CUSTOM_TAB
                           : WebappInstallSource::MENU_BROWSER_TAB;
    // Create shortcut does not exist on Android, so it doesn't apply to custom
    // tab.
    case InstallTrigger::CREATE_SHORTCUT:
      DCHECK(!is_custom_tab);
      return WebappInstallSource::MENU_CREATE_SHORTCUT;
  }
  NOTREACHED();
  return WebappInstallSource::COUNT;
}

// static
void InstallableMetrics::RecordCheckServiceWorkerTime(base::TimeDelta time) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Webapp.CheckServiceWorker.Time", time);
}

// static
void InstallableMetrics::RecordCheckServiceWorkerStatus(
    ServiceWorkerOfflineCapability status) {
  UMA_HISTOGRAM_ENUMERATION("Webapp.CheckServiceWorker.Status", status);
}

// static
ServiceWorkerOfflineCapability
    InstallableMetrics::ConvertFromServiceWorkerCapability(
        content::ServiceWorkerCapability capability) {
  switch (capability) {
    case content::ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER:
      return ServiceWorkerOfflineCapability::kServiceWorkerWithOfflineSupport;
    case content::ServiceWorkerCapability::SERVICE_WORKER_NO_FETCH_HANDLER:
      return ServiceWorkerOfflineCapability::kServiceWorkerNoFetchHandler;
    case content::ServiceWorkerCapability::NO_SERVICE_WORKER:
      return ServiceWorkerOfflineCapability::kNoServiceWorker;
  }
  NOTREACHED();
}

// static
ServiceWorkerOfflineCapability InstallableMetrics::ConvertFromOfflineCapability(
    content::OfflineCapability capability) {
  switch (capability) {
    case content::OfflineCapability::kSupported:
      return ServiceWorkerOfflineCapability::kServiceWorkerWithOfflineSupport;
    case content::OfflineCapability::kUnsupported:
      return ServiceWorkerOfflineCapability::kServiceWorkerNoOfflineSupport;
  }
  NOTREACHED();
}

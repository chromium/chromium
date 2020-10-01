// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALLABLE_INSTALLABLE_METRICS_H_
#define CHROME_BROWSER_INSTALLABLE_INSTALLABLE_METRICS_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "content/public/browser/service_worker_context.h"

namespace content {
class WebContents;
}

enum class InstallTrigger {
  AMBIENT_BADGE,
  API,
  AUTOMATIC_PROMPT,
  MENU,
  CREATE_SHORTCUT,
};

// Sources for triggering webapp installation.
// NOTE: each enum entry which is reportable must be added to
// InstallableMetrics::IsReportableInstallSource().
// This enum backs a UMA histogram and must be treated as append-only.
enum class WebappInstallSource {
  // Menu item in a browser tab.
  MENU_BROWSER_TAB = 0,

  // Menu item in an Android Custom Tab.
  MENU_CUSTOM_TAB = 1,

  // Automatic prompt in a browser tab.
  AUTOMATIC_PROMPT_BROWSER_TAB = 2,

  // Automatic prompt in an Android Custom Tab.
  AUTOMATIC_PROMPT_CUSTOM_TAB = 3,

  // Developer-initiated API in a browser tab.
  API_BROWSER_TAB = 4,

  // Developer-initiated API in an Android Custom Tab.
  API_CUSTOM_TAB = 5,

  // Installation from a debug flow (e.g. via devtools).
  DEVTOOLS = 6,

  // Extensions management API (not reported).
  MANAGEMENT_API = 7,

  // PWA ambient badge in an Android Custom Tab.
  AMBIENT_BADGE_BROWSER_TAB = 8,

  // PWA ambient badge in browser Tab.
  AMBIENT_BADGE_CUSTOM_TAB = 9,

  // Installation via ARC on Chrome OS.
  ARC = 10,

  // An internal default-installed app on Chrome OS (i.e. triggered from code).
  INTERNAL_DEFAULT = 11,

  // An external default-installed app on Chrome OS (i.e. triggered from an
  // external source file).
  EXTERNAL_DEFAULT = 12,

  // A policy-installed app on Chrome OS.
  EXTERNAL_POLICY = 13,

  // A system app installed on Chrome OS.
  SYSTEM_DEFAULT = 14,

  // Install icon in the Omnibox.
  OMNIBOX_INSTALL_ICON = 15,

  // Installed from sync (not reported by |TrackInstallEvent|).
  SYNC = 16,

  // Create shortcut item in menu
  MENU_CREATE_SHORTCUT = 17,

  // Add any new values above this one.
  COUNT,
};

// This is the result of the promotability check that is recorded in the
// Webapp.CheckServiceWorker.Status histogram.
// Do not reorder or reuse any values in this enum. New values must be added to
// the end only.
enum class ServiceWorkerOfflineCapability {
  kNoServiceWorker,
  kServiceWorkerNoFetchHandler,
  // Service worker with a fetch handler but no offline support.
  kServiceWorkerNoOfflineSupport,
  // Service worker with a fetch handler with offline support.
  kServiceWorkerWithOfflineSupport,
  // Note: kMaxValue is needed only for histograms.
  kMaxValue = kServiceWorkerWithOfflineSupport,
};

class InstallableMetrics {
 public:
  // Records |source| in the Webapp.Install.InstallSource histogram.
  // IsReportableInstallSource(|source|) must be true.
  static void TrackInstallEvent(WebappInstallSource source);

  // Returns whether |source| is a value that may be passed to
  // TrackInstallEvent.
  static bool IsReportableInstallSource(WebappInstallSource source);

  // Returns the appropriate WebappInstallSource for |web_contents| when the
  // install originates from |trigger|.
  static WebappInstallSource GetInstallSource(
      content::WebContents* web_contents,
      InstallTrigger trigger);

  // Records |time| in the Webapp.CheckServiceWorker.Time histogram.
  static void RecordCheckServiceWorkerTime(base::TimeDelta time);

  // Records |status| in the Webapp.CheckServiceWorker.Status histogram.
  static void RecordCheckServiceWorkerStatus(
      ServiceWorkerOfflineCapability status);

  // Converts ServiceWorkerCapability to ServiceWorkerOfflineCapability.
  static ServiceWorkerOfflineCapability ConvertFromServiceWorkerCapability(
      content::ServiceWorkerCapability capability);

  // Converts OfflineCapability to ServiceWorkerOfflineCapability.
  static ServiceWorkerOfflineCapability ConvertFromOfflineCapability(
      content::OfflineCapability capability);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(InstallableMetrics);
};

#endif  // CHROME_BROWSER_INSTALLABLE_INSTALLABLE_METRICS_H_

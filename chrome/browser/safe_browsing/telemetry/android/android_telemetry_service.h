// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TELEMETRY_ANDROID_ANDROID_TELEMETRY_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_TELEMETRY_ANDROID_ANDROID_TELEMETRY_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/safe_browsing/telemetry/telemetry_service.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"

class Profile;
class PrefService;

namespace safe_browsing {

// Enumerates the possibilities for whether the CSBRR report was sent (or not).
enum class ApkDownloadTelemetryOutcome {
  NOT_SENT_SAFE_BROWSING_NOT_ENABLED = 0,
  // |web_contents| was nullptr. This happens sometimes when downloads are
  // resumed but it's not clear exactly when.
  NOT_SENT_MISSING_WEB_CONTENTS = 1,
  // No ping sent because the user is in Incognito mode.
  NOT_SENT_INCOGNITO = 2,
  // No ping sent because the user hasn't enabled extended reporting.
  // Deprecated for NOT_SENT_UNCONSENTED.
  NOT_SENT_EXTENDED_REPORTING_DISABLED = 3,
  // Download was cancelled.
  NOT_SENT_DOWNLOAD_CANCELLED = 4,
  // Failed to serialize the report.
  NOT_SENT_FAILED_TO_SERIALIZE = 5,
  // Feature not enabled so don't send.
  NOT_SENT_FEATURE_NOT_ENABLED = 6,
  // Download completed. Ping sent.
  SENT = 7,
  // No ping sent because the user hasn't enabled enhanced protection and
  // extended reporting.
  NOT_SENT_UNCONSENTED = 8,
  kMaxValue = NOT_SENT_UNCONSENTED
};

// This class is used to send telemetry information to Safe Browsing for
// security related incidents. The information is sent only if:
// 1. The user has opted in to extended reporting, AND
// 2. The security incident did not happen in an incognito window.
// As the name suggests, this works only on Android.

// The following events are currently considered security related incidents from
// the perspective of this class:
// 1. Downloading off-market APKs. See: go/zurkon-v1-referrer-dd

class AndroidTelemetryService
    : public download::DownloadItem::Observer,
      public download::SimpleDownloadManagerCoordinator::Observer,
      public TelemetryService {
 public:
  explicit AndroidTelemetryService(Profile* profile);

  AndroidTelemetryService(const AndroidTelemetryService&) = delete;
  AndroidTelemetryService& operator=(const AndroidTelemetryService&) = delete;

  ~AndroidTelemetryService() override;

  // download::SimpleDownloadManagerCoordinator::Observer.
  void OnDownloadCreated(download::DownloadItem* item) override;

  // download::DownloadItem::Observer.
  void OnDownloadUpdated(download::DownloadItem* item) override;
  void OnDownloadRemoved(download::DownloadItem* item) override;

  Profile* profile() { return profile_; }

 private:
  friend class AndroidTelemetryServiceTest;

  using OnGetReportDoneCallback = base::OnceCallback<void(
      std::unique_ptr<ClientSafeBrowsingReportRequest>)>;

  enum class ApkDownloadTelemetryIncompleteReason {
    // |web_contents| was nullptr. This happens sometimes when downloads are
    // resumed but it's not clear exactly when.
    MISSING_WEB_CONTENTS = 0,
    // Navigation manager wasn't ready yet to provide the referrer chain.
    SB_NAVIGATION_MANAGER_NOT_READY = 1,
    // Full referrer chain captured.
    COMPLETE = 2,
    // No render frame host existed for the download
    MISSING_RENDER_FRAME_HOST = 3,
    // The render frame host had not committed to a valid URL
    RENDER_FRAME_HOST_INVALID_URL = 4,

    kMaxValue = RENDER_FRAME_HOST_INVALID_URL,
  };

  struct ReferrerChainResult {
    ApkDownloadTelemetryIncompleteReason missing_reason;
    SafeBrowsingNavigationObserverManager::AttributionResult result;
    bool triggered_by_intent = false;
  };

  // Whether the ping can be sent, based on empty web_contents, or incognito
  // mode, or extended reporting opt-in status,
  bool CanSendPing(download::DownloadItem* item);

  // Populates the `ReferrerChainData` on `item` so that we can use it during
  // report construction.
  void FillReferrerChain(download::DownloadItem* item);

  // Sets the relevant fields in an instance of
  // |ClientSafeBrowsingReportRequest| proto and returns that proto
  // asynchronously.
  void GetReport(download::DownloadItem* item,
                 OnGetReportDoneCallback callback);

  // Callback when we know if app verification is enabled.
  void IsVerifyAppsEnabled(
      std::unique_ptr<ClientSafeBrowsingReportRequest> report,
      OnGetReportDoneCallback callback,
      VerifyAppsEnabledResult result);

  // Callback when report generation is complete.
  void OnGetReportDone(download::DownloadItem* item,
                       std::unique_ptr<ClientSafeBrowsingReportRequest> report);

  // Sends |report| proto to the Safe Browsing backend. The report may not be
  // sent if the proto fails to serialize.
  void MaybeSendApkDownloadReport(
      content::BrowserContext* browser_context,
      std::unique_ptr<ClientSafeBrowsingReportRequest> report);

  // Helper method to get prefs from |profile_|.
  const PrefService* GetPrefs();

  // Profile associated with this instance. Unowned.
  raw_ptr<Profile> profile_;

  // Referrer chains are best computed at download start, before we
  // know whether it will be suitable to send a ping. In order to log
  // metrics only for downloads that send a ping, we persist the
  // outcome of referrer chain computation.
  std::map<download::DownloadItem*, ReferrerChainResult> referrer_chain_result_;

  // The collection of `DownloadItem`s we're currently collecting reports for.
  std::set<download::DownloadItem*> reports_in_progress_;

  base::WeakPtrFactory<AndroidTelemetryService> weak_ptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_TELEMETRY_ANDROID_ANDROID_TELEMETRY_SERVICE_H_

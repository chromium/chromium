// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TELEMETRY_ANDROID_ANDROID_TELEMETRY_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_TELEMETRY_ANDROID_ANDROID_TELEMETRY_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/safe_browsing/telemetry/telemetry_service.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "content/public/browser/download_manager.h"

class Profile;
class PrefService;

namespace content {
class WebContents;
}

namespace safe_browsing {

class SafeBrowsingService;

// Enumerates the possibilities for whether the CSBRR report was sent (or not).
enum class ApkDownloadTelemetryOutcome {
  NOT_SENT_SAFE_BROWSING_NOT_ENABLED = 0,
  // |web_contents| was nullptr. This happens sometimes when downloads are
  // resumed but it's not clear exactly when.
  NOT_SENT_MISSING_WEB_CONTENTS = 1,
  // No ping sent because the user is in Incognito mode.
  NOT_SENT_INCOGNITO = 2,
  // No ping sent because the user hasn't enabled extended reporting.
  NOT_SENT_EXTENDED_REPORTING_DISABLED = 3,
  // Download was cancelled.
  NOT_SENT_DOWNLOAD_CANCELLED = 4,
  // Failed to serialize the report.
  NOT_SENT_FAILED_TO_SERIALIZE = 5,
  // Feature not enabled so don't send.
  NOT_SENT_FEATURE_NOT_ENABLED = 6,
  // Download completed. Ping sent.
  SENT = 7,

  kMaxValue = SENT
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
  AndroidTelemetryService(SafeBrowsingService* sb_service, Profile* profile);
  ~AndroidTelemetryService() override;

  // download::SimpleDownloadManagerCoordinator::Observer.
  void OnDownloadCreated(download::DownloadItem* item) override;

  // download::DownloadItem::Observer.
  void OnDownloadUpdated(download::DownloadItem* download) override;

  Profile* profile() { return profile_; }

 private:
  friend class AndroidTelemetryServiceTest;

  // Whether the ping can be sent, based on empty web_contents, or incognito
  // mode, or extended reporting opt-in status,
  bool CanSendPing(download::DownloadItem* item);

  // Fill the referrer chain in |report| with the actual referrer chain for the
  // given |web_contents|, as well as recent navigations.
  void FillReferrerChain(content::WebContents* web_contents,
                         ClientSafeBrowsingReportRequest* report);

  // Sets the relevant fields in an instance of
  // |ClientSafeBrowsingReportRequest| proto and returns that proto.
  std::unique_ptr<ClientSafeBrowsingReportRequest> GetReport(
      download::DownloadItem* item);

  // If |safety_net_id_on_ui_thread_| isn't already set, post a task on the IO
  // thread to get the safety net ID of the device and then store that value on
  // the UI thread in |safety_net_id_on_ui_thread_|.
  void MaybeCaptureSafetyNetId();

  // Sends |report| proto to the Safe Browsing backend. The report may not be
  // sent if the proto fails to serialize.
  void MaybeSendApkDownloadReport(
      std::unique_ptr<ClientSafeBrowsingReportRequest> report);

  // Gets called on the UI thread when the |safety_net_id| of the device has
  // been captured. Sets |safety_net_id_on_ui_thread_| to the captured value.
  void SetSafetyNetIdOnUIThread(const std::string& safety_net_id);

  // Helper method to get prefs from |profile_|.
  const PrefService* GetPrefs();

  // Helper method to check if Safe Browsing is enabled.
  bool IsSafeBrowsingEnabled();

  // Profile associated with this instance. Unowned.
  Profile* profile_;

  std::string safety_net_id_on_ui_thread_;

  // Unowned.
  SafeBrowsingService* sb_service_;

  base::WeakPtrFactory<AndroidTelemetryService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AndroidTelemetryService);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_TELEMETRY_ANDROID_ANDROID_TELEMETRY_SERVICE_H_

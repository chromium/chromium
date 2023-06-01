// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SCT_REPORTING_SERVICE_H_
#define CHROME_BROWSER_SSL_SCT_REPORTING_SERVICE_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class PrefService;
class Profile;

namespace safe_browsing {
class SafeBrowsingService;
}

namespace network::mojom {
enum class SCTAuditingMode;
}  // namespace network::mojom

// This class observes SafeBrowsing preference changes to enable/disable
// reporting. It does this by subscribing to changes in SafeBrowsing and
// extended reporting preferences. It also handles configuring SCT auditing in
// the network service.
class SCTReportingService : public KeyedService {
 public:
  static GURL& GetReportURLInstance();
  static GURL& GetHashdanceLookupQueryURLInstance();
  static void ReconfigureAfterNetworkRestart();

  // Returns whether the browser can send another SCT auditing report (i.e.,
  // whether the maximum report limit has been reached).
  static bool CanSendSCTAuditingReport();
  // Notification that a new SCT auditing report has been sent. Used to keep
  // track a browser-wide report count.
  static void OnNewSCTAuditingReportSent();

  SCTReportingService(safe_browsing::SafeBrowsingService* safe_browsing_service,
                      Profile* profile);
  ~SCTReportingService() override;

  SCTReportingService(const SCTReportingService&) = delete;
  const SCTReportingService& operator=(const SCTReportingService&) = delete;

  // Returns the reporting mode for the current profile settings.
  network::mojom::SCTAuditingMode GetReportingMode();

 private:
  void OnPreferenceChanged();

  raw_ptr<safe_browsing::SafeBrowsingService, DanglingUntriaged>
      safe_browsing_service_;
  const raw_ref<const PrefService> pref_service_;
  raw_ptr<Profile> profile_;
  base::CallbackListSubscription safe_browsing_state_subscription_;
};

#endif  // CHROME_BROWSER_SSL_SCT_REPORTING_SERVICE_H_

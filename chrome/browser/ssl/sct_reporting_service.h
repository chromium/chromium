// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SCT_REPORTING_SERVICE_H_
#define CHROME_BROWSER_SSL_SCT_REPORTING_SERVICE_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class PrefService;
class Profile;

namespace safe_browsing {
class SafeBrowsingService;
}

// This class observes SafeBrowsing preference changes to enable/disable
// reporting. It does this by subscribing to changes in SafeBrowsing and
// extended reporting preferences. It also handles configuring SCT auditing in
// the network service.
class SCTReportingService : public KeyedService {
 public:
  static GURL& GetReportURLInstance();
  static void ReconfigureAfterNetworkRestart();

  SCTReportingService(safe_browsing::SafeBrowsingService* safe_browsing_service,
                      Profile* profile);
  ~SCTReportingService() override;

  SCTReportingService(const SCTReportingService&) = delete;
  const SCTReportingService& operator=(const SCTReportingService&) = delete;

  // Enables or disables reporting.
  void SetReportingEnabled(bool enabled);

 private:
  void OnPreferenceChanged();

  safe_browsing::SafeBrowsingService* safe_browsing_service_;
  const PrefService& pref_service_;
  Profile* profile_;
  std::unique_ptr<base::CallbackList<void(void)>::Subscription>
      safe_browsing_state_subscription_;
};

#endif  // CHROME_BROWSER_SSL_SCT_REPORTING_SERVICE_H_

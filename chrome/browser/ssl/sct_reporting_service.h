// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SCT_REPORTING_SERVICE_H_
#define CHROME_BROWSER_SSL_SCT_REPORTING_SERVICE_H_

#include "base/callback_list.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;
class Profile;

namespace safe_browsing {
class SafeBrowsingService;
}

// This class observes SafeBrowsing preference changes to enable/disable
// reporting. It does this by subscribing to changes in SafeBrowsing and
// extended reporting preferences. It also receives notifications about new
// audit reports added to the SCT auditing cache and handles routing them to
// the correct NetworkContext for sending.
class SCTReportingService : public KeyedService {
 public:
  class TestObserver : public base::CheckedObserver {
   public:
    virtual void OnSCTReportReady(const std::string& cache_key) = 0;
  };

  SCTReportingService(safe_browsing::SafeBrowsingService* safe_browsing_service,
                      Profile* profile);
  ~SCTReportingService() override;

  SCTReportingService(const SCTReportingService&) = delete;
  const SCTReportingService& operator=(const SCTReportingService&) = delete;

  // Enables or disables reporting.
  void SetReportingEnabled(bool enabled);

  // Receives notification about a new entry being added to the network
  // service's SCT auditing cache under the key `cache_key`.
  void OnSCTReportReady(const std::string& cache_key);

  void AddObserverForTesting(TestObserver* observer);
  void RemoveObserverForTesting(TestObserver* observer);

 private:
  void OnPreferenceChanged();

  safe_browsing::SafeBrowsingService* safe_browsing_service_;
  const PrefService& pref_service_;
  Profile* profile_;
  std::unique_ptr<base::CallbackList<void(void)>::Subscription>
      safe_browsing_state_subscription_;

  base::ObserverList<TestObserver> test_observers_;
};

#endif  // CHROME_BROWSER_SSL_SCT_REPORTING_SERVICE_H_

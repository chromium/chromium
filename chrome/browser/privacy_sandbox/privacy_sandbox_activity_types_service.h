// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ACTIVITY_TYPES_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ACTIVITY_TYPES_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"

namespace privacy_sandbox {

class PrivacySandboxActivityTypesService : public KeyedService {
 public:
  explicit PrivacySandboxActivityTypesService(PrefService* pref_service);
  ~PrivacySandboxActivityTypesService() override;

  // KeyedService:
  void Shutdown() override;

  // On Clank startup, the RecordActivityType function will be called once,
  // passing in the corresponding PrivacySandboxStorageActivityType. Each time
  // the function is called, the kPrivacySandboxActivityTypeRecord2 preference
  // will be updated with a new list of activity type launches. This list is
  // limited in size and by the timestamps of recordable launches
  // (kPrivacySandboxActivityTypeStorageLastNLaunches and
  // kPrivacySandboxActivityTypeStorageWithinXDays). By having this storage
  // component, we can create an accurate heuristic to identify distinct user
  // groups based on their Chrome usage patterns. This will enable us to tailor
  // the user experience for specific launches in the near future.
  //
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.privacy_sandbox
  // LINT.IfChange(PrivacySandboxStorageActivityType)
  enum class PrivacySandboxStorageActivityType {
    kOther = 0,               // Partial CCT and all other unknowns
    kTabbed = 1,              // BrApp
    kAGSACustomTab = 2,       // AGSA-CCT
    kNonAGSACustomTab = 3,    // Non-AGSA-CCT
    kTrustedWebActivity = 4,  // TWA
    //   https://chromium.googlesource.com/chromium/src/+/HEAD/docs/webapps/README.md
    kWebapp = 5,  // Shortcut
    //   - https://web.dev/webapks/
    kWebApk = 6,  // PWA
    kPreFirstTab =
        7,  // Chrome has started running, but no tab has yet become visible.
    kMaxValue = kPreFirstTab,
  };
  // LINT.ThenChange(/tools/metrics/histograms/enums.xml)

  // Enum used for recording metrics about Clank Activity Type Storage
  //
  // LINT.IfChange(PrivacySandboxStorageUserSegmentByRecentActivity)
  enum class PrivacySandboxStorageUserSegmentByRecentActivity {
    kHasOther = 0,
    kHasBrowserApp = 1,
    kHasAGSACCT = 2,
    kHasNonAGSACCT = 3,
    kHasPWA = 4,
    kHasTWA = 5,
    kHasWebapp = 6,
    kHasPreFirstTab = 7,
    kMaxValue = kHasPreFirstTab,
  };
  // LINT.ThenChange(/tools/metrics/histograms/enums.xml)

  void RecordActivityType(PrivacySandboxStorageActivityType type) const;

 private:
  raw_ptr<PrefService> pref_service_;
};

}  // namespace privacy_sandbox
#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ACTIVITY_TYPES_SERVICE_H_

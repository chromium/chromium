// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_SAFE_BROWSING_CHECKER_CLIENT_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_SAFE_BROWSING_CHECKER_CLIENT_H_

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "url/gurl.h"

namespace safe_browsing {
struct ThreatMetadata;
}  // namespace safe_browsing

// Class used to check URL safety. A URL will be considered unsafe if it is
// present in the Safe Browsing blocklist for any of the threat types defined by
// `threat_types_`.
//
// At any time there can only exist one running check. For cases where multiple
// checks are received, only the latest one will be allowed to complete, while
// all previous ones will report the URL as not safe.
//
// Checker is configured to fail close. If the check takes longer than
// `safe_browsing_check_delay_`, the URL will be reported as not safe.
class AutoPictureInPictureSafeBrowsingCheckerClient
    : safe_browsing::SafeBrowsingDatabaseManager::Client {
 public:
  using ReportUrlSafetyCb = base::RepeatingCallback<void(bool)>;

  static constexpr base::TimeDelta kMinimumCheckDelay = base::Milliseconds(500);

  AutoPictureInPictureSafeBrowsingCheckerClient(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      base::TimeDelta safe_browsing_check_delay,
      ReportUrlSafetyCb report_url_safety_cb);

  ~AutoPictureInPictureSafeBrowsingCheckerClient() override;
  AutoPictureInPictureSafeBrowsingCheckerClient(
      const AutoPictureInPictureSafeBrowsingCheckerClient&) = delete;
  AutoPictureInPictureSafeBrowsingCheckerClient& operator=(
      const AutoPictureInPictureSafeBrowsingCheckerClient&) = delete;

  // Trigger the call to check the URL safety.
  void CheckUrlSafety(GURL url);

 private:
  FRIEND_TEST_ALL_PREFIXES(AutoPictureInPictureSafeBrowsingCheckerClientTest,
                           CheckCanceledOnCheckBlocklistTimeout);

  // safe_browsing::SafeBrowsingDatabaseManager::Client:
  void OnCheckBrowseUrlResult(
      const GURL& url,
      safe_browsing::SBThreatType threat_type,
      const safe_browsing::ThreatMetadata& metadata) override;

  // Callback to be run if a Safe Browsing request does not return a response
  // within `safe_browsing_check_delay` time.
  void OnCheckBlocklistTimeout();

  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager_;

  // Delay amount allowed for blocklist checks.
  base::TimeDelta safe_browsing_check_delay_;

  // Timer for running Safe Browsing checks. If `safe_browsing_check_delay_`
  // time has passed, run `OnCheckBlocklistTimeout`.
  base::OneShotTimer timer_;

  // Callback to report URL safety check results.
  ReportUrlSafetyCb report_url_safety_cb_;

  // All threat types used by `this` when performing URL safety checks.
  safe_browsing::SBThreatTypeSet threat_types_;

  base::WeakPtrFactory<AutoPictureInPictureSafeBrowsingCheckerClient>
      weak_factory_{this};
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_SAFE_BROWSING_CHECKER_CLIENT_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_OTP_FILLING_SAFE_BROWSING_CHECKER_CLIENT_H_
#define CHROME_BROWSER_AUTOFILL_OTP_FILLING_SAFE_BROWSING_CHECKER_CLIENT_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "url/gurl.h"

namespace safe_browsing {
class V5GetHashProtocolManager;
}

namespace autofill {

// Class used to check main frame and frame-to-fill safety. A URL will be
// considered malicious/unsafe if it is present in the Safe Browsing blocklist
// for any of the threat types defined by `threat_types_`.
//
// Checker is configured to fail close. If the check takes longer than
// `safe_browsing_check_delay_`, the URL will be reported as malicious/unsafe
// (`is_malicious` = true in callback).
//
// Note: This class is strictly single-use per check. `CreateAndCheck` creates a
// `std::unique_ptr<OtpFillingSafeBrowsingCheckerClient>` that starts the check
// immediately.
class OtpFillingSafeBrowsingCheckerClient
    : public safe_browsing::SafeBrowsingDatabaseManager::Client {
 public:
  using ResultCallback = base::OnceCallback<void(bool is_malicious)>;

  static constexpr base::TimeDelta kDefaultCheckDelay = base::Seconds(2);

  // Creates an instance and starts checking the URL safety. The caller owns the
  // returned object.
  static std::unique_ptr<OtpFillingSafeBrowsingCheckerClient> CreateAndCheck(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      base::WeakPtr<safe_browsing::V5GetHashProtocolManager>
          v5_get_hash_protocol_manager,
      base::TimeDelta safe_browsing_check_delay,
      const GURL& main_frame_url,
      const GURL& frame_to_fill_url,
      ResultCallback callback);

  ~OtpFillingSafeBrowsingCheckerClient() override;
  OtpFillingSafeBrowsingCheckerClient(
      const OtpFillingSafeBrowsingCheckerClient&) = delete;
  OtpFillingSafeBrowsingCheckerClient& operator=(
      const OtpFillingSafeBrowsingCheckerClient&) = delete;

 private:
  FRIEND_TEST_ALL_PREFIXES(OtpFillingSafeBrowsingCheckerClientTest,
                           GetV5GetHashProtocolManager);

  OtpFillingSafeBrowsingCheckerClient(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      base::WeakPtr<safe_browsing::V5GetHashProtocolManager>
          v5_get_hash_protocol_manager,
      base::TimeDelta safe_browsing_check_delay,
      ResultCallback callback);

  // Trigger the call to check the URLs safety.
  void CheckUrlSafety(const GURL& main_frame_url,
                      const GURL& frame_to_fill_url);

 private:
  // safe_browsing::SafeBrowsingDatabaseManager::Client:
  void OnCheckBrowseUrlResult(const GURL& url,
                              safe_browsing::SBThreatType threat_type) override;
  base::WeakPtr<safe_browsing::V5GetHashProtocolManager>
  GetV5GetHashProtocolManager() override;

  // Callback to be run if a Safe Browsing request does not return a response
  // within `safe_browsing_check_delay_` time.
  void OnCheckBlocklistTimeout();

  void CheckNextUrl();
  void RunCallback(bool is_malicious);

  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager_;
  base::WeakPtr<safe_browsing::V5GetHashProtocolManager>
      v5_get_hash_protocol_manager_;

  // Delay amount allowed for blocklist checks.
  base::TimeDelta safe_browsing_check_delay_;

  // Timer for running Safe Browsing checks. If `safe_browsing_check_delay_`
  // time has passed, run `OnCheckBlocklistTimeout`.
  base::OneShotTimer timer_;

  // Callback to report URL safety check results.
  ResultCallback callback_;

  // All threat types used by `this` when performing URL safety checks.
  safe_browsing::SBThreatTypeSet threat_types_;

  std::vector<GURL> urls_to_check_;
  size_t current_url_index_ = 0;

  base::WeakPtrFactory<OtpFillingSafeBrowsingCheckerClient> weak_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_OTP_FILLING_SAFE_BROWSING_CHECKER_CLIENT_H_

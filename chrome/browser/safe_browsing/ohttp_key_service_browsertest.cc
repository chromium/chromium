// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hashprefix_realtime/ohttp_key_service.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/ohttp_key_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "components/variations/pref_names.h"
#include "content/public/test/browser_test.h"

namespace safe_browsing {

namespace {
const char kTestOhttpKey[] = "TestOhttpKey";
}  // namespace

class SafeBrowsingOhttpKeyServiceTest : public InProcessBrowserTest {
 public:
  SafeBrowsingOhttpKeyServiceTest() = default;
  SafeBrowsingOhttpKeyServiceTest(const SafeBrowsingOhttpKeyServiceTest&) =
      delete;
  SafeBrowsingOhttpKeyServiceTest& operator=(
      const SafeBrowsingOhttpKeyServiceTest&) = delete;

 private:
  OhttpKeyServiceAllowerForTesting allow_ohttp_key_service_;
  hash_realtime_utils::GoogleChromeBrandingPretenderForTesting apply_branding_;
};

IN_PROC_BROWSER_TEST_F(SafeBrowsingOhttpKeyServiceTest,
                       ServiceRespectsLocationChanges) {
  auto* ohttp_key_service =
      OhttpKeyServiceFactory::GetForProfile(browser()->profile());
  ohttp_key_service->set_ohttp_key_for_testing(
      {kTestOhttpKey, base::Time::Now() + base::Days(10)});

  // By default, the service should be enabled.
  base::MockCallback<OhttpKeyService::Callback> success_callback;
  EXPECT_CALL(success_callback,
              Run(testing::Optional(std::string(kTestOhttpKey))))
      .Times(1);
  ohttp_key_service->GetOhttpKey(success_callback.Get());

  // Changing to CN should disable the service.
  g_browser_process->local_state()->SetString(
      variations::prefs::kVariationsCountry, "cn");
  base::MockCallback<OhttpKeyService::Callback> empty_callback;
  EXPECT_CALL(empty_callback, Run(testing::Eq(std::nullopt))).Times(1);
  ohttp_key_service->GetOhttpKey(empty_callback.Get());

  // Changing to US should re-enable the service.
  g_browser_process->local_state()->SetString(
      variations::prefs::kVariationsCountry, "us");
  base::MockCallback<OhttpKeyService::Callback> success_callback2;
  EXPECT_CALL(success_callback2,
              Run(testing::Optional(std::string(kTestOhttpKey))))
      .Times(1);
  ohttp_key_service->GetOhttpKey(success_callback2.Get());
}

}  // namespace safe_browsing

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/loader/features.h"
#include "chrome/browser/loader/keep_alive_request_browsertest_util.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/keep_alive_url_loader_utils.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

// Browser tests for features::kKeepAliveBrowserProcessAlive, which keeps the
// browser process (via ScopedKeepAlive) and the requests' profiles (via
// ScopedProfileKeepAlive) alive while browser-side fetch keepalive loaders
// (KeepAliveURLLoader) have requests in flight, so that fetch keepalive and
// fetchLater requests are not aborted when the last browser window closes
// before they complete. See https://crbug.com/408010432.

// Fixture with both the fetch keepalive in-browser migration and the
// process-alive hold enabled.
class FetchKeepAliveProcessAliveBrowserTest
    : public ChromeKeepAliveRequestBrowserTestBase {
 public:
  FetchKeepAliveProcessAliveBrowserTest() {
    InitFeatureList({{blink::features::kKeepAliveInBrowserMigration, {}},
                     {features::kKeepAliveBrowserProcessAlive, {}}});
  }
};

// Verifies that a KeepAliveOrigin::FETCH_KEEPALIVE_REQUEST keepalive is
// registered while a fetch keepalive request is in flight, and unregistered
// once the request completes.
IN_PROC_BROWSER_TEST_F(FetchKeepAliveProcessAliveBrowserTest,
                       RegistersKeepAliveWhileRequestInFlight) {
  auto* registry = KeepAliveRegistry::GetInstance();
  ASSERT_FALSE(
      registry->IsOriginRegistered(KeepAliveOrigin::FETCH_KEEPALIVE_REQUEST));

  const std::string target_url = kKeepAliveEndpoint;
  auto request_handler = std::move(RegisterRequestHandlers({target_url})[0]);
  ASSERT_TRUE(server()->Start());

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      GetKeepAlivePageURL(kPrimaryHost, target_url,
                          net::HttpRequestHeaders::kGetMethod)));

  request_handler->WaitForRequest();
  EXPECT_TRUE(
      registry->IsOriginRegistered(KeepAliveOrigin::FETCH_KEEPALIVE_REQUEST));

  request_handler->Send(k200TextResponse);
  request_handler->Done();
  loaders_observer().WaitForTotalOnCompleteForwarded({net::OK});

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !registry->IsOriginRegistered(
        KeepAliveOrigin::FETCH_KEEPALIVE_REQUEST);
  }));
}

#if !BUILDFLAG(IS_CHROMEOS)
// Verifies that every profile with in-flight fetch keepalive loaders is held
// via ScopedProfileKeepAlive, not only the first one (crbug.com/408010432).
IN_PROC_BROWSER_TEST_F(FetchKeepAliveProcessAliveBrowserTest,
                       HoldsEveryProfileWithInFlightLoaders) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* first_profile = browser()->GetProfile();
  Profile& second_profile = profiles::testing::CreateProfileSync(
      profile_manager,
      profile_manager->user_data_dir().AppendASCII("Profile 2"));

  ChromeContentBrowserClient client;
  client.OnFetchKeepAliveRequestCreated(*first_profile);
  client.OnFetchKeepAliveRequestCreated(second_profile);

  EXPECT_TRUE(profile_manager->HasKeepAliveForTesting(
      first_profile, ProfileKeepAliveOrigin::kFetchKeepAlive));
  EXPECT_TRUE(profile_manager->HasKeepAliveForTesting(
      &second_profile, ProfileKeepAliveOrigin::kFetchKeepAlive));

  // ~ScopedProfileKeepAlive removes the keepalive via a posted task when
  // features::kDestroyProfileOnBrowserClose is enabled, so wait for it.
  client.OnFetchKeepAliveRequestDestroyed(*first_profile);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !profile_manager->HasKeepAliveForTesting(
        first_profile, ProfileKeepAliveOrigin::kFetchKeepAlive);
  }));
  EXPECT_TRUE(profile_manager->HasKeepAliveForTesting(
      &second_profile, ProfileKeepAliveOrigin::kFetchKeepAlive));

  client.OnFetchKeepAliveRequestDestroyed(second_profile);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !profile_manager->HasKeepAliveForTesting(
        &second_profile, ProfileKeepAliveOrigin::kFetchKeepAlive);
  }));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Fixture with the fetch keepalive in-browser migration enabled but the
// process-alive hold explicitly disabled.
class FetchKeepAliveProcessAliveDisabledBrowserTest
    : public ChromeKeepAliveRequestBrowserTestBase {
 public:
  FetchKeepAliveProcessAliveDisabledBrowserTest() {
    InitFeatureList({{blink::features::kKeepAliveInBrowserMigration, {}}});
    disabled_features_.InitAndDisableFeature(
        features::kKeepAliveBrowserProcessAlive);
  }

 private:
  base::test::ScopedFeatureList disabled_features_;
};

// Verifies that no KeepAliveOrigin::FETCH_KEEPALIVE_REQUEST keepalive is
// registered for an in-flight fetch keepalive request when
// features::kKeepAliveBrowserProcessAlive is disabled.
IN_PROC_BROWSER_TEST_F(FetchKeepAliveProcessAliveDisabledBrowserTest,
                       DoesNotRegisterKeepAliveWhenFeatureDisabled) {
  auto* registry = KeepAliveRegistry::GetInstance();
  ASSERT_FALSE(
      registry->IsOriginRegistered(KeepAliveOrigin::FETCH_KEEPALIVE_REQUEST));

  const std::string target_url = kKeepAliveEndpoint;
  auto request_handler = std::move(RegisterRequestHandlers({target_url})[0]);
  ASSERT_TRUE(server()->Start());

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      GetKeepAlivePageURL(kPrimaryHost, target_url,
                          net::HttpRequestHeaders::kGetMethod)));
  request_handler->WaitForRequest();

  EXPECT_FALSE(
      registry->IsOriginRegistered(KeepAliveOrigin::FETCH_KEEPALIVE_REQUEST));

  request_handler->Send(k200TextResponse);
  request_handler->Done();
  loaders_observer().WaitForTotalOnCompleteForwarded({net::OK});
}

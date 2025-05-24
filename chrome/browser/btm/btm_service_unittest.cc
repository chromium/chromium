// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/btm_service.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/btm_service_test_utils.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
bool Has3pcException(BrowserContext* browser_context,
                     WebContents* web_contents,
                     const GURL& url,
                     const GURL& initial_url,
                     const GURL& final_url) {
  BtmRedirectInfoPtr redirect = BtmRedirectInfo::CreateForServer(
      UrlAndSourceId(url, ukm::kInvalidSourceId), BtmDataAccessType::kWrite,
      base::Time::Now(), false, net::HTTP_FOUND, base::TimeDelta());
  Populate3PcExceptions(browser_context, web_contents, initial_url, final_url,
                        base::span_from_ref(redirect));
  return redirect->has_3pc_exception.value();
}

class BtmService3pcExceptionsTest : public testing::Test {
 public:
  BtmService3pcExceptionsTest()
      : cookie_settings_(
            CookieSettingsFactory::GetForProfile(&profile_).get()) {}

  TestingProfile* GetProfile() { return &profile_; }

 protected:
  BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  raw_ptr<content_settings::CookieSettings> cookie_settings_;

  void SetUp() override {
    GetProfile()->GetPrefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled,
                                         true);
    ASSERT_FALSE(Are3PcsGenerallyEnabled());
  }

  // Add an exception to the third-party cookie blocking rule for
  // `third_party_url` embedded by `first_party_url`.
  void Add3PCException(const GURL& first_party_url,
                       const GURL& third_party_url) {
    cookie_settings_->SetTemporaryCookieGrantForHeuristic(
        third_party_url, first_party_url, base::Days(1),
        /*use_schemeless_patterns=*/false);

    auto* client = GetContentClientForTesting()->browser();
    ASSERT_TRUE(client->IsFullCookieAccessAllowed(
        &profile_, nullptr, third_party_url,
        blink::StorageKey::CreateFirstParty(
            url::Origin::Create(first_party_url)),
        /*overrides=*/{}));
    ASSERT_FALSE(client->IsFullCookieAccessAllowed(
        &profile_, nullptr, first_party_url,
        blink::StorageKey::CreateFirstParty(
            url::Origin::Create(third_party_url)),
        /*overrides=*/{}));
  }

  bool Are3PcsGenerallyEnabled() {
    return ::content::Are3PcsGenerallyEnabled(&profile_, nullptr);
  }
};

// Verifies that redirect chains that start or end on embedder URLs do not
// have 3PC exceptions.
TEST_F(BtmService3pcExceptionsTest, EmbedderUrl_DoesNotHaveException) {
  GURL non_web_url(chrome::kChromeUINewTabURL);
  GURL redirect_url("https://redirect.com");

  EXPECT_FALSE(Has3pcException(GetProfile(), nullptr, redirect_url, non_web_url,
                               GURL("https://final.com")));
  EXPECT_FALSE(Has3pcException(GetProfile(), nullptr, redirect_url,
                               GURL("https://initial.com"), non_web_url));

  // Verify that exception logic is working as expected for HTTP(S) URLs.
  EXPECT_FALSE(Has3pcException(GetProfile(), nullptr, redirect_url,
                               GURL("https://initial.com"),
                               GURL("https://final.com")));
  Add3PCException(GURL("https://initial.com"), redirect_url);
  EXPECT_TRUE(Has3pcException(GetProfile(), nullptr, redirect_url,
                              GURL("https://initial.com"),
                              GURL("https://final.com")));
}

}  // namespace content

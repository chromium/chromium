// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/critical_actions/critical_action_ui_utils.h"

#include "base/strings/strcat.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace critical_actions {
namespace {

TEST(CriticalActionUiUtilsTest, PasswordManagerLinkoutDomainExtraction) {
  EXPECT_EQ(
      GetCriticalActionLinkoutUrl(ActionType::kCredentialAccess,
                                  GURL("https://www.example.com/login")),
      base::StrCat({GetGooglePasswordManagerSubPageURLStr(), "/example.com"}));

  EXPECT_EQ(GetCriticalActionLinkoutUrl(
                ActionType::kCredentialAccess,
                GURL("https://login.example.co.uk/auth")),
            base::StrCat(
                {GetGooglePasswordManagerSubPageURLStr(), "/example.co.uk"}));

  EXPECT_EQ(
      GetCriticalActionLinkoutUrl(ActionType::kCredentialAccess,
                                  GURL("http://localhost:8080/login")),
      base::StrCat({GetGooglePasswordManagerSubPageURLStr(), "/localhost"}));

  EXPECT_EQ(GetCriticalActionLinkoutUrl(ActionType::kCredentialAccess, GURL()),
            chrome::kChromeUIPasswordManagerURL);
}

TEST(CriticalActionUiUtilsTest, LinkoutsForActionTypes) {
  EXPECT_EQ(
      GetCriticalActionLinkoutUrl(ActionType::kFormFill,
                                  GURL("https://www.example.com/checkout")),
      base::StrCat({chrome::kChromeUISettingsURL, chrome::kAddressesSubPage}));

  EXPECT_EQ(GetCriticalActionLinkoutUrl(ActionType::kDownload,
                                        GURL("https://www.example.com/file")),
            chrome::kChromeUIDownloadsURL);

  EXPECT_EQ(GetCriticalActionLinkoutUrl(
                ActionType::kSettingChange,
                GURL("https://docs.google.com/document/d/123")),
            net::AppendQueryParameter(
                GURL(base::StrCat({chrome::kChromeUISettingsURL,
                                   chrome::kSiteDetailsSubpage})),
                "site", "https://docs.google.com")
                .spec());

  EXPECT_EQ(
      GetCriticalActionLinkoutUrl(ActionType::kGooglePasswordManager,
                                  GURL("https://www.example.com/login")),
      base::StrCat({GetGooglePasswordManagerSubPageURLStr(), "/example.com"}));

  EXPECT_EQ(
      GetCriticalActionLinkoutUrl(ActionType::kFederatedLogin,
                                  GURL("https://www.example.com/auth")),
      base::StrCat({GetGooglePasswordManagerSubPageURLStr(), "/example.com"}));

  EXPECT_EQ(
      GetCriticalActionLinkoutUrl(ActionType::kCredentialsOtp,
                                  GURL("https://www.example.com/otp")),
      base::StrCat({GetGooglePasswordManagerSubPageURLStr(), "/example.com"}));

  EXPECT_EQ(GetCriticalActionLinkoutUrl(ActionType::kUnknown,
                                        GURL("https://www.example.com/")),
            chrome::kChromeUISettingsURL);
}

// Regression test: an empty URL yields an opaque origin, which serialises to
// the literal string "null". Without a guard this produces a site details page
// for a site that does not exist.
TEST(CriticalActionUiUtilsTest, SettingChangeWithEmptyUrlDoesNotLinkToNull) {
  const std::string linkout =
      GetCriticalActionLinkoutUrl(ActionType::kSettingChange, GURL());

  EXPECT_EQ(linkout, chrome::kChromeUISettingsURL);
  EXPECT_NE(linkout,
            net::AppendQueryParameter(
                GURL(base::StrCat({chrome::kChromeUISettingsURL,
                                   chrome::kSiteDetailsSubpage})),
                "site", "null")
                .spec());
}

}  // namespace
}  // namespace critical_actions

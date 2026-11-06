// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"

#include "base/strings/strcat.h"
#include "chrome/common/webui_url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/common/constants.h"
#endif

namespace search {

TEST(SearchURLsUnitTest, IsNewTabOrigin) {
  EXPECT_TRUE(IsNewTabOrigin(GURL("chrome://newtab/")));
  EXPECT_TRUE(IsNewTabOrigin(GURL("chrome://newtab/path?query=1")));
  EXPECT_TRUE(IsNewTabOrigin(url::Origin::Create(GURL("chrome://newtab/"))));
  EXPECT_FALSE(IsNewTabOrigin(GURL("chrome://new-tab-page/")));
  EXPECT_FALSE(IsNewTabOrigin(GURL("chrome://new-tab-page-third-party/")));
  EXPECT_FALSE(IsNewTabOrigin(GURL("chrome-native://newtab/")));
  EXPECT_FALSE(IsNewTabOrigin(GURL()));
  EXPECT_FALSE(
      IsNewTabOrigin(url::Origin::Create(GURL("chrome://new-tab-page/"))));
}

TEST(SearchURLsUnitTest, IsNewTabPageOrigin) {
  EXPECT_TRUE(IsNewTabPageOrigin(GURL("chrome://new-tab-page/")));
  EXPECT_TRUE(IsNewTabPageOrigin(GURL("chrome://new-tab-page/path?query=1")));
  EXPECT_TRUE(
      IsNewTabPageOrigin(url::Origin::Create(GURL("chrome://new-tab-page/"))));
  EXPECT_FALSE(IsNewTabPageOrigin(GURL("chrome://newtab/")));
  EXPECT_FALSE(IsNewTabPageOrigin(GURL("chrome://new-tab-page-third-party/")));
  EXPECT_FALSE(IsNewTabPageOrigin(GURL()));
  EXPECT_FALSE(
      IsNewTabPageOrigin(url::Origin::Create(GURL("chrome://newtab/"))));
}

TEST(SearchURLsUnitTest, IsThirdPartyNewTabPageOrigin) {
  EXPECT_TRUE(
      IsThirdPartyNewTabPageOrigin(GURL("chrome://new-tab-page-third-party/")));
  EXPECT_TRUE(IsThirdPartyNewTabPageOrigin(
      GURL("chrome://new-tab-page-third-party/path?query=1")));
  EXPECT_FALSE(IsThirdPartyNewTabPageOrigin(GURL("chrome://new-tab-page/")));
  EXPECT_FALSE(IsThirdPartyNewTabPageOrigin(GURL("chrome://newtab/")));
  EXPECT_FALSE(IsThirdPartyNewTabPageOrigin(GURL()));
}

TEST(SearchURLsUnitTest, IsWebUINewTabPageOrigin) {
  EXPECT_TRUE(IsWebUINewTabPageOrigin(GURL("chrome://newtab/")));
  EXPECT_TRUE(IsWebUINewTabPageOrigin(GURL("chrome://new-tab-page/")));
  EXPECT_TRUE(
      IsWebUINewTabPageOrigin(GURL("chrome://new-tab-page-third-party/")));
  EXPECT_FALSE(IsWebUINewTabPageOrigin(GURL("chrome-search://remote-ntp/")));
  EXPECT_FALSE(IsWebUINewTabPageOrigin(GURL("chrome-native://newtab/")));
  EXPECT_FALSE(
      IsWebUINewTabPageOrigin(GURL("https://www.google.com/_/chrome/newtab")));
  EXPECT_FALSE(IsWebUINewTabPageOrigin(GURL()));
}

TEST(SearchURLsUnitTest, IsSplitViewNewTabPage) {
  EXPECT_TRUE(
      IsSplitViewNewTabPage(GURL(chrome::kChromeUISplitViewNewTabPageURL)));
  EXPECT_FALSE(IsSplitViewNewTabPage(GURL("chrome://new-tab-page/")));
  EXPECT_FALSE(IsSplitViewNewTabPage(GURL("chrome://newtab/")));
  EXPECT_FALSE(IsSplitViewNewTabPage(GURL()));
}

TEST(SearchURLsUnitTest, NewTabPredicatesAreExactMatches) {
  // Opaque origins never match, even when derived from an NTP origin.
  const url::Origin ntp = url::Origin::Create(GURL("chrome://new-tab-page/"));
  const url::Origin newtab = url::Origin::Create(GURL("chrome://newtab/"));
  EXPECT_FALSE(IsNewTabOrigin(url::Origin()));
  EXPECT_FALSE(IsNewTabPageOrigin(url::Origin()));
  EXPECT_FALSE(IsNewTabPageOrigin(ntp.DeriveNewOpaqueOrigin()));
  EXPECT_FALSE(IsNewTabOrigin(newtab.DeriveNewOpaqueOrigin()));

  // Neighboring schemes and hosts must not match.
  for (const char* spec :
       {"chrome-untrusted://new-tab-page/", "chrome://newtab-footer/",
        "chrome-search://remote-ntp/"}) {
    const GURL url(spec);
    EXPECT_FALSE(IsNewTabOrigin(url)) << spec;
    EXPECT_FALSE(IsNewTabPageOrigin(url)) << spec;
    EXPECT_FALSE(IsThirdPartyNewTabPageOrigin(url)) << spec;
    EXPECT_FALSE(IsWebUINewTabPageOrigin(url)) << spec;
    EXPECT_FALSE(IsNewTabOrigin(url::Origin::Create(url))) << spec;
    EXPECT_FALSE(IsNewTabPageOrigin(url::Origin::Create(url))) << spec;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  const GURL ext_ntp(
      base::StrCat({"chrome-extension://",
                    extension_misc::kNewTabPageExtensionId, "/index.html"}));
  EXPECT_FALSE(IsNewTabOrigin(ext_ntp));
  EXPECT_FALSE(IsNewTabPageOrigin(ext_ntp));
  EXPECT_FALSE(IsThirdPartyNewTabPageOrigin(ext_ntp));
  EXPECT_FALSE(IsWebUINewTabPageOrigin(ext_ntp));
  EXPECT_FALSE(IsNewTabOrigin(url::Origin::Create(ext_ntp)));
  EXPECT_FALSE(IsNewTabPageOrigin(url::Origin::Create(ext_ntp)));
#endif

  // Invalid GURLs whose parsed scheme and host read chrome://<ntp-host> must
  // not match.
  for (const char* valid_spec : {"chrome://newtab/", "chrome://new-tab-page/",
                                 "chrome://new-tab-page-third-party/"}) {
    const GURL valid_url(valid_spec);
    const GURL invalid_url(valid_url.spec(),
                           valid_url.parsed_for_possibly_invalid_spec(),
                           /*is_valid=*/false);
    ASSERT_FALSE(invalid_url.is_valid()) << valid_spec;
    EXPECT_FALSE(IsNewTabOrigin(invalid_url)) << valid_spec;
    EXPECT_FALSE(IsNewTabPageOrigin(invalid_url)) << valid_spec;
    EXPECT_FALSE(IsThirdPartyNewTabPageOrigin(invalid_url)) << valid_spec;
    EXPECT_FALSE(IsWebUINewTabPageOrigin(invalid_url)) << valid_spec;
  }

  // GURL overloads match the URL's own scheme and host. The inner origin of a
  // filesystem: or blob: URL is intentionally not unwrapped by the GURL
  // overloads.
  const GURL fs("filesystem:chrome://new-tab-page/temporary/a.html");
  EXPECT_FALSE(IsNewTabPageOrigin(fs));
  EXPECT_FALSE(IsWebUINewTabPageOrigin(fs));
  EXPECT_TRUE(IsNewTabPageOrigin(url::Origin::Create(fs)));

  const GURL blob("blob:chrome://new-tab-page/1234-5678");
  EXPECT_FALSE(IsNewTabPageOrigin(blob));
  EXPECT_FALSE(IsWebUINewTabPageOrigin(blob));
  EXPECT_TRUE(IsNewTabPageOrigin(url::Origin::Create(blob)));
}

TEST(SearchURLsUnitTest, NtpPredicatesRejectOtherSchemesAndWrappers) {
  for (const char* host :
       {chrome::kChromeUINewTabHost, chrome::kChromeUINewTabPageHost,
        chrome::kChromeUINewTabPageThirdPartyHost}) {
    for (const char* prefix :
         {"chrome-untrusted://", "chrome-search://", "https://",
          "filesystem:chrome://", "blob:chrome://"}) {
      const GURL url(base::StrCat({prefix, host, "/temporary/x"}));
      SCOPED_TRACE(url.possibly_invalid_spec());
      EXPECT_FALSE(IsNewTabOrigin(url));
      EXPECT_FALSE(IsNewTabPageOrigin(url));
      EXPECT_FALSE(IsThirdPartyNewTabPageOrigin(url));
      EXPECT_FALSE(IsWebUINewTabPageOrigin(url));
    }
    const url::Origin untrusted = url::Origin::Create(
        GURL(base::StrCat({"chrome-untrusted://", host, "/"})));
    EXPECT_FALSE(IsNewTabOrigin(untrusted));
    EXPECT_FALSE(IsNewTabPageOrigin(untrusted));
  }
  // The Origin overloads must not match the 3P NTP origin.
  const url::Origin third_party =
      url::Origin::Create(GURL("chrome://new-tab-page-third-party/"));
  EXPECT_FALSE(IsNewTabOrigin(third_party));
  EXPECT_FALSE(IsNewTabPageOrigin(third_party));
}

}  // namespace search

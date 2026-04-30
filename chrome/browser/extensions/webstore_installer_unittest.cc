// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/webstore_installer.h"

#include <string>

#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "chrome/browser/update_client/chrome_update_query_params_delegate.h"
#include "components/crx_file/id_util.h"
#include "components/update_client/update_query_params.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using base::StringPrintf;
using update_client::UpdateQueryParams;

namespace extensions {

// Returns true if |target| is found in |source|.
bool Contains(const std::string& source, const std::string& target) {
  return source.find(target) != std::string::npos;
}

TEST(WebstoreInstallerTest, PlatformParams) {
  std::string id = crx_file::id_util::GenerateId("some random string");
  std::string source = "inline";
  GURL url = WebstoreInstaller::GetWebstoreInstallURL(
      id, WebstoreInstaller::INSTALL_SOURCE_INLINE);
  std::string query = url.GetQuery();
  EXPECT_TRUE(
      Contains(query, StringPrintf("os=%s", UpdateQueryParams::GetOS())));
  EXPECT_TRUE(
      Contains(query, StringPrintf("arch=%s", UpdateQueryParams::GetArch())));
  EXPECT_TRUE(Contains(
      query, base::EscapeQueryParamValue(
                 StringPrintf("installsource=%s", source.c_str()), true)));
  EXPECT_TRUE(Contains(
      query,
      StringPrintf("lang=%s", ChromeUpdateQueryParamsDelegate::GetLang())));
  // Information about NaCl architecture is omitted following NaCl removal
  EXPECT_FALSE(Contains(query, "nacl_arch"));
  EXPECT_FALSE(Contains(query, "os_arch"));
}

TEST(WebstoreInstallerTest, AuthUserParameterEncoding) {
  {
    // Regression test for crbug.com/500569738.
    // A malicious authuser string that attempts to inject another parameter.
    GURL url("https://example.com/download");
    std::string malicious_authuser = "0&prodversion=50.0.0.0";
    WebstoreInstaller::MaybeAppendAuthUserParameter(malicious_authuser, url);
    std::string query = url.GetQuery();
    // The malicious string should now be correctly encoded.
    EXPECT_TRUE(Contains(query, "authuser=0%26prodversion%3D50.0.0.0"));
    EXPECT_FALSE(Contains(query, "prodversion=50.0.0.0"));
  }

  {
    // If authuser is empty, the URL should not be modified.
    GURL url("https://example.com/download?a=b");
    WebstoreInstaller::MaybeAppendAuthUserParameter("", url);
    EXPECT_EQ("https://example.com/download?a=b", url.spec());
  }

  {
    // If authuser is already present, it should NOT be replaced.
    GURL url("https://example.com/download?authuser=1&a=b");
    WebstoreInstaller::MaybeAppendAuthUserParameter("2", url);
    std::string query = url.GetQuery();
    EXPECT_TRUE(Contains(query, "authuser=1"));
    EXPECT_FALSE(Contains(query, "authuser=2"));
    EXPECT_TRUE(Contains(query, "a=b"));
  }
}

}  // namespace extensions

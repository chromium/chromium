// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/update_client/chrome_update_query_params_delegate.h"
#include "components/crx_file/id_util.h"
#include "components/update_client/update_query_params.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  std::string query = url.query();
  EXPECT_TRUE(
      Contains(query, StringPrintf("os=%s", UpdateQueryParams::GetOS())));
  EXPECT_TRUE(
      Contains(query, StringPrintf("arch=%s", UpdateQueryParams::GetArch())));
  EXPECT_TRUE(Contains(
      query,
      StringPrintf("os_arch=%s",
                   base::SysInfo().OperatingSystemArchitecture().c_str())));
  EXPECT_TRUE(Contains(
      query, StringPrintf("nacl_arch=%s", UpdateQueryParams::GetNaclArch())));
  EXPECT_TRUE(Contains(
      query, base::EscapeQueryParamValue(
                 StringPrintf("installsource=%s", source.c_str()), true)));
  EXPECT_TRUE(Contains(
      query,
      StringPrintf("lang=%s", ChromeUpdateQueryParamsDelegate::GetLang())));
}

}  // namespace extensions

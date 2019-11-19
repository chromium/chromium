// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_url.h"

#include <string>

#include "chrome/browser/chromeos/smb_client/smb_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace smb_client {

class SmbUrlTest : public testing::Test {
 public:
  SmbUrlTest() = default;
  ~SmbUrlTest() override = default;

  void ExpectInvalidUrl(const std::string& url) {
    SmbUrl smb_url(url);
    EXPECT_FALSE(smb_url.IsValid());
  }

  void ExpectValidUrl(const std::string& url,
                      const std::string& expected_url,
                      const std::string& expected_host,
                      const std::string& expected_share) {
    SmbUrl smb_url(url);
    EXPECT_TRUE(smb_url.IsValid());
    EXPECT_EQ(expected_url, smb_url.ToString());
    EXPECT_EQ(expected_host, smb_url.GetHost());
    EXPECT_EQ(expected_share, smb_url.GetShare());
  }

  void ExpectValidWindowsUNC(const std::string& url,
                             const std::string& expected_unc) {
    SmbUrl smb_url(url);
    EXPECT_TRUE(smb_url.IsValid());
    EXPECT_EQ(expected_unc, smb_url.GetWindowsUNCString());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SmbUrlTest);
};

TEST_F(SmbUrlTest, EmptyUrlIsInvalid) {
  ExpectInvalidUrl("");
}

TEST_F(SmbUrlTest, InvalidUrls) {
  ExpectInvalidUrl("smb");
  ExpectInvalidUrl("smb://");
  ExpectInvalidUrl("\\");
  ExpectInvalidUrl("\\\\");
  ExpectInvalidUrl("smb:///");
  ExpectInvalidUrl("://host/path");
  ExpectInvalidUrl("\\://host/path");
  ExpectInvalidUrl("\\:/host/path");
}

TEST_F(SmbUrlTest, ValidUrls) {
  ExpectValidUrl("smb://x", "smb://x/", "x", "");
  ExpectValidUrl("smb:///x", "smb://x/", "x", "");
  ExpectValidUrl("smb:///x/y", "smb://x/y", "x", "y");
  ExpectValidUrl("smb:///x/y/", "smb://x/y/", "x", "y");
  ExpectValidUrl("smb:///x//y", "smb://x//y", "x", "");
  ExpectValidUrl("smb:///x//y//", "smb://x//y//", "x", "");
  ExpectValidUrl("smb://server/share/long/folder",
                 "smb://server/share/long/folder", "server", "share");
  ExpectValidUrl("smb://server/share/folder.with.dots",
                 "smb://server/share/folder.with.dots", "server", "share");
  ExpectValidUrl("smb://server\\share/mixed\\slashes",
                 "smb://server/share/mixed/slashes", "server", "share");
  ExpectValidUrl("\\\\server", "smb://server/", "server", "");
  ExpectValidUrl("\\\\server/share", "smb://server/share", "server", "share");
  ExpectValidUrl("\\\\server\\share/mixed//slashes",
                 "smb://server/share/mixed//slashes", "server", "share");
  ExpectValidUrl("smb://192.168.0.1/share", "smb://192.168.0.1/share",
                 "192.168.0.1", "share");
}

TEST_F(SmbUrlTest, NotValidIfStartsWithoutSchemeOrDoubleBackslash) {
  ExpectInvalidUrl("192.168.0.1/share");
}

TEST_F(SmbUrlTest, StartsWithBackslashRemovesBackslashAndAddsScheme) {
  ExpectValidUrl("\\\\192.168.0.1\\share", "smb://192.168.0.1/share",
                 "192.168.0.1", "share");
}

TEST_F(SmbUrlTest, GetHostWithIp) {
  ExpectValidUrl("smb://192.168.0.1/share", "smb://192.168.0.1/share",
                 "192.168.0.1", "share");
}

TEST_F(SmbUrlTest, GetHostWithDomain) {
  ExpectValidUrl("smb://server/share", "smb://server/share", "server", "share");
}

TEST_F(SmbUrlTest, HostBecomesLowerCase) {
  ExpectValidUrl("smb://SERVER/share", "smb://server/share", "server", "share");
}

TEST_F(SmbUrlTest, ReplacesHost) {
  SmbUrl smb_url("smb://server/share");
  EXPECT_TRUE(smb_url.IsValid());

  const std::string expected_host = "server";
  EXPECT_EQ(expected_host, smb_url.GetHost());

  const std::string new_host = "192.168.0.1";
  const std::string expected_url = "smb://192.168.0.1/share";
  EXPECT_EQ(expected_url, smb_url.ReplaceHost(new_host));

  // GetHost returns the original host.
  EXPECT_EQ(expected_host, smb_url.GetHost());
}

TEST_F(SmbUrlTest, GetWindowsURL) {
  ExpectValidWindowsUNC("smb://server/share", "\\\\server\\share");
  ExpectValidWindowsUNC("smb://server/share/long/folder",
                        "\\\\server\\share\\long\\folder");
  ExpectValidWindowsUNC("smb://server/share/folder.with.dots",
                        "\\\\server\\share\\folder.with.dots");
  ExpectValidWindowsUNC("smb://server\\share/mixed\\slashes",
                        "\\\\server\\share\\mixed\\slashes");
  ExpectValidWindowsUNC("\\\\server/share", "\\\\server\\share");
}

}  // namespace smb_client
}  // namespace chromeos

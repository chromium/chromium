// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/url_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::input_method {
namespace {

TEST(UrlUtilsTest, IsSubDomainHandlesStandardDomains) {
  EXPECT_TRUE(IsSubDomain(GURL("https://google.com"), "google"));
  EXPECT_TRUE(IsSubDomain(GURL("https://amazon.com"), "amazon"));
  EXPECT_TRUE(IsSubDomain(GURL("http://example.org"), "example"));

  EXPECT_FALSE(IsSubDomain(GURL("https://google.com"), "amazon"));
  EXPECT_FALSE(IsSubDomain(GURL("https://notgoogle.com"), "google"));
  EXPECT_FALSE(IsSubDomain(GURL("https://fake-google.com"), "google"));
  EXPECT_FALSE(IsSubDomain(GURL("https://google.fake.com"), "google"));
}

TEST(UrlUtilsTest, IsSubDomainHandlesSubDomains) {
  EXPECT_TRUE(IsSubDomain(GURL("https://mail.google.com"), "google"));
  EXPECT_TRUE(IsSubDomain(GURL("https://mail.google.com"), "mail.google"));
  EXPECT_TRUE(IsSubDomain(GURL("https://sub.mail.google.com"), "mail.google"));
  EXPECT_TRUE(IsSubDomain(GURL("https://sub.mail.google.com"), "google"));
  EXPECT_TRUE(IsSubDomain(GURL("https://b.corp.google.com"), "b.corp.google"));
  EXPECT_TRUE(IsSubDomain(GURL("https://b.corp.google.com"), "corp.google"));
  EXPECT_TRUE(IsSubDomain(GURL("https://b.corp.google.com"), "google"));
  EXPECT_TRUE(IsSubDomain(GURL("http://smile.amazon.com"), "amazon"));

  EXPECT_FALSE(IsSubDomain(GURL("https://mail.google.com"), "docs.google"));
  EXPECT_FALSE(IsSubDomain(GURL("https://amazon.domain.com"), "amazon"));
  EXPECT_FALSE(IsSubDomain(GURL("https://smile.amazon.foo.com"), "amazon"));
}

TEST(UrlUtilsTest, IsSubDomainHandlesMultiPartRegistries) {
  EXPECT_TRUE(IsSubDomain(GURL("https://amazon.co.uk"), "amazon"));
  EXPECT_TRUE(IsSubDomain(GURL("https://smile.amazon.co.uk"), "amazon"));
  EXPECT_TRUE(IsSubDomain(GURL("https://amazon.com.au"), "amazon"));
  EXPECT_TRUE(
      IsSubDomain(GURL("http://www.abc.smile.amazon.com.au/path"), "amazon"));
  EXPECT_TRUE(IsSubDomain(GURL("https://mail.google.co.jp"), "mail.google"));

  EXPECT_FALSE(IsSubDomain(GURL("https://amazon.co.uk"), "co.uk"));
  EXPECT_FALSE(IsSubDomain(GURL("https://notamazon.co.uk"), "amazon"));
}

TEST(UrlUtilsTest, IsSubDomainHandlesLocalhost) {
  EXPECT_TRUE(IsSubDomain(GURL("http://localhost"), "localhost"));
  EXPECT_TRUE(IsSubDomain(GURL("http://localhost:8080"), "localhost"));
  EXPECT_TRUE(IsSubDomain(GURL("http://localhost/test"), "localhost"));
  EXPECT_TRUE(IsSubDomain(GURL("http://sub.localhost:3000"), "localhost"));

  EXPECT_FALSE(IsSubDomain(GURL("http://localhost"), "google"));
  EXPECT_FALSE(IsSubDomain(GURL("http://localhost"), ""));
  EXPECT_FALSE(IsSubDomain(GURL("http://notlocalhost"), "localhost"));
}

TEST(UrlUtilsTest, IsSubDomainHandlesNonRegistryAndIpAddresses) {
  EXPECT_FALSE(IsSubDomain(GURL("http://127.0.0.1"), "127.0.0.1"));
  EXPECT_FALSE(IsSubDomain(GURL("http://192.168.1.1:8080"), "192.168.1.1"));
  EXPECT_FALSE(IsSubDomain(GURL("http://127.0.0.1"), "localhost"));
  EXPECT_FALSE(IsSubDomain(GURL("http://[::1]"), "localhost"));
  EXPECT_FALSE(IsSubDomain(GURL("http://intranet_server"), "intranet_server"));
}

TEST(UrlUtilsTest, IsSubDomainHandlesInvalidUrlsAndMissingHosts) {
  EXPECT_FALSE(IsSubDomain(GURL("invalid"), "google"));
  EXPECT_FALSE(IsSubDomain(GURL(""), "google"));
  EXPECT_FALSE(IsSubDomain(GURL("http://"), "google"));
  EXPECT_FALSE(
      IsSubDomain(GURL("file:///home/chronos/user/file.pdf"), "google"));
  EXPECT_FALSE(IsSubDomain(GURL("about:blank"), "google"));
  EXPECT_FALSE(IsSubDomain(GURL("data:text/html,Hello"), "google"));
  EXPECT_FALSE(IsSubDomain(GURL("javascript:void(0)"), "google"));

  EXPECT_FALSE(IsSubDomain(GURL("invalid"), ""));
  EXPECT_FALSE(IsSubDomain(GURL(""), ""));
  EXPECT_FALSE(IsSubDomain(GURL("http://"), ""));
  EXPECT_FALSE(IsSubDomain(GURL("file:///home/chronos/user/file.pdf"), ""));
  EXPECT_FALSE(IsSubDomain(GURL("about:blank"), ""));
  EXPECT_FALSE(IsSubDomain(GURL("https://google.com"), ""));
}

TEST(UrlUtilsTest, IsSubDomainWithPathPrefixMatchesSubDomainAndPathPrefix) {
  EXPECT_TRUE(IsSubDomainWithPathPrefix(GURL("https://mail.google.com/chat"),
                                        "mail.google", "/chat"));
  EXPECT_TRUE(IsSubDomainWithPathPrefix(
      GURL("https://mail.google.com/chat/123"), "mail.google", "/chat"));
  EXPECT_TRUE(IsSubDomainWithPathPrefix(
      GURL("https://mail.google.com/chat?key=val"), "mail.google", "/chat"));
  EXPECT_TRUE(IsSubDomainWithPathPrefix(
      GURL("https://docs.google.com/document/d/123/edit"), "docs.google",
      "/document"));
  EXPECT_TRUE(IsSubDomainWithPathPrefix(GURL("https://b.corp.google.com/134"),
                                        "b.corp.google", "/134"));
  EXPECT_TRUE(IsSubDomainWithPathPrefix(GURL("http://localhost:8080/app/test"),
                                        "localhost", "/app"));
}

TEST(UrlUtilsTest, IsSubDomainWithPathPrefixRejectsNonMatchingPathPrefix) {
  EXPECT_FALSE(IsSubDomainWithPathPrefix(
      GURL("https://mail.google.com/mail/u/0"), "mail.google", "/chat"));
  EXPECT_FALSE(IsSubDomainWithPathPrefix(
      GURL("https://docs.google.com/spreadsheets/d/123"), "docs.google",
      "/document"));
  EXPECT_FALSE(IsSubDomainWithPathPrefix(GURL("https://mail.google.com/"),
                                         "mail.google", "/chat"));
  EXPECT_FALSE(IsSubDomainWithPathPrefix(GURL("https://mail.google.com"),
                                         "mail.google", "/chat"));
}

TEST(UrlUtilsTest, IsSubDomainWithPathPrefixRejectsNonMatchingDomain) {
  EXPECT_FALSE(IsSubDomainWithPathPrefix(GURL("https://other.com/chat"),
                                         "mail.google", "/chat"));
  EXPECT_FALSE(IsSubDomainWithPathPrefix(GURL("https://evilgoogle.com/chat"),
                                         "mail.google", "/chat"));
}

TEST(UrlUtilsTest, IsSubDomainWithPathPrefixHandlesInvalidUrlsAndMissingHosts) {
  EXPECT_FALSE(IsSubDomainWithPathPrefix(GURL("invalid"), "google", "/test"));
  EXPECT_FALSE(IsSubDomainWithPathPrefix(GURL(""), "google", "/test"));
  EXPECT_FALSE(IsSubDomainWithPathPrefix(GURL("file:///path/to/file"), "google",
                                         "/path"));
}

TEST(UrlUtilsTest, HasFileExtensionMatchesExtensions) {
  EXPECT_TRUE(HasFileExtension(GURL("https://example.com/file.pdf"), "pdf"));
  EXPECT_TRUE(
      HasFileExtension(GURL("file:///home/chronos/user/file.pdf"), "pdf"));
  EXPECT_TRUE(
      HasFileExtension(GURL("https://example.com/docs/file.docx"), "docx"));
}

TEST(UrlUtilsTest, HasFileExtensionIsCaseInsensitive) {
  EXPECT_TRUE(HasFileExtension(GURL("https://example.com/file.PDF"), "pdf"));
  EXPECT_TRUE(HasFileExtension(GURL("https://example.com/file.pdf"), "PDF"));
  EXPECT_TRUE(HasFileExtension(GURL("https://example.com/file.PdF"), "pDf"));
}

TEST(UrlUtilsTest, HasFileExtensionIgnoresQueryAndFragment) {
  EXPECT_TRUE(
      HasFileExtension(GURL("https://example.com/file.pdf?query=1"), "pdf"));
  EXPECT_TRUE(
      HasFileExtension(GURL("https://example.com/file.pdf#section"), "pdf"));
  EXPECT_TRUE(HasFileExtension(
      GURL("https://example.com/file.pdf?query=1#section"), "pdf"));
}

TEST(UrlUtilsTest, HasFileExtensionRejectsNonMatchingExtensions) {
  EXPECT_FALSE(HasFileExtension(GURL("https://example.com/file.txt"), "pdf"));
  EXPECT_FALSE(HasFileExtension(GURL("https://example.com/filepdf"), "pdf"));
  EXPECT_FALSE(
      HasFileExtension(GURL("https://example.com/file.pdf.bak"), "pdf"));
  EXPECT_FALSE(HasFileExtension(GURL("https://example.com/pdf"), "pdf"));
  EXPECT_FALSE(
      HasFileExtension(GURL("https://example.com/file.pdf/other"), "pdf"));
  EXPECT_FALSE(HasFileExtension(GURL("https://example.com/"), "pdf"));
  EXPECT_FALSE(HasFileExtension(GURL("invalid"), "pdf"));
  EXPECT_FALSE(HasFileExtension(GURL(""), "pdf"));
}

}  // namespace
}  // namespace ash::input_method

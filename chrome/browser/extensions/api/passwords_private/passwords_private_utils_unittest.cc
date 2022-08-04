// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"

#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

namespace {

using password_manager::CredentialUIEntry;

struct StringFirstLetterCmp {
  bool operator()(const std::string& lhs, const std::string& rhs) const {
    return lhs.empty() ? !rhs.empty() : lhs[0] < rhs[0];
  }
};

}  // namespace

TEST(CreateUrlCollectionFromFormTest, UrlsFromHtmlForm) {
  password_manager::PasswordForm html_form;
  html_form.url = GURL("http://example.com/LoginAuth");
  html_form.signon_realm = html_form.url.DeprecatedGetOriginAsURL().spec();

  api::passwords_private::UrlCollection html_urls =
      CreateUrlCollectionFromCredential(CredentialUIEntry(html_form));
  EXPECT_EQ(html_urls.signon_realm, "http://example.com/");
  EXPECT_EQ(html_urls.shown, "example.com");
  EXPECT_EQ(html_urls.link, "http://example.com/LoginAuth");
}

TEST(CreateUrlCollectionFromFormTest, UrlsFromFederatedForm) {
  password_manager::PasswordForm federated_form;
  federated_form.signon_realm = "federation://example.com/google.com";
  federated_form.url = GURL("https://example.com/");
  federated_form.federation_origin =
      url::Origin::Create(GURL("https://google.com/"));

  api::passwords_private::UrlCollection federated_urls =
      CreateUrlCollectionFromCredential(CredentialUIEntry(federated_form));
  EXPECT_EQ(federated_urls.signon_realm, "federation://example.com/google.com");
  EXPECT_EQ(federated_urls.shown, "example.com");
  EXPECT_EQ(federated_urls.link, "https://example.com/");
}

TEST(CreateUrlCollectionFromFormTest, UrlsFromAndroidFormWithoutDisplayName) {
  password_manager::PasswordForm android_form;
  android_form.signon_realm = "android://example@com.example.android";
  android_form.app_display_name.clear();

  api::passwords_private::UrlCollection android_urls =
      CreateUrlCollectionFromCredential(CredentialUIEntry(android_form));
  EXPECT_EQ("android://example@com.example.android", android_urls.signon_realm);
  EXPECT_EQ("android.example.com", android_urls.shown);
  EXPECT_EQ("https://play.google.com/store/apps/details?id=com.example.android",
            android_urls.link);
}

TEST(CreateUrlCollectionFromFormTest, UrlsFromAndroidFormWithAppName) {
  password_manager::PasswordForm android_form;
  android_form.signon_realm = "android://hash@com.example.android";
  android_form.app_display_name = "Example Android App";

  api::passwords_private::UrlCollection android_urls =
      CreateUrlCollectionFromCredential(CredentialUIEntry(android_form));
  EXPECT_EQ(android_urls.signon_realm, "android://hash@com.example.android");
  EXPECT_EQ("Example Android App", android_urls.shown);
  EXPECT_EQ("https://play.google.com/store/apps/details?id=com.example.android",
            android_urls.link);
}

TEST(CreateUrlCollectionFromGURLTest, UrlsFromGURL) {
  GURL url = GURL("https://example.com/login");
  api::passwords_private::UrlCollection urls = CreateUrlCollectionFromGURL(url);

  EXPECT_EQ(urls.shown, "example.com");
  EXPECT_EQ(urls.signon_realm, "https://example.com/");
  EXPECT_EQ(urls.link, "https://example.com/login");
}

TEST(IdGeneratorTest, GenerateIds) {
  using ::testing::Pointee;
  using ::testing::Eq;

  IdGenerator<std::string> id_generator;
  int foo_id = id_generator.GenerateId("foo");

  // Check idempotence.
  EXPECT_EQ(foo_id, id_generator.GenerateId("foo"));

  // Check TryGetKey(id) == s iff id == GenerateId(*s).
  EXPECT_THAT(id_generator.TryGetKey(foo_id), Pointee(Eq("foo")));
  EXPECT_EQ(nullptr, id_generator.TryGetKey(foo_id + 1));

  // Check that different sort keys result in different ids.
  int bar_id = id_generator.GenerateId("bar");
  int baz_id = id_generator.GenerateId("baz");
  EXPECT_NE(foo_id, bar_id);
  EXPECT_NE(bar_id, baz_id);

  EXPECT_THAT(id_generator.TryGetKey(bar_id), Pointee(Eq("bar")));
  EXPECT_THAT(id_generator.TryGetKey(baz_id), Pointee(Eq("baz")));

  // Check that due to clashing |KeyCompare|, new |key|s invalidate existing
  // |key|.
  IdGenerator<std::string, int32_t, StringFirstLetterCmp>
      clashing_id_generator_;
  int crab_id = clashing_id_generator_.GenerateId("crab");

  EXPECT_THAT(clashing_id_generator_.TryGetKey(crab_id), Pointee(Eq("crab")));

  int chromium_id = clashing_id_generator_.GenerateId("chromium");

  EXPECT_EQ(crab_id, chromium_id);
  EXPECT_THAT(clashing_id_generator_.TryGetKey(chromium_id),
              Pointee(Eq("chromium")));
}

}  // namespace extensions

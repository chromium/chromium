// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"

#include "components/autofill/core/common/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

TEST(CreateUrlCollectionFromFormTest, UrlsFromHtmlForm) {
  autofill::PasswordForm html_form;
  html_form.origin = GURL("http://example.com/LoginAuth");
  html_form.signon_realm = html_form.origin.GetOrigin().spec();

  api::passwords_private::UrlCollection html_urls =
      CreateUrlCollectionFromForm(html_form);
  EXPECT_EQ(html_urls.origin, "http://example.com/");
  EXPECT_EQ(html_urls.shown, "example.com");
  EXPECT_EQ(html_urls.link, "http://example.com/LoginAuth");
}

TEST(CreateUrlCollectionFromFormTest, UrlsFromFederatedForm) {
  autofill::PasswordForm federated_form;
  federated_form.signon_realm = "federation://example.com/google.com";
  federated_form.origin = GURL("https://example.com/");
  federated_form.federation_origin =
      url::Origin::Create(GURL("https://google.com/"));

  api::passwords_private::UrlCollection federated_urls =
      CreateUrlCollectionFromForm(federated_form);
  EXPECT_EQ(federated_urls.origin, "federation://example.com/google.com");
  EXPECT_EQ(federated_urls.shown, "example.com");
  EXPECT_EQ(federated_urls.link, "https://example.com/");
}

TEST(CreateUrlCollectionFromFormTest, UrlsFromAndroidFormWithoutDisplayName) {
  autofill::PasswordForm android_form;
  android_form.signon_realm = "android://example@com.example.android";
  android_form.app_display_name.clear();

  api::passwords_private::UrlCollection android_urls =
      CreateUrlCollectionFromForm(android_form);
  EXPECT_EQ("android://example@com.example.android", android_urls.origin);
  EXPECT_EQ("android.example.com", android_urls.shown);
  EXPECT_EQ("https://play.google.com/store/apps/details?id=com.example.android",
            android_urls.link);
}

TEST(CreateUrlCollectionFromFormTest, UrlsFromAndroidFormWithAppName) {
  autofill::PasswordForm android_form;
  android_form.signon_realm = "android://hash@com.example.android";
  android_form.app_display_name = "Example Android App";

  api::passwords_private::UrlCollection android_urls =
      CreateUrlCollectionFromForm(android_form);
  EXPECT_EQ(android_urls.origin, "android://hash@com.example.android");
  EXPECT_EQ("Example Android App", android_urls.shown);
  EXPECT_EQ("https://play.google.com/store/apps/details?id=com.example.android",
            android_urls.link);
}

TEST(SortKeyIdGeneratorTest, GenerateIds) {
  using ::testing::Pointee;
  using ::testing::Eq;

  SortKeyIdGenerator id_generator;
  int foo_id = id_generator.GenerateId("foo");

  // Check idempotence.
  EXPECT_EQ(foo_id, id_generator.GenerateId("foo"));

  // Check TryGetSortKey(id) == s iff id == GenerateId(*s).
  EXPECT_THAT(id_generator.TryGetSortKey(foo_id), Pointee(Eq("foo")));
  EXPECT_EQ(nullptr, id_generator.TryGetSortKey(foo_id + 1));

  // Check that different sort keys result in different ids.
  int bar_id = id_generator.GenerateId("bar");
  int baz_id = id_generator.GenerateId("baz");
  EXPECT_NE(foo_id, bar_id);
  EXPECT_NE(bar_id, baz_id);

  EXPECT_THAT(id_generator.TryGetSortKey(bar_id), Pointee(Eq("bar")));
  EXPECT_THAT(id_generator.TryGetSortKey(baz_id), Pointee(Eq("baz")));
}

}  // namespace extensions

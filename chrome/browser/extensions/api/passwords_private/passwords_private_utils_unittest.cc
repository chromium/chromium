// Copyright 2017 The Chromium Authors
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
      url::SchemeHostPort(GURL("https://google.com/"));

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

using ::testing::Eq;
using ::testing::Pointee;

class IdGeneratorTest : public ::testing::Test {
 public:
  IdGeneratorTest() = default;

  IdGenerator& id_generator() { return id_generator_; }

 private:
  IdGenerator id_generator_;
};

TEST_F(IdGeneratorTest, GenerateIds) {
  password_manager::PasswordForm form;
  form.url = GURL("http://foo.com/LoginAuth");
  form.signon_realm = "http://foo.com/";

  CredentialUIEntry credential(form);

  int foo_id = id_generator().GenerateId(credential);

  // Check idempotence.
  EXPECT_EQ(foo_id, id_generator().GenerateId(credential));

  // Check TryGetKey(id) == s iff id == GenerateId(*s).
  EXPECT_THAT(id_generator().TryGetKey(foo_id), Pointee(Eq(credential)));
  EXPECT_EQ(nullptr, id_generator().TryGetKey(foo_id + 1));
}

TEST_F(IdGeneratorTest, DifferentIdsForDifferentKeys) {
  password_manager::PasswordForm form1;
  form1.url = GURL("http://bar.com/LoginAuth");
  form1.signon_realm = "http://bar.com/";
  password_manager::PasswordForm form2;
  form2.url = GURL("http://baz_id.com/LoginAuth");
  form2.signon_realm = "http://baz_id.com/";

  CredentialUIEntry credential1(form1);
  CredentialUIEntry credential2(form2);

  int bar_id = id_generator().GenerateId(credential1);
  int baz_id = id_generator().GenerateId(credential2);
  EXPECT_NE(bar_id, baz_id);

  EXPECT_THAT(id_generator().TryGetKey(bar_id), Pointee(Eq(credential1)));
  EXPECT_THAT(id_generator().TryGetKey(baz_id), Pointee(Eq(credential2)));
}

TEST_F(IdGeneratorTest, UpdatedCacheWithNewGenerateId) {
  password_manager::PasswordForm form;
  form.url = GURL("http://foo.com/LoginAuth");
  form.signon_realm = "http://foo.com/";

  CredentialUIEntry credential(form);

  int id = id_generator().GenerateId(credential);
  EXPECT_THAT(id_generator().TryGetKey(id), Pointee(Eq(credential)));

  CredentialUIEntry updated_credential(credential);
  updated_credential.note = u"new note";

  int same_id = id_generator().GenerateId(updated_credential);

  EXPECT_EQ(id, same_id);
  EXPECT_THAT(id_generator().TryGetKey(id), Pointee(Eq(updated_credential)));
}

}  // namespace extensions

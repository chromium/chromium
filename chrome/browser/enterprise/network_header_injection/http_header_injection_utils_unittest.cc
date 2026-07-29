// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/network_header_injection/http_header_injection_utils.h"

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/network_header_injection/core/features.h"
#include "components/enterprise/network_header_injection/core/http_header_injection_rule.h"
#include "components/enterprise/network_header_injection/core/network_header_injection_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_custom_headers {

class HttpHeaderInjectionUtilsTest : public testing::Test {
 public:
  HttpHeaderInjectionUtilsTest() {
    scoped_feature_list_.InitAndEnableFeature(kHttpHeadersInjection);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that MaybeWrapTrustedURLLoaderHeaderClient does not wrap when no rules
// exist.
TEST_F(HttpHeaderInjectionUtilsTest,
       MaybeWrapTrustedURLLoaderHeaderClient_NoRules) {
  TestingProfile profile;
  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      header_client;

  MaybeWrapTrustedURLLoaderHeaderClient(&profile, &header_client);

  EXPECT_FALSE(header_client.is_valid());
}

// Tests that MaybeWrapTrustedURLLoaderHeaderClient wraps when rules exist.
TEST_F(HttpHeaderInjectionUtilsTest,
       MaybeWrapTrustedURLLoaderHeaderClient_WithRules) {
  TestingProfile profile;

  base::ListValue rules = base::ListValue().Append(
      base::DictValue()
          .Set(kKeyPatterns, base::ListValue().Append("example.com"))
          .Set(kKeyHeaders, base::ListValue().Append(
                                base::DictValue()
                                    .Set(kKeyHeaderName, "X-Custom-Header")
                                    .Set(kKeyHeaderValue, "value"))));

  profile.GetPrefs()->SetList(prefs::kHttpHeaderInjection, std::move(rules));

  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      header_client;

  MaybeWrapTrustedURLLoaderHeaderClient(&profile, &header_client);

  EXPECT_TRUE(header_client.is_valid());
}

// Tests that MaybeCreateWebSocketHeaderClient does not wrap when no rules
// exist.
TEST_F(HttpHeaderInjectionUtilsTest, MaybeCreateWebSocketHeaderClient_NoRules) {
  TestingProfile profile;
  mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client;

  MaybeCreateWebSocketHeaderClient(&profile, &header_client);

  EXPECT_FALSE(header_client.is_valid());
}

// Tests that MaybeCreateWebSocketHeaderClient wraps when rules exist.
TEST_F(HttpHeaderInjectionUtilsTest,
       MaybeCreateWebSocketHeaderClient_WithRules) {
  TestingProfile profile;

  base::ListValue rules = base::ListValue().Append(
      base::DictValue()
          .Set(kKeyPatterns, base::ListValue().Append("example.com"))
          .Set(kKeyHeaders, base::ListValue().Append(
                                base::DictValue()
                                    .Set(kKeyHeaderName, "X-Custom-Header")
                                    .Set(kKeyHeaderValue, "value"))));

  profile.GetPrefs()->SetList(prefs::kHttpHeaderInjection, std::move(rules));

  mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client;

  MaybeCreateWebSocketHeaderClient(&profile, &header_client);

  EXPECT_TRUE(header_client.is_valid());
}

}  // namespace enterprise_custom_headers

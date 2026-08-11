// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_verification_tokens/private_verification_tokens_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/private_verification_tokens/private_verification_tokens_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config.h"
#include "content/public/test/browser_test.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

class PrivateVerificationTokensServiceBrowserTest : public PlatformBrowserTest {
 public:
  PrivateVerificationTokensServiceBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kEnablePrivateVerificationTokens);
  }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceBrowserTest,
                       GetForProfile_FeatureEnabled_ReturnsInstance) {
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(GetProfile());
  EXPECT_TRUE(service);
}

class PrivateVerificationTokensServiceDisabledBrowserTest
    : public PlatformBrowserTest {
 public:
  PrivateVerificationTokensServiceDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        net::features::kEnablePrivateVerificationTokens);
  }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceDisabledBrowserTest,
                       GetForProfile_FeatureDisabled_ReturnsNull) {
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(GetProfile());
  EXPECT_FALSE(service);
}

class PrivateVerificationTokensServiceCustomIssuerBrowserTest
    : public PlatformBrowserTest {
 public:
  PrivateVerificationTokensServiceCustomIssuerBrowserTest() {
    const std::vector<uint8_t> serialized_public_key = {1, 2, 3, 4};
    const std::string encoded_public_key =
        base::Base64Encode(serialized_public_key);
    const std::vector<uint8_t> serialized_proof = {5, 6, 7};
    const std::string encoded_proof = base::Base64Encode(serialized_proof);
    const std::string custom_issuer_json = base::StringPrintf(
        R"({
          "issuerRequestUrl": "https://commandline.example.com/issue",
          "version": 1,
          "publicKey": "%s",
          "publicKeyProof": "%s",
          "batchSize": 7,
          "expiration": "2147483647",
          "redeemers": ["https://s1.commandline.example.com"],
          "deploymentId": "cmd-deployment-id"
        })",
        encoded_public_key.c_str(), encoded_proof.c_str());

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kEnablePrivateVerificationTokens,
        {{net::features::kPrivateVerificationTokensCustomIssuer.name,
          custom_issuer_json}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceCustomIssuerBrowserTest,
                       GlobalIssuerConfig_CustomIssuerFromCommandLine) {
  PrivateVerificationTokensServiceFactory::SetGlobalIssuerConfig(nullptr);
  auto config =
      PrivateVerificationTokensServiceFactory::GetGlobalIssuerConfig();
  ASSERT_TRUE(config);
  const url::Origin expected_origin =
      url::Origin::Create(GURL("https://commandline.example.com"));
  EXPECT_TRUE(config->config().contains(expected_origin));

  const auto& issuer_config = config->config().at(expected_origin);
  EXPECT_EQ(issuer_config.issuer_request_url,
            GURL("https://commandline.example.com/issue"));
  EXPECT_EQ(issuer_config.batch_size, 7);
  EXPECT_EQ(issuer_config.deployment_id, "cmd-deployment-id");
  EXPECT_THAT(issuer_config.redeemers,
              testing::ElementsAre(url::Origin::Create(
                  GURL("https://s1.commandline.example.com"))));

  const private_verification_tokens::PrivateVerificationTokensPublicKey
      expected_public_key{expected_origin,
                          {1, 2, 3, 4},
                          {5, 6, 7},
                          base::Time::UnixEpoch() + base::Seconds(2147483647),
                          1};
  EXPECT_EQ(issuer_config.public_key, expected_public_key);
}

}  // namespace

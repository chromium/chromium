// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_param.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kChallenge[] = "test_challenge";
}  // namespace

class BoundSessionRegistrationFetcherParamTest : public testing::Test {
 public:
  BoundSessionRegistrationFetcherParamTest() = default;
  BoundSessionRegistrationFetcherParamTest(
      const BoundSessionRegistrationFetcherParamTest&) = delete;
  BoundSessionRegistrationFetcherParamTest& operator=(
      const BoundSessionRegistrationFetcherParamTest&) = delete;
  ~BoundSessionRegistrationFetcherParamTest() override = default;
};

TEST_F(BoundSessionRegistrationFetcherParamTest, AllInvalid) {
  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos;
  BoundSessionRegistrationFetcherParam params =
      BoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
          GURL(), supported_algos, "");
}

TEST_F(BoundSessionRegistrationFetcherParamTest, AllValid) {
  GURL registration_request = GURL("https://www.google.com/registration");
  auto response_headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->SetHeader(
      "Sec-Session-Google-Registration",
      base::StrCat(
          {"registration=startsession; supported-alg=ES256,RS256; challenge=",
           kChallenge, ";"}));
  absl::optional<BoundSessionRegistrationFetcherParam> maybe_params =
      BoundSessionRegistrationFetcherParam::MaybeCreateInstance(
          registration_request, response_headers.get());
  ASSERT_TRUE(maybe_params.has_value());
  BoundSessionRegistrationFetcherParam params = std::move(maybe_params.value());
  ASSERT_EQ(params.RegistrationEndpoint(),
            GURL("https://www.google.com/startsession"));
  ASSERT_EQ(params.SupportedAlgos()[0],
            crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256);
  ASSERT_EQ(params.SupportedAlgos()[1],
            crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256);
  ASSERT_EQ(params.Challenge(), kChallenge);
}

TEST_F(BoundSessionRegistrationFetcherParamTest, AllValidFullUrl) {
  GURL registration_request = GURL("https://www.google.com/registration");
  auto response_headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->SetHeader(
      "Sec-Session-Google-Registration",
      base::StrCat({"registration=https://accounts.google.com/"
                    "startsession; supported-alg=ES256,RS256; challenge=",
                    kChallenge, ";"}));
  absl::optional<BoundSessionRegistrationFetcherParam> maybe_params =
      BoundSessionRegistrationFetcherParam::MaybeCreateInstance(
          registration_request, response_headers.get());
  ASSERT_TRUE(maybe_params.has_value());
  BoundSessionRegistrationFetcherParam params = std::move(maybe_params.value());
  ASSERT_EQ(params.RegistrationEndpoint(),
            GURL("https://accounts.google.com/startsession"));
  ASSERT_EQ(params.SupportedAlgos()[0],
            crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256);
  ASSERT_EQ(params.SupportedAlgos()[1],
            crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256);
  ASSERT_EQ(params.Challenge(), kChallenge);
}

TEST_F(BoundSessionRegistrationFetcherParamTest, AllValidFullDifferentUrl) {
  GURL registration_request = GURL("https://www.google.com/registration");
  auto response_headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->SetHeader(
      "Sec-Session-Google-Registration",
      base::StrCat({"registration=https://accounts.different.url/"
                    "startsession; supported-alg=ES256,RS256; challenge=",
                    kChallenge, ";"}));
  absl::optional<BoundSessionRegistrationFetcherParam> maybe_params =
      BoundSessionRegistrationFetcherParam::MaybeCreateInstance(
          registration_request, response_headers.get());
  ASSERT_FALSE(maybe_params.has_value());
}

TEST_F(BoundSessionRegistrationFetcherParamTest, AllValidSwapAlgo) {
  GURL registration_request = GURL("https://www.google.com/registration");
  auto response_headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->SetHeader(
      "Sec-Session-Google-Registration",
      base::StrCat(
          {"registration=startsession; supported-alg=RS256,ES256; challenge=",
           kChallenge, ";"}));
  absl::optional<BoundSessionRegistrationFetcherParam> maybe_params =
      BoundSessionRegistrationFetcherParam::MaybeCreateInstance(
          registration_request, response_headers.get());
  ASSERT_TRUE(maybe_params.has_value());
  BoundSessionRegistrationFetcherParam params = std::move(maybe_params.value());
  ASSERT_EQ(params.RegistrationEndpoint(),
            GURL("https://www.google.com/startsession"));
  ASSERT_EQ(params.SupportedAlgos()[0],
            crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256);
  ASSERT_EQ(params.SupportedAlgos()[1],
            crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256);
  ASSERT_EQ(params.Challenge(), kChallenge);
}

TEST_F(BoundSessionRegistrationFetcherParamTest, AllValidOneAlgo) {
  GURL registration_request = GURL("https://www.google.com/registration");
  auto response_headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->SetHeader(
      "Sec-Session-Google-Registration",
      base::StrCat(
          {"registration=startsession; supported-alg=RS256; challenge=",
           kChallenge, ";"}));
  absl::optional<BoundSessionRegistrationFetcherParam> maybe_params =
      BoundSessionRegistrationFetcherParam::MaybeCreateInstance(
          registration_request, response_headers.get());
  ASSERT_TRUE(maybe_params.has_value());
  BoundSessionRegistrationFetcherParam params = std::move(maybe_params.value());
  ASSERT_EQ(params.RegistrationEndpoint(),
            GURL("https://www.google.com/startsession"));
  ASSERT_EQ(params.SupportedAlgos()[0],
            crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256);
  ASSERT_EQ(params.Challenge(), kChallenge);
}

TEST_F(BoundSessionRegistrationFetcherParamTest, MissingHeader) {
  GURL registration_request = GURL("https://www.google.com/registration");
  auto response_headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  // Note: not adding the right header, causing absl::nullopt to be returned.
  absl::optional<BoundSessionRegistrationFetcherParam> maybe_params =
      BoundSessionRegistrationFetcherParam::MaybeCreateInstance(
          registration_request, response_headers.get());
  ASSERT_FALSE(maybe_params.has_value());
}

TEST_F(BoundSessionRegistrationFetcherParamTest, MissingUrl) {
  GURL registration_request = GURL();
  auto response_headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->SetHeader(
      "Sec-Session-Google-Registration",
      base::StrCat(
          {"registration=startsession; supported-alg=ES256,RS256; challenge=",
           kChallenge, ";"}));
  absl::optional<BoundSessionRegistrationFetcherParam> maybe_params =
      BoundSessionRegistrationFetcherParam::MaybeCreateInstance(
          registration_request, response_headers.get());
  ASSERT_FALSE(maybe_params.has_value());
}

TEST_F(BoundSessionRegistrationFetcherParamTest, MissingAlgo) {
  GURL registration_request = GURL("https://www.google.com/registration");
  auto response_headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->SetHeader(
      "Sec-Session-Google-Registration",
      base::StrCat({"registration=startsession; supported-alg=; challenge=",
                    kChallenge, ";"}));
  absl::optional<BoundSessionRegistrationFetcherParam> maybe_params =
      BoundSessionRegistrationFetcherParam::MaybeCreateInstance(
          registration_request, response_headers.get());
  ASSERT_FALSE(maybe_params.has_value());
}

TEST_F(BoundSessionRegistrationFetcherParamTest, MissingRegistration) {
  GURL registration_request = GURL("https://www.google.com/registration");
  auto response_headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->SetHeader(
      "Sec-Session-Google-Registration",
      base::StrCat({"supported-alg=ES256,RS256; challenge=", kChallenge, ";"}));
  absl::optional<BoundSessionRegistrationFetcherParam> maybe_params =
      BoundSessionRegistrationFetcherParam::MaybeCreateInstance(
          registration_request, response_headers.get());
  ASSERT_FALSE(maybe_params.has_value());
}

TEST_F(BoundSessionRegistrationFetcherParamTest, MissingChallenge) {
  GURL registration_request = GURL("https://www.google.com/registration");
  auto response_headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->SetHeader(
      "Sec-Session-Google-Registration",
      "registration=startsession; supported-alg=ES256,RS256");
  absl::optional<BoundSessionRegistrationFetcherParam> maybe_params =
      BoundSessionRegistrationFetcherParam::MaybeCreateInstance(
          registration_request, response_headers.get());
  ASSERT_FALSE(maybe_params.has_value());
}

TEST_F(BoundSessionRegistrationFetcherParamTest, EmptyChallenge) {
  GURL registration_request = GURL("https://www.google.com/registration");
  auto response_headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->SetHeader(
      "Sec-Session-Google-Registration",
      "registration=startsession; supported-alg=ES256,RS256; challenge=;");
  absl::optional<BoundSessionRegistrationFetcherParam> maybe_params =
      BoundSessionRegistrationFetcherParam::MaybeCreateInstance(
          registration_request, response_headers.get());
  ASSERT_FALSE(maybe_params.has_value());
}

TEST_F(BoundSessionRegistrationFetcherParamTest, ChallengeInvalidUtf8) {
  GURL registration_request = GURL("https://www.google.com/registration");
  auto response_headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->SetHeader(
      "Sec-Session-Google-Registration",
      "registration=startsession; supported-alg=ES256,RS256; "
      "challenge=ab\xC0\x80;");
  absl::optional<BoundSessionRegistrationFetcherParam> maybe_params =
      BoundSessionRegistrationFetcherParam::MaybeCreateInstance(
          registration_request, response_headers.get());
  ASSERT_FALSE(maybe_params.has_value());
}

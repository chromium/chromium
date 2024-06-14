// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_param.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher.h"
#include "crypto/signature_verifier.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::ElementsAre;
using enum crypto::SignatureVerifier::SignatureAlgorithm;

constexpr char kChallenge[] = "Y2hhbGxlbmdl";
constexpr char kChallenge2[] = "Y2hhbGxlbmdlMg==";

class BoundSessionRegistrationFetcherParamTest : public testing::Test {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      kBoundSessionRegistrationListHeaderSupport};
};

TEST_F(BoundSessionRegistrationFetcherParamTest, AllInvalid) {
  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos;
  BoundSessionRegistrationFetcherParam params =
      BoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
          GURL(), supported_algos, "");
}

TEST_F(BoundSessionRegistrationFetcherParamTest, AllValid) {
  GURL registration_request = GURL("https://www.google.com/registration");
  std::vector<scoped_refptr<net::HttpResponseHeaders>> test_cases = {
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration",
                     "registration=startsession; supported-alg=ES256,RS256; "
                     "challenge=Y2hhbGxlbmdl;")
          .Build(),
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader(
              "Sec-Session-Google-Registration-List",
              "(ES256 RS256);path=\"startsession\";challenge=\"Y2hhbGxlbmdl\"")
          .Build(),
  };

  for (size_t i = 0; i < test_cases.size(); ++i) {
    SCOPED_TRACE(i);
    std::vector<BoundSessionRegistrationFetcherParam> maybe_params =
        BoundSessionRegistrationFetcherParam::CreateFromHeaders(
            registration_request, test_cases[i].get());

    ASSERT_EQ(maybe_params.size(), 1U);
    const BoundSessionRegistrationFetcherParam& params = maybe_params[0];
    EXPECT_EQ(params.registration_endpoint(),
              GURL("https://www.google.com/startsession"));
    EXPECT_THAT(params.supported_algos(),
                ElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
    EXPECT_EQ(params.challenge(), kChallenge);
  }
}

TEST_F(BoundSessionRegistrationFetcherParamTest, AllValidFullUrl) {
  GURL registration_request = GURL("https://www.google.com/registration");
  std::vector<scoped_refptr<net::HttpResponseHeaders>> test_cases = {
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration",
                     "registration=https://accounts.google.com/startsession; "
                     "supported-alg=ES256,RS256; challenge=Y2hhbGxlbmdl;")
          .Build(),
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader(
              "Sec-Session-Google-Registration-List",
              "(ES256 RS256);path=\"https://accounts.google.com/startsession\";"
              "challenge=\"Y2hhbGxlbmdl\"")
          .Build(),
  };

  for (size_t i = 0; i < test_cases.size(); ++i) {
    SCOPED_TRACE(i);
    std::vector<BoundSessionRegistrationFetcherParam> maybe_params =
        BoundSessionRegistrationFetcherParam::CreateFromHeaders(
            registration_request, test_cases[i].get());

    ASSERT_EQ(maybe_params.size(), 1U);
    const BoundSessionRegistrationFetcherParam& params = maybe_params[0];
    EXPECT_EQ(params.registration_endpoint(),
              GURL("https://accounts.google.com/startsession"));
    EXPECT_THAT(params.supported_algos(),
                ElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
    EXPECT_EQ(params.challenge(), kChallenge);
  }
}

TEST_F(BoundSessionRegistrationFetcherParamTest, AllValidFullDifferentUrl) {
  GURL registration_request = GURL("https://www.google.com/registration");
  std::vector<scoped_refptr<net::HttpResponseHeaders>> test_cases = {
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration",
                     "registration=https://accounts.different.url/startsession;"
                     "supported-alg=ES256,RS256; challenge=Y2hhbGxlbmdl;")
          .Build(),
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration-List",
                     "(ES256 RS256);"
                     "path=\"https://accounts.different.url/startsession\";"
                     "challenge=\"Y2hhbGxlbmdl\"")
          .Build(),
  };

  for (size_t i = 0; i < test_cases.size(); ++i) {
    SCOPED_TRACE(i);
    std::vector<BoundSessionRegistrationFetcherParam> maybe_params =
        BoundSessionRegistrationFetcherParam::CreateFromHeaders(
            registration_request, test_cases[i].get());
    EXPECT_THAT(maybe_params, testing::IsEmpty());
  }
}

TEST_F(BoundSessionRegistrationFetcherParamTest, AllValidEmptyRegistration) {
  GURL registration_request = GURL("https://www.google.com/registration");
  std::vector<scoped_refptr<net::HttpResponseHeaders>> test_cases = {
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration",
                     "registration=; supported-alg=ES256,RS256; "
                     "challenge=Y2hhbGxlbmdl;")
          .Build(),
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration-List",
                     "(ES256 RS256);path=\"\";challenge=\"Y2hhbGxlbmdl\"")
          .Build(),
  };

  for (size_t i = 0; i < test_cases.size(); ++i) {
    SCOPED_TRACE(i);
    std::vector<BoundSessionRegistrationFetcherParam> maybe_params =
        BoundSessionRegistrationFetcherParam::CreateFromHeaders(
            registration_request, test_cases[i].get());

    ASSERT_EQ(maybe_params.size(), 1U);
    const BoundSessionRegistrationFetcherParam& params = maybe_params[0];
    EXPECT_EQ(params.registration_endpoint(),
              GURL("https://www.google.com/registration"));
    EXPECT_THAT(params.supported_algos(),
                ElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
    EXPECT_EQ(params.challenge(), kChallenge);
  }
}

TEST_F(BoundSessionRegistrationFetcherParamTest, AllValidSwapAlgo) {
  GURL registration_request = GURL("https://www.google.com/registration");
  std::vector<scoped_refptr<net::HttpResponseHeaders>> test_cases = {
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration",
                     "registration=startsession; supported-alg=RS256,ES256; "
                     "challenge=Y2hhbGxlbmdl;")
          .Build(),
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader(
              "Sec-Session-Google-Registration-List",
              "(RS256 ES256);path=\"startsession\";challenge=\"Y2hhbGxlbmdl\"")
          .Build(),
  };

  for (size_t i = 0; i < test_cases.size(); ++i) {
    SCOPED_TRACE(i);
    std::vector<BoundSessionRegistrationFetcherParam> maybe_params =
        BoundSessionRegistrationFetcherParam::CreateFromHeaders(
            registration_request, test_cases[i].get());

    ASSERT_EQ(maybe_params.size(), 1U);
    const BoundSessionRegistrationFetcherParam& params = maybe_params[0];
    EXPECT_EQ(params.registration_endpoint(),
              GURL("https://www.google.com/startsession"));
    EXPECT_THAT(params.supported_algos(),
                ElementsAre(RSA_PKCS1_SHA256, ECDSA_SHA256));
    EXPECT_EQ(params.challenge(), kChallenge);
  }
}

TEST_F(BoundSessionRegistrationFetcherParamTest, AllValidOneAlgo) {
  GURL registration_request = GURL("https://www.google.com/registration");
  std::vector<scoped_refptr<net::HttpResponseHeaders>> test_cases = {
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration",
                     "registration=startsession; supported-alg=RS256; "
                     "challenge=Y2hhbGxlbmdl;")
          .Build(),
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration-List",
                     "(RS256);path=\"startsession\";challenge=\"Y2hhbGxlbmdl\"")
          .Build(),
  };

  for (size_t i = 0; i < test_cases.size(); ++i) {
    SCOPED_TRACE(i);
    std::vector<BoundSessionRegistrationFetcherParam> maybe_params =
        BoundSessionRegistrationFetcherParam::CreateFromHeaders(
            registration_request, test_cases[i].get());

    ASSERT_EQ(maybe_params.size(), 1U);
    const BoundSessionRegistrationFetcherParam& params = maybe_params[0];
    EXPECT_EQ(params.registration_endpoint(),
              GURL("https://www.google.com/startsession"));
    EXPECT_THAT(params.supported_algos(), ElementsAre(RSA_PKCS1_SHA256));
    EXPECT_EQ(params.challenge(), kChallenge);
  }
}

TEST_F(BoundSessionRegistrationFetcherParamTest, AllValidUnrecognizedAlgo) {
  GURL registration_request = GURL("https://www.google.com/registration");
  std::vector<scoped_refptr<net::HttpResponseHeaders>> test_cases = {
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration",
                     "registration=startsession; supported-alg=RS256;BF512; "
                     "challenge=Y2hhbGxlbmdl;")
          .Build(),
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader(
              "Sec-Session-Google-Registration-List",
              "(RS256 BF512);path=\"startsession\";challenge=\"Y2hhbGxlbmdl\"")
          .Build(),
  };

  for (size_t i = 0; i < test_cases.size(); ++i) {
    SCOPED_TRACE(i);
    std::vector<BoundSessionRegistrationFetcherParam> maybe_params =
        BoundSessionRegistrationFetcherParam::CreateFromHeaders(
            registration_request, test_cases[i].get());

    ASSERT_EQ(maybe_params.size(), 1U);
    const BoundSessionRegistrationFetcherParam& params = maybe_params[0];
    EXPECT_EQ(params.registration_endpoint(),
              GURL("https://www.google.com/startsession"));
    EXPECT_THAT(params.supported_algos(), ElementsAre(RSA_PKCS1_SHA256));
    EXPECT_EQ(params.challenge(), kChallenge);
  }
}

TEST_F(BoundSessionRegistrationFetcherParamTest, MultipleValidRegistrations) {
  GURL registration_request = GURL("https://www.google.com/registration");
  std::vector<scoped_refptr<net::HttpResponseHeaders>> test_cases = {
      // Two sessions in one header.
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader(
              "Sec-Session-Google-Registration-List",
              "(ES256 RS256);path=\"startsession\";challenge=\"Y2hhbGxlbmdl\","
              "(ES256);path=\"startsession2\";challenge=\"Y2hhbGxlbmdlMg==\"")
          .Build(),
      // Two sessions in two headers.
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader(
              "Sec-Session-Google-Registration-List",
              "(ES256 RS256);path=\"startsession\";challenge=\"Y2hhbGxlbmdl\"")
          .AddHeader(
              "Sec-Session-Google-Registration-List",
              "(ES256);path=\"startsession2\";challenge=\"Y2hhbGxlbmdlMg==\"")
          .Build(),
  };

  for (size_t i = 0; i < test_cases.size(); ++i) {
    SCOPED_TRACE(i);
    std::vector<BoundSessionRegistrationFetcherParam> maybe_params =
        BoundSessionRegistrationFetcherParam::CreateFromHeaders(
            registration_request, test_cases[i].get());

    ASSERT_EQ(maybe_params.size(), 2U);
    const BoundSessionRegistrationFetcherParam& params = maybe_params[0];
    EXPECT_EQ(params.registration_endpoint(),
              GURL("https://www.google.com/startsession"));
    EXPECT_THAT(params.supported_algos(),
                ElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
    EXPECT_EQ(params.challenge(), kChallenge);
    const BoundSessionRegistrationFetcherParam& params2 = maybe_params[1];
    EXPECT_EQ(params2.registration_endpoint(),
              GURL("https://www.google.com/startsession2"));
    EXPECT_THAT(params2.supported_algos(), ElementsAre(ECDSA_SHA256));
    EXPECT_EQ(params2.challenge(), kChallenge2);
  }
}

TEST_F(BoundSessionRegistrationFetcherParamTest, ValidAndInvalid) {
  GURL registration_request = GURL("https://www.google.com/registration");
  std::vector<scoped_refptr<net::HttpResponseHeaders>> test_cases = {
      // Two sessions in one header.
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader(
              "Sec-Session-Google-Registration-List",
              "();path=\"startsession\","
              "(ES256);path=\"startsession2\";challenge=\"Y2hhbGxlbmdlMg==\"")
          .Build(),
      // Two sessions in two headers.
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration-List",
                     "();path=\"startsession\"")
          .AddHeader(
              "Sec-Session-Google-Registration-List",
              "(ES256);path=\"startsession2\";challenge=\"Y2hhbGxlbmdlMg==\"")
          .Build(),
  };

  for (size_t i = 0; i < test_cases.size(); ++i) {
    SCOPED_TRACE(i);
    std::vector<BoundSessionRegistrationFetcherParam> maybe_params =
        BoundSessionRegistrationFetcherParam::CreateFromHeaders(
            registration_request, test_cases[i].get());

    ASSERT_EQ(maybe_params.size(), 1U);
    const BoundSessionRegistrationFetcherParam& params = maybe_params[0];
    EXPECT_EQ(params.registration_endpoint(),
              GURL("https://www.google.com/startsession2"));
    EXPECT_THAT(params.supported_algos(), ElementsAre(ECDSA_SHA256));
    EXPECT_EQ(params.challenge(), kChallenge2);
  }
}

TEST_F(BoundSessionRegistrationFetcherParamTest,
       ListHeaderOverridesLegacyHeader) {
  GURL registration_request = GURL("https://www.google.com/registration");
  auto response_headers =
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration",
                     "registration=startsession; supported-alg=ES256,RS256; "
                     "challenge=Y2hhbGxlbmdl;")
          .AddHeader(
              "Sec-Session-Google-Registration-List",
              "(ES256);path=\"startsession2\";challenge=\"Y2hhbGxlbmdlMg==\"")
          .Build();
  std::vector<BoundSessionRegistrationFetcherParam> maybe_params =
      BoundSessionRegistrationFetcherParam::CreateFromHeaders(
          registration_request, response_headers.get());

  ASSERT_EQ(maybe_params.size(), 1U);
  const BoundSessionRegistrationFetcherParam& params = maybe_params[0];
  EXPECT_EQ(params.registration_endpoint(),
            GURL("https://www.google.com/startsession2"));
  EXPECT_THAT(params.supported_algos(), ElementsAre(ECDSA_SHA256));
  EXPECT_EQ(params.challenge(), kChallenge2);
}

TEST_F(BoundSessionRegistrationFetcherParamTest,
       InvalidListHeaderOverridesLegacyHeader) {
  GURL registration_request = GURL("https://www.google.com/registration");
  auto response_headers =
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration",
                     "registration=startsession; supported-alg=ES256,RS256; "
                     "challenge=Y2hhbGxlbmdl;")
          // Invalid because of missing parameters.
          .AddHeader("Sec-Session-Google-Registration-List",
                     "();path=\"startsession\"")
          .Build();

  std::vector<BoundSessionRegistrationFetcherParam> maybe_params =
      BoundSessionRegistrationFetcherParam::CreateFromHeaders(
          registration_request, response_headers.get());
  EXPECT_THAT(maybe_params, testing::IsEmpty());
}

// List header is ignored when the `kBoundSessionRegistrationListHeaderSupport`
// is disabled.
TEST(BoundSessionRegistrationFetcherParamListHeaderDisabledTest,
     ListHeaderIgnored) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      kBoundSessionRegistrationListHeaderSupport);
  GURL registration_request = GURL("https://www.google.com/registration");
  auto response_headers =
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration",
                     "registration=startsession; supported-alg=ES256,RS256; "
                     "challenge=Y2hhbGxlbmdl;")
          .AddHeader(
              "Sec-Session-Google-Registration-List",
              "(ES256);path=\"startsession2\";challenge=\"Y2hhbGxlbmdlMg==\"")
          .Build();
  std::vector<BoundSessionRegistrationFetcherParam> maybe_params =
      BoundSessionRegistrationFetcherParam::CreateFromHeaders(
          registration_request, response_headers.get());

  ASSERT_EQ(maybe_params.size(), 1U);
  const BoundSessionRegistrationFetcherParam& params = maybe_params[0];
  EXPECT_EQ(params.registration_endpoint(),
            GURL("https://www.google.com/startsession"));
  EXPECT_THAT(params.supported_algos(),
              ElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(params.challenge(), kChallenge);
}

TEST_F(BoundSessionRegistrationFetcherParamTest, MissingHeader) {
  GURL registration_request = GURL("https://www.google.com/registration");
  auto response_headers =
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200").Build();
  // Note: not adding the right header, causing an empty vector to be returned.
  std::vector<BoundSessionRegistrationFetcherParam> maybe_params =
      BoundSessionRegistrationFetcherParam::CreateFromHeaders(
          registration_request, response_headers.get());
  EXPECT_THAT(maybe_params, testing::IsEmpty());
}

TEST_F(BoundSessionRegistrationFetcherParamTest, MissingUrl) {
  GURL registration_request = GURL();
  std::vector<scoped_refptr<net::HttpResponseHeaders>> test_cases = {
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration",
                     "registration=startsession; supported-alg=ES256,RS256; "
                     "challenge=Y2hhbGxlbmdl;")
          .Build(),
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader(
              "Sec-Session-Google-Registration-List",
              "(ES256 RS256);path=\"startsession\";challenge=\"Y2hhbGxlbmdl\"")
          .Build(),
  };

  for (size_t i = 0; i < test_cases.size(); ++i) {
    SCOPED_TRACE(i);
    std::vector<BoundSessionRegistrationFetcherParam> maybe_params =
        BoundSessionRegistrationFetcherParam::CreateFromHeaders(
            registration_request, test_cases[i].get());
    EXPECT_THAT(maybe_params, testing::IsEmpty());
  }
}

TEST_F(BoundSessionRegistrationFetcherParamTest, MissingAlgo) {
  GURL registration_request = GURL("https://www.google.com/registration");
  std::vector<scoped_refptr<net::HttpResponseHeaders>> test_cases = {
      // Parameter is absent.
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration",
                     "registration=startsession; challenge=Y2hhbGxlbmdl;")
          .Build(),
      // Parameter is empty.
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration",
                     "registration=startsession; supported-alg=; "
                     "challenge=Y2hhbGxlbmdl;")
          .Build(),
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration-List",
                     "();path=\"startsession\";challenge=\"Y2hhbGxlbmdl\"")
          .Build(),
  };

  for (size_t i = 0; i < test_cases.size(); ++i) {
    SCOPED_TRACE(i);
    std::vector<BoundSessionRegistrationFetcherParam> maybe_params =
        BoundSessionRegistrationFetcherParam::CreateFromHeaders(
            registration_request, test_cases[i].get());
    EXPECT_THAT(maybe_params, testing::IsEmpty());
  }
}

TEST_F(BoundSessionRegistrationFetcherParamTest, AbsentRegistration) {
  GURL registration_request = GURL("https://www.google.com/registration");
  std::vector<scoped_refptr<net::HttpResponseHeaders>> test_cases = {
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration",
                     "supported-alg=ES256,RS256; challenge=Y2hhbGxlbmdl;")
          .Build(),
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration-List",
                     "(ES256 RS256);challenge=\"Y2hhbGxlbmdl\"")
          .Build()};

  for (size_t i = 0; i < test_cases.size(); ++i) {
    SCOPED_TRACE(i);
    std::vector<BoundSessionRegistrationFetcherParam> maybe_params =
        BoundSessionRegistrationFetcherParam::CreateFromHeaders(
            registration_request, test_cases[i].get());
    EXPECT_THAT(maybe_params, testing::IsEmpty());
  }
}

TEST_F(BoundSessionRegistrationFetcherParamTest, MissingChallenge) {
  GURL registration_request = GURL("https://www.google.com/registration");
  std::vector<scoped_refptr<net::HttpResponseHeaders>> test_cases = {
      // Parameter is absent.
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration",
                     "registration=startsession; supported-alg=ES256,RS256;")
          .Build(),
      // Parameter is empty.
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration",
                     "registration=startsession; supported-alg=ES256,RS256; "
                     "challenge=;")
          .Build(),
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration-List",
                     "(ES256 RS256);path=\"startsession\"")
          .Build(),
  };
  for (size_t i = 0; i < test_cases.size(); ++i) {
    SCOPED_TRACE(i);
    std::vector<BoundSessionRegistrationFetcherParam> maybe_params =
        BoundSessionRegistrationFetcherParam::CreateFromHeaders(
            registration_request, test_cases[i].get());
    EXPECT_THAT(maybe_params, testing::IsEmpty());
  }
}

TEST_F(BoundSessionRegistrationFetcherParamTest, InvalidChallenge) {
  GURL registration_request = GURL("https://www.google.com/registration");
  std::vector<scoped_refptr<net::HttpResponseHeaders>> test_cases = {
      // Non UTF-8 characters.
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration",
                     "registration=startsession; supported-alg=ES256,RS256; "
                     "challenge=ab\xC0\x80;")
          .Build(),
      // Non UTF-8 characters.
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader(
              "Sec-Session-Google-Registration-List",
              "(ES256 RS256);path=\"startsession\";challenge=\"ab\xC0\x80\"")
          .Build(),
      // Byte sequence instead of a string.
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200")
          .AddHeader("Sec-Session-Google-Registration-List",
                     "(ES256 RS256);path=\"startsession\";challenge=:00ff:")
          .Build(),
  };

  for (size_t i = 0; i < test_cases.size(); ++i) {
    SCOPED_TRACE(i);
    std::vector<BoundSessionRegistrationFetcherParam> maybe_params =
        BoundSessionRegistrationFetcherParam::CreateFromHeaders(
            registration_request, test_cases[i].get());
    EXPECT_THAT(maybe_params, testing::IsEmpty());
  }
}

}  // namespace

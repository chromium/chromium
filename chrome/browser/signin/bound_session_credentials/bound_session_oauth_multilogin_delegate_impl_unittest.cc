// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_oauth_multilogin_delegate_impl.h"

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_key.h"
#include "chrome/browser/signin/bound_session_credentials/mock_bound_session_cookie_refresh_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "google_apis/gaia/oauth_multilogin_result.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::base::test::EqualsProto;

using ::bound_session_credentials::BoundSessionParams;

OAuthMultiloginResult CreateOAuthMultiloginResult(
    const std::string& raw_data,
    net::HttpStatusCode status = net::HTTP_OK) {
  return OAuthMultiloginResult(
      raw_data, status,
      /*cookie_decryptor=*/
      base::BindLambdaForTesting([](std::string_view encrypted_cookie) {
        return base::StrCat({encrypted_cookie, ".decrypted"});
      }));
}

bound_session_credentials::Credential CreateCookieCredential(
    std::string_view name,
    std::string_view domain,
    std::string_view path) {
  bound_session_credentials::Credential credential;
  bound_session_credentials::CookieCredential* cookie =
      credential.mutable_cookie_credential();
  cookie->set_name(name);
  cookie->set_domain(domain);
  cookie->set_path(path);
  return credential;
}

class BoundSessionOAuthMultiLoginDelegateImplTest : public testing::Test {
 protected:
  signin::BoundSessionOAuthMultiLoginDelegate& delegate() {
    if (!delegate_) {
      delegate_ = std::make_unique<BoundSessionOAuthMultiLoginDelegateImpl>(
          mock_bound_session_cookie_refresh_service_.GetWeakPtr(),
          identity_test_environment_.identity_manager());
    }
    return *delegate_;
  }

  MockBoundSessionCookieRefreshService&
  mock_bound_session_cookie_refresh_service() {
    return mock_bound_session_cookie_refresh_service_;
  }

  void Signin(const std::vector<uint8_t>& wrapped_key) {
    identity_test_environment_.MakeAccountAvailable(
        signin::AccountAvailabilityOptionsBuilder()
            .WithRefreshTokenBindingKey(wrapped_key)
            .Build("test@email.com"));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
  MockBoundSessionCookieRefreshService
      mock_bound_session_cookie_refresh_service_;
  std::unique_ptr<BoundSessionOAuthMultiLoginDelegateImpl> delegate_;
};

TEST_F(BoundSessionOAuthMultiLoginDelegateImplTest,
       BeforeSetCookiesNoBoundSessions) {
  Signin(/*wrapped_key=*/{1, 2, 3});
  const std::string raw_data =
      R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name": "__Secure-1PSIDTS",
              "value": "secure-1p-sidts-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            }
          ],
          "token_binding_directed_response": {}
        }
      )";
  const OAuthMultiloginResult result = CreateOAuthMultiloginResult(raw_data);

  EXPECT_CALL(mock_bound_session_cookie_refresh_service(), StopCookieRotation)
      .Times(0);

  delegate().BeforeSetCookies(result);
}

TEST_F(BoundSessionOAuthMultiLoginDelegateImplTest,
       BeforeSetCookiesNoBindingKeyToReuse) {
  base::HistogramTester histogram_tester;

  Signin(/*wrapped_key=*/{});
  const std::string raw_data =
      R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name": "__Secure-1PSIDTS",
              "value": "secure-1p-sidts-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            }
          ],
          "token_binding_directed_response": {},
          "device_bound_session_info": [
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true,
              "register_session_payload": {
                "session_identifier": "id",
                "credentials": [
                  {
                    "type": "cookie",
                    "name": "__Secure-1PSIDTS",
                    "scope": {
                      "domain": ".google.com",
                      "path": "/"
                    }
                  }
                ],
                "refresh_url": "/RotateBoundCookies"
              }
            }
          ]
        }
      )";
  const OAuthMultiloginResult result = CreateOAuthMultiloginResult(raw_data);

  EXPECT_CALL(mock_bound_session_cookie_refresh_service(), StopCookieRotation)
      .Times(0);

  delegate().BeforeSetCookies(result);

  histogram_tester.ExpectUniqueSample(
      "Signin.BoundSessionCredentials.OAuthMultilogin.BindingKeyMissing",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST_F(BoundSessionOAuthMultiLoginDelegateImplTest,
       BeforeSetCookiesStopsCookiesRotation) {
  base::HistogramTester histogram_tester;

  Signin(/*wrapped_key=*/{1, 2, 3});

  const std::string raw_data =
      R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name": "__Secure-1PSIDTS",
              "value": "secure-1p-sidts-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            },
            {
              "name": "__Secure-Google-Cookie",
              "value": "secure-google-cookie-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            }
          ],
          "token_binding_directed_response": {},
          "device_bound_session_info": [
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true,
              "register_session_payload": {
                "session_identifier": "id_1",
                "credentials": [
                  {
                    "type": "cookie",
                    "name": "__Secure-1PSIDTS",
                    "scope": {
                      "domain": ".google.com",
                      "path": "/"
                    }
                  }
                ],
                "refresh_url": "/RotateBoundCookies"
              }
            },
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true,
              "register_session_payload": {
                "session_identifier": "id_2",
                "credentials": [
                  {
                    "type": "cookie",
                    "name": "__Secure-Google-Cookie",
                    "scope": {
                      "domain": ".google.com",
                      "path": "/"
                    }
                  }
                ],
                "refresh_url": "/RotateBoundDifferentCookies"
              }
            }
          ]
        }
      )";
  const OAuthMultiloginResult result = CreateOAuthMultiloginResult(raw_data);

  const BoundSessionKey expected_key_1 = {.site = GURL("https://google.com"),
                                          .session_id = "id_1"};
  EXPECT_CALL(mock_bound_session_cookie_refresh_service(),
              StopCookieRotation(expected_key_1));

  const BoundSessionKey expected_key_2 = {.site = GURL("https://google.com"),
                                          .session_id = "id_2"};
  EXPECT_CALL(mock_bound_session_cookie_refresh_service(),
              StopCookieRotation(expected_key_2));

  delegate().BeforeSetCookies(result);

  histogram_tester.ExpectUniqueSample(
      "Signin.BoundSessionCredentials.OAuthMultilogin.InvalidParams",
      /*sample=*/0,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Signin.BoundSessionCredentials.OAuthMultilogin.BindingKeyMissing",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Signin.BoundSessionCredentials.OAuthMultilogin.RegisteredSessions",
      /*expected_count=*/0);
}

TEST_F(BoundSessionOAuthMultiLoginDelegateImplTest,
       BeforeSetCookiesSkipsSessionsToReuse) {
  Signin(/*wrapped_key=*/{1, 2, 3});

  const std::string raw_data =
      R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name": "__Secure-1PSIDTS",
              "value": "secure-1p-sidts-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            }
          ],
          "token_binding_directed_response": {},
          "device_bound_session_info": [
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true,
              "register_session_payload": {
                "session_identifier": "id",
                "credentials": [
                  {
                    "type": "cookie",
                    "name": "__Secure-1PSIDTS",
                    "scope": {
                      "domain": ".google.com",
                      "path": "/"
                    }
                  }
                ],
                "refresh_url": "/RotateBoundCookies"
              }
            },
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true
            }
          ]
        }
      )";
  const OAuthMultiloginResult result = CreateOAuthMultiloginResult(raw_data);

  const BoundSessionKey expected_key = {.site = GURL("https://google.com"),
                                        .session_id = "id"};
  EXPECT_CALL(mock_bound_session_cookie_refresh_service(),
              StopCookieRotation(expected_key));

  delegate().BeforeSetCookies(result);
}

TEST_F(BoundSessionOAuthMultiLoginDelegateImplTest,
       BeforeSetCookiesSkipsSessionsWithInvalidParams) {
  base::HistogramTester histogram_tester;

  Signin(/*wrapped_key=*/{1, 2, 3});

  // Second session has invalid params because of the invalid credential scope
  // (domain).
  const std::string raw_data =
      R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name": "__Secure-1PSIDTS",
              "value": "secure-1p-sidts-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            },
            {
              "name": "__Secure-Google-Cookie",
              "value": "secure-google-cookie-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            }
          ],
          "token_binding_directed_response": {},
          "device_bound_session_info": [
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true,
              "register_session_payload": {
                "session_identifier": "id_1",
                "credentials": [
                  {
                    "type": "cookie",
                    "name": "__Secure-1PSIDTS",
                    "scope": {
                      "domain": ".google.com",
                      "path": "/"
                    }
                  }
                ],
                "refresh_url": "/RotateBoundCookies"
              }
            },
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true,
              "register_session_payload": {
                "session_identifier": "id_2",
                "credentials": [
                  {
                    "type": "cookie",
                    "name": "__Secure-Google-Cookie",
                    "scope": {
                      "domain": "invalid",
                      "path": "/"
                    }
                  }
                ],
                "refresh_url": "/RotateBoundDifferentCookies"
              }
            }
          ]
        }
      )";
  const OAuthMultiloginResult result = CreateOAuthMultiloginResult(raw_data);

  const BoundSessionKey expected_key = {.site = GURL("https://google.com"),
                                        .session_id = "id_1"};
  EXPECT_CALL(mock_bound_session_cookie_refresh_service(),
              StopCookieRotation(expected_key));

  delegate().BeforeSetCookies(result);

  histogram_tester.ExpectUniqueSample(
      "Signin.BoundSessionCredentials.OAuthMultilogin.InvalidParams",
      /*sample=*/1,
      /*expected_bucket_count=*/1);
}

TEST_F(BoundSessionOAuthMultiLoginDelegateImplTest,
       OnCookiesSetNoSessionsToRegister) {
  base::HistogramTester histogram_tester;

  Signin(/*wrapped_key=*/{1, 2, 3});

  const std::string raw_data =
      R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name": "__Secure-1PSIDTS",
              "value": "secure-1p-sidts-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            }
          ],
          "token_binding_directed_response": {},
          "device_bound_session_info": [
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true
            }
          ]
        }
      )";
  const OAuthMultiloginResult result = CreateOAuthMultiloginResult(raw_data);

  EXPECT_CALL(mock_bound_session_cookie_refresh_service(), StopCookieRotation)
      .Times(0);

  delegate().BeforeSetCookies(result);

  EXPECT_CALL(mock_bound_session_cookie_refresh_service(),
              RegisterNewBoundSession)
      .Times(0);

  delegate().OnCookiesSet();

  histogram_tester.ExpectUniqueSample(
      "Signin.BoundSessionCredentials.OAuthMultilogin.RegisteredSessions",
      /*sample=*/0,
      /*expected_bucket_count=*/1);
}

// Matcher to match `bound_session_credentials::BoundSessionParams` ignoring the
// `creation_time` field.
MATCHER_P(BoundSessionParamsEquals, expected, "") {
  const BoundSessionParams& actual = arg;
  BoundSessionParams actual_copy = actual;
  BoundSessionParams expected_copy = expected;
  actual_copy.clear_creation_time();
  expected_copy.clear_creation_time();
  return testing::ExplainMatchResult(base::test::EqualsProto(expected_copy),
                                     actual_copy, result_listener);
}

TEST_F(BoundSessionOAuthMultiLoginDelegateImplTest,
       OnCookiesSetRegistersSessions) {
  base::HistogramTester histogram_tester;

  Signin(/*wrapped_key=*/{1, 2, 3});

  const std::string raw_data =
      R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name": "__Secure-1PSIDTS",
              "value": "secure-1p-sidts-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            },
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true,
              "register_session_payload": {
                "session_identifier": "id_2",
                "credentials": [
                  {
                    "type": "cookie",
                    "name": "__Secure-Google-Cookie",
                    "scope": {
                      "domain": ".google.com",
                      "path": "/"
                    }
                  }
                ],
                "refresh_url": "/RotateBoundDifferentCookies"
              }
            }
          ],
          "token_binding_directed_response": {},
          "device_bound_session_info": [
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true,
              "register_session_payload": {
                "session_identifier": "id_1",
                "credentials": [
                  {
                    "type": "cookie",
                    "name": "__Secure-1PSIDTS",
                    "scope": {
                      "domain": ".google.com",
                      "path": "/"
                    }
                  }
                ],
                "refresh_url": "/RotateBoundCookies"
              }
            },
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true,
              "register_session_payload": {
                "session_identifier": "id_2",
                "credentials": [
                  {
                    "type": "cookie",
                    "name": "__Secure-Different-Cookie",
                    "scope": {
                      "domain": ".google.com",
                      "path": "/"
                    }
                  }
                ],
                "refresh_url": "/RotateBoundDifferentCookies"
              }
            }
          ]
        }
      )";
  const OAuthMultiloginResult result = CreateOAuthMultiloginResult(raw_data);

  EXPECT_CALL(mock_bound_session_cookie_refresh_service(), StopCookieRotation)
      .Times(2);

  delegate().BeforeSetCookies(result);

  BoundSessionParams expected_params_1;
  expected_params_1.set_site("https://google.com/");
  expected_params_1.set_session_id("id_1");
  expected_params_1.set_wrapped_key("\x01\x02\x03");
  *expected_params_1.add_credentials() = CreateCookieCredential(
      /*name=*/"__Secure-1PSIDTS",
      /*domain=*/".google.com", /*path=*/"/");
  expected_params_1.set_refresh_url(
      "https://accounts.google.com/RotateBoundCookies");
  EXPECT_CALL(
      mock_bound_session_cookie_refresh_service(),
      RegisterNewBoundSession(BoundSessionParamsEquals(expected_params_1)));

  BoundSessionParams expected_params_2;
  expected_params_2.set_site("https://google.com/");
  expected_params_2.set_session_id("id_2");
  expected_params_2.set_wrapped_key("\x01\x02\x03");
  *expected_params_2.add_credentials() = CreateCookieCredential(
      /*name=*/"__Secure-Different-Cookie",
      /*domain=*/".google.com", /*path=*/"/");
  expected_params_2.set_refresh_url(
      "https://accounts.google.com/RotateBoundDifferentCookies");
  EXPECT_CALL(
      mock_bound_session_cookie_refresh_service(),
      RegisterNewBoundSession(BoundSessionParamsEquals(expected_params_2)));
  delegate().OnCookiesSet();

  histogram_tester.ExpectUniqueSample(
      "Signin.BoundSessionCredentials.OAuthMultilogin.RegisteredSessions",
      /*sample=*/2,
      /*expected_bucket_count=*/1);
}

}  // namespace

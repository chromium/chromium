// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zone.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chrome/browser/ash/printing/oauth2/test_authorization_server.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/printing/uri.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::printing::oauth2 {
namespace {

class MockAuthorizationZone : public AuthorizationZone {
 public:
  MOCK_METHOD(void,
              InitAuthorization,
              (const std::string& scope, StatusCallback callback),
              (override));
  MOCK_METHOD(void,
              FinishAuthorization,
              (const GURL& redirect_url, StatusCallback callback),
              (override));
  MOCK_METHOD(void,
              GetEndpointAccessToken,
              (const chromeos::Uri& ipp_endpoint,
               const std::string& scope,
               StatusCallback callback),
              (override));
  MOCK_METHOD(void,
              MarkEndpointAccessTokenAsExpired,
              (const chromeos::Uri& ipp_endpoint,
               const std::string& endpoint_access_token),
              (override));
};

class PrintingOAuth2AuthorizationZonesManagerTest : public testing::Test {
 protected:
  using AuthZoneMock = testing::StrictMock<MockAuthorizationZone>;

  PrintingOAuth2AuthorizationZonesManagerTest() = default;

  // Creates a mock of AuthorizationZone, stores it as a trusted server.
  // Returns a pointer to the mock. The mock is owned by `auth_zones_manager_`.
  AuthZoneMock* CallSaveAuthorizationServerAsTrusted(const GURL& auth_server) {
    auto auth_zone = std::make_unique<AuthZoneMock>();
    auto* auth_zone_ptr = auth_zone.get();
    const StatusCode sc =
        auth_zones_manager_->SaveAuthorizationServerAsTrustedForTesting(
            auth_server, std::move(auth_zone));
    EXPECT_EQ(sc, StatusCode::kOK);
    return auth_zone_ptr;
  }

  // Calls InitAuthorization(...) and waits for the callback.
  CallbackResult CallInitAuthorization(const GURL& auth_server,
                                       const std::string& scope) {
    base::MockOnceCallback<void(StatusCode, const std::string&)> callback;
    CallbackResult cr;
    base::RunLoop loop;
    EXPECT_CALL(callback, Run)
        .WillOnce([&cr, &loop](StatusCode status, const std::string& data) {
          cr.status = status;
          cr.data = data;
          loop.Quit();
        });
    auth_zones_manager_->InitAuthorization(auth_server, scope, callback.Get());
    loop.Run();
    return cr;
  }

  // Calls FinishAuthorization(...) and waits for the callback.
  CallbackResult CallFinishAuthorization(const GURL& auth_server,
                                         const GURL& redirect_url) {
    base::MockOnceCallback<void(StatusCode, const std::string&)> callback;
    CallbackResult cr;
    base::RunLoop loop;
    EXPECT_CALL(callback, Run)
        .WillOnce([&cr, &loop](StatusCode status, const std::string& data) {
          cr.status = status;
          cr.data = data;
          loop.Quit();
        });
    auth_zones_manager_->FinishAuthorization(auth_server, redirect_url,
                                             callback.Get());
    loop.Run();
    return cr;
  }

  // Calls GetEndpointAccessToken(...) and waits for the callback.
  CallbackResult CallGetEndpointAccessToken(const GURL& auth_server,
                                            const chromeos::Uri& ipp_endpoint,
                                            const std::string& scope) {
    base::MockOnceCallback<void(StatusCode, const std::string&)> callback;
    CallbackResult cr;
    base::RunLoop loop;
    EXPECT_CALL(callback, Run)
        .WillOnce([&cr, &loop](StatusCode status, const std::string& data) {
          cr.status = status;
          cr.data = data;
          loop.Quit();
        });
    auth_zones_manager_->GetEndpointAccessToken(auth_server, ipp_endpoint,
                                                scope, callback.Get());
    loop.Run();
    return cr;
  }

  void ExpectCallInitAuthorization(AuthZoneMock* auth_zone,
                                   const std::string& scope,
                                   CallbackResult results_to_report) {
    EXPECT_CALL(*auth_zone, InitAuthorization(scope, testing::_))
        .WillOnce(
            [&results_to_report](const std::string&, StatusCallback callback) {
              std::move(callback).Run(results_to_report.status,
                                      results_to_report.data);
            });
  }

  void ExpectCallFinishAuthorization(AuthZoneMock* auth_zone,
                                     const GURL& redirect_url,
                                     CallbackResult results_to_report) {
    EXPECT_CALL(*auth_zone, FinishAuthorization(redirect_url, testing::_))
        .WillOnce([&results_to_report](const GURL&, StatusCallback callback) {
          std::move(callback).Run(results_to_report.status,
                                  results_to_report.data);
        });
  }

  void ExpectCallGetEndpointAccessToken(AuthZoneMock* auth_zone,
                                        const chromeos::Uri& ipp_endpoint,
                                        const std::string& scope,
                                        CallbackResult results_to_report) {
    EXPECT_CALL(*auth_zone,
                GetEndpointAccessToken(ipp_endpoint, scope, testing::_))
        .WillOnce([&results_to_report](const chromeos::Uri&, const std::string&,
                                       StatusCallback callback) {
          std::move(callback).Run(results_to_report.status,
                                  results_to_report.data);
        });
  }

  void ExpectCallMarkEndpointAccessTokenAsExpired(
      AuthZoneMock* auth_zone,
      const chromeos::Uri& ipp_endpoint,
      const std::string& endpoint_access_token) {
    EXPECT_CALL(*auth_zone, MarkEndpointAccessTokenAsExpired(
                                ipp_endpoint, endpoint_access_token))
        .Times(1);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<AuthorizationZonesManager> auth_zones_manager_ =
      AuthorizationZonesManager::Create(&profile_);
};

TEST_F(PrintingOAuth2AuthorizationZonesManagerTest, UnknownAuthServer) {
  GURL url("https://ala.ma.kota/albo/psa");
  GURL redirect_url("https://abc:123/def?ghi=jkl");
  chromeos::Uri ipp_endpoint("https://printer");

  CallbackResult cr = CallInitAuthorization(url, "scope");
  EXPECT_EQ(cr.status, StatusCode::kUnknownAuthorizationServer);

  cr = CallFinishAuthorization(url, redirect_url);
  EXPECT_EQ(cr.status, StatusCode::kUnknownAuthorizationServer);

  cr = CallGetEndpointAccessToken(url, ipp_endpoint, "scope");
  EXPECT_EQ(cr.status, StatusCode::kUnknownAuthorizationServer);
}

TEST_F(PrintingOAuth2AuthorizationZonesManagerTest, PassingCallsToAuthZones) {
  GURL url_1("https://ala.ma.kota/albo/psa");
  GURL url_2("https://other.server:1234");
  GURL redirect_url("https://abc:123/def?ghi=jkl");
  chromeos::Uri ipp_endpoint("https://printer");

  AuthZoneMock* auth_zone_1 = CallSaveAuthorizationServerAsTrusted(url_1);
  AuthZoneMock* auth_zone_2 = CallSaveAuthorizationServerAsTrusted(url_2);

  ExpectCallInitAuthorization(auth_zone_1, "scope1",
                              {StatusCode::kOK, "auth_url"});
  CallbackResult cr = CallInitAuthorization(url_1, "scope1");
  EXPECT_EQ(cr.status, StatusCode::kOK);
  EXPECT_EQ(cr.data, "auth_url");

  ExpectCallFinishAuthorization(auth_zone_2, redirect_url,
                                {StatusCode::kNoMatchingSession, "abc"});
  cr = CallFinishAuthorization(url_2, redirect_url);
  EXPECT_EQ(cr.status, StatusCode::kNoMatchingSession);
  EXPECT_EQ(cr.data, "abc");

  ExpectCallGetEndpointAccessToken(
      auth_zone_1, ipp_endpoint, "scope1 scope2",
      {StatusCode::kServerTemporarilyUnavailable, "123"});
  cr = CallGetEndpointAccessToken(url_1, ipp_endpoint, "scope1 scope2");
  EXPECT_EQ(cr.status, StatusCode::kServerTemporarilyUnavailable);
  EXPECT_EQ(cr.data, "123");

  ExpectCallMarkEndpointAccessTokenAsExpired(auth_zone_2, ipp_endpoint, "zaq1");
  auth_zones_manager_->MarkEndpointAccessTokenAsExpired(url_2, ipp_endpoint,
                                                        "zaq1");
}

}  // namespace
}  // namespace ash::printing::oauth2

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zone.h"
#include "chrome/browser/ash/printing/oauth2/mock_client_ids_database.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chrome/browser/ash/printing/oauth2/test_authorization_server.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/printing/uri.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::printing::oauth2 {
namespace {

syncer::EntityData ToEntityData(const std::string& uri) {
  syncer::EntityData entity_data;
  entity_data.specifics.mutable_printers_authorization_server()->set_uri(uri);
  entity_data.name = uri;
  return entity_data;
}

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
  MOCK_METHOD(void, MarkAuthorizationZoneAsUntrusted, (), (override));
};

class PrintingOAuth2AuthorizationZonesManagerTest : public testing::Test {
 protected:
  using AuthZoneMock = testing::StrictMock<MockAuthorizationZone>;

  PrintingOAuth2AuthorizationZonesManagerTest() {
    // This method is called when the bridge inside AuthorizationZonesManager
    // is fully initialized.
    EXPECT_CALL(mock_processor_, ModelReadyToSync(testing::_))
        .Times(testing::AtMost(1))
        .WillOnce([this]() { bridge_initialization_.Quit(); });

    auto client_ids_database =
        std::make_unique<testing::NiceMock<MockClientIdsDatabase>>();
    client_ids_database_ = client_ids_database.get();
    auth_zones_manager_ = AuthorizationZonesManager::CreateForTesting(
        &profile_,
        base::BindRepeating(
            &PrintingOAuth2AuthorizationZonesManagerTest::CreateAuthZoneMock,
            base::Unretained(this)),
        std::move(client_ids_database),
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()));
  }

  // Wait for `auth_zones_manager_` to be completely initialized. It is done
  // when the internal sync bridge completes its initialization.
  void WaitForTheCompletionOfInitialization() {
    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
    bridge_initialization_.Run();
  }

  // Creates a mock of AuthorizationZone, stores it as a trusted server.
  // Returns a pointer to the mock. The mock is owned by `auth_zones_manager_`.
  AuthZoneMock* CallSaveAuthorizationServerAsTrusted(const GURL& auth_server) {
    const StatusCode sc =
        auth_zones_manager_->SaveAuthorizationServerAsTrusted(auth_server);
    DCHECK_EQ(sc, StatusCode::kOK);
    return auth_zones_[auth_server];
  }

  // Calls InitAuthorization(...) and waits for the callback.
  CallbackResult CallInitAuthorization(const GURL& auth_server,
                                       const std::string& scope) {
    base::MockOnceCallback<void(StatusCode, std::string)> callback;
    CallbackResult cr;
    base::RunLoop loop;
    EXPECT_CALL(callback, Run)
        .WillOnce([&cr, &loop](StatusCode status, std::string data) {
          cr.status = status;
          cr.data = std::move(data);
          loop.Quit();
        });
    auth_zones_manager_->InitAuthorization(auth_server, scope, callback.Get());
    loop.Run();
    return cr;
  }

  // Calls FinishAuthorization(...) and waits for the callback.
  CallbackResult CallFinishAuthorization(const GURL& auth_server,
                                         const GURL& redirect_url) {
    base::MockOnceCallback<void(StatusCode, std::string)> callback;
    CallbackResult cr;
    base::RunLoop loop;
    EXPECT_CALL(callback, Run)
        .WillOnce([&cr, &loop](StatusCode status, std::string data) {
          cr.status = status;
          cr.data = std::move(data);
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
    base::MockOnceCallback<void(StatusCode, std::string)> callback;
    CallbackResult cr;
    base::RunLoop loop;
    EXPECT_CALL(callback, Run)
        .WillOnce([&cr, &loop](StatusCode status, std::string data) {
          cr.status = status;
          cr.data = std::move(data);
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
            [results_to_report](const std::string&, StatusCallback callback) {
              std::move(callback).Run(results_to_report.status,
                                      std::move(results_to_report.data));
            });
  }

  void ExpectCallFinishAuthorization(AuthZoneMock* auth_zone,
                                     const GURL& redirect_url,
                                     CallbackResult results_to_report) {
    EXPECT_CALL(*auth_zone, FinishAuthorization(redirect_url, testing::_))
        .WillOnce([results_to_report](const GURL&, StatusCallback callback) {
          std::move(callback).Run(results_to_report.status,
                                  std::move(results_to_report.data));
        });
  }

  void ExpectCallGetEndpointAccessToken(AuthZoneMock* auth_zone,
                                        const chromeos::Uri& ipp_endpoint,
                                        const std::string& scope,
                                        CallbackResult results_to_report) {
    EXPECT_CALL(*auth_zone,
                GetEndpointAccessToken(ipp_endpoint, scope, testing::_))
        .WillOnce([results_to_report](const chromeos::Uri&, const std::string&,
                                      StatusCallback callback) {
          std::move(callback).Run(results_to_report.status,
                                  std::move(results_to_report.data));
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

  std::unique_ptr<AuthorizationZone> CreateAuthZoneMock(
      const GURL& url,
      ClientIdsDatabase* client_ids_database) {
    auto auth_zone = std::make_unique<AuthZoneMock>();
    auto [_, created] = auth_zones_.emplace(url, auth_zone.get());
    DCHECK(created);
    return auth_zone;
  }

  raw_ptr<testing::NiceMock<MockClientIdsDatabase>, DanglingUntriaged>
      client_ids_database_;
  std::map<GURL, raw_ptr<AuthZoneMock, CtnExperimental>> auth_zones_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<syncer::DataTypeStore> store_ =
      syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest();
  base::RunLoop bridge_initialization_;
  std::unique_ptr<AuthorizationZonesManager> auth_zones_manager_;
};

TEST_F(PrintingOAuth2AuthorizationZonesManagerTest, UntrustedAuthServer) {
  GURL url("https://ala.ma.kota/albo/psa");
  GURL redirect_url("https://abc:123/def?ghi=jkl");
  chromeos::Uri ipp_endpoint("https://printer");

  CallbackResult cr = CallInitAuthorization(url, "scope");
  EXPECT_EQ(cr.status, StatusCode::kUntrustedAuthorizationServer);

  cr = CallFinishAuthorization(url, redirect_url);
  EXPECT_EQ(cr.status, StatusCode::kUntrustedAuthorizationServer);

  cr = CallGetEndpointAccessToken(url, ipp_endpoint, "scope");
  EXPECT_EQ(cr.status, StatusCode::kUntrustedAuthorizationServer);
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

TEST_F(PrintingOAuth2AuthorizationZonesManagerTest,
       SaveAuthorizationServerAsTrustedBeforeInitialization) {
  GURL url_1("https://ala.ma.kota/albo/psa");
  CallbackResult cr;
  auto callback = [&cr](StatusCode status, std::string data) {
    cr.status = status;
    cr.data = std::move(data);
  };

  AuthZoneMock* auth_zone_1 = CallSaveAuthorizationServerAsTrusted(url_1);
  auth_zones_manager_->InitAuthorization(url_1, "scope",
                                         base::BindLambdaForTesting(callback));

  EXPECT_CALL(mock_processor_, Put(url_1.spec(), testing::_, testing::_));
  ExpectCallInitAuthorization(auth_zone_1, "scope",
                              CallbackResult{StatusCode::kOK, "data"});
  WaitForTheCompletionOfInitialization();
}

TEST_F(PrintingOAuth2AuthorizationZonesManagerTest,
       SaveAuthorizationServerAsTrustedAfterInitialization) {
  GURL url_1("https://ala.ma.kota/albo/psa");
  CallbackResult cr;
  auto callback = [&cr](StatusCode status, std::string data) {
    cr.status = status;
    cr.data = std::move(data);
  };

  WaitForTheCompletionOfInitialization();

  EXPECT_CALL(mock_processor_, Put(url_1.spec(), testing::_, testing::_));
  AuthZoneMock* auth_zone_1 = CallSaveAuthorizationServerAsTrusted(url_1);

  ExpectCallInitAuthorization(auth_zone_1, "scope",
                              CallbackResult{StatusCode::kOK, "data"});
  auth_zones_manager_->InitAuthorization(url_1, "scope",
                                         base::BindLambdaForTesting(callback));
  EXPECT_EQ(cr.status, StatusCode::kOK);
  EXPECT_EQ(cr.data, "data");
}

TEST_F(PrintingOAuth2AuthorizationZonesManagerTest,
       ApplyIncrementalSyncChanges) {
  GURL url_1("https://ala.ma.kota/albo/psa");
  GURL url_2("https://other.server:1234");

  WaitForTheCompletionOfInitialization();

  AuthZoneMock* auth_zone_1 = CallSaveAuthorizationServerAsTrusted(url_1);

  EXPECT_CALL(*auth_zone_1, MarkAuthorizationZoneAsUntrusted());

  syncer::EntityChangeList data_change_list;
  data_change_list.push_back(syncer::EntityChange::CreateAdd(
      url_2.spec(), ToEntityData(url_2.spec())));
  data_change_list.push_back(syncer::EntityChange::CreateDelete(url_1.spec()));
  syncer::DataTypeSyncBridge* bridge =
      auth_zones_manager_->GetDataTypeSyncBridge();

  std::optional<syncer::ModelError> error = bridge->ApplyIncrementalSyncChanges(
      bridge->CreateMetadataChangeList(), std::move(data_change_list));
  EXPECT_FALSE(error);

  // Check if |url_1| is gone.
  CallbackResult cr = CallInitAuthorization(url_1, "scope1");
  EXPECT_EQ(cr.status, StatusCode::kUntrustedAuthorizationServer);

  // Check if |url_2| is added.
  AuthZoneMock* auth_zone_2 = auth_zones_[url_2];
  ASSERT_TRUE(auth_zone_2);
  ExpectCallInitAuthorization(auth_zone_2, "scope1", {StatusCode::kOK, "x"});
  cr = CallInitAuthorization(url_2, "scope1");
  EXPECT_EQ(cr.status, StatusCode::kOK);
}

}  // namespace
}  // namespace ash::printing::oauth2

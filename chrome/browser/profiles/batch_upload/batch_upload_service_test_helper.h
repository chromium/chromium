// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_SERVICE_TEST_HELPER_H_
#define CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_SERVICE_TEST_HELPER_H_

#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "chrome/browser/ui/profiles/batch_upload_ui_delegate.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/test/mock_sync_service.h"

class BatchUploadDelegate;
class Profile;

namespace content {
class BrowserContext;
}  // namespace content

// The purpose of this class is to facilitate testing of the
// `BatchUploadService`. The main service relies on the `syncer::SyncService` to
// retrieve the local data to be used by the service, and given that the real
// SyncService is usually not used in tests, there is a need to always override
// the returned values that are used by the `BatchUploadService`.
// `BatchUploadServiceTestHelper` provides abstraction of that complexity by
// offering methods that constructs the service with the required testing
// dependencies, and easily control the retrieval of local data.
//
// Even though this test helper uses an internal `syncer::MockSyncService` which
// is also used by the constructed service, the tests using it can still use
// their own implementation of SyncService that may be tied to a profile, and
// will not affect this test helper/service, and vice versa.
class BatchUploadServiceTestHelper {
 public:
  BatchUploadServiceTestHelper();
  ~BatchUploadServiceTestHelper();

  // Setups up the TestingFactory to override the service constructed for
  // `profile` with a service that has the proper testing setup.
  // It is recommended for this function to be called in
  // `InProcessBrowserTest::SetUpBrowserContextKeyedServices()` during test
  // setup - if called after the service was already constructed, it will
  // override the existing service, potentially breaking other dependent
  // services. Even though `BatchUploadService` is not constructed at profile
  // initialization, it is still recommended to follow this pattern for
  // consistency.
  //  If `identity_manager` is nullptr, then the IdentityManager from the
  //  `profile` is used.
  // If `delegate` is not set, then the regular `BatchUploadUIDelegate` is used.
  void SetupBatchUploadTestingFactoryInProfile(
      Profile* profile,
      signin::IdentityManager* identity_manager = nullptr,
      std::unique_ptr<BatchUploadDelegate> delegate =
          std::make_unique<BatchUploadUIDelegate>());
  // Constructs a `BatchUploadService` instance that is not tied to any profile.
  std::unique_ptr<BatchUploadService> CreateBatchUploadService(
      signin::IdentityManager* identity_manager,
      std::unique_ptr<BatchUploadDelegate> delegate);

  // The following methods will affect the constructed BatchUploadService
  // through this class with the above methods.
  //
  // Simulates the addition of `item_count` local item(s) of `type`. Those items
  // will be returned when attempting to open the Batch Upload.
  // Returns the added description.
  const syncer::LocalDataDescription& SetReturnDescriptions(
      syncer::DataType type,
      size_t item_count);
  // Simulates the addition of 1 item of each available type. Those items will
  // be returned when attempting to open the Batch Upload.
  void SetLocalDataDescriptionForAllAvailableTypes();
  // Clears all fake local items.
  void ClearReturnDescriptions();
  // Returns the expected return description for `type`.
  syncer::LocalDataDescription& GetReturnDescription(syncer::DataType type);

  syncer::MockSyncService* GetSyncServiceMock() { return &mock_sync_service_; }
  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 private:
  std::unique_ptr<KeyedService> CreateBatchUploadServiceInternal(
      signin::IdentityManager* identity_manager,
      std::unique_ptr<BatchUploadDelegate> delegate,
      content::BrowserContext* browser_context);

  testing::NiceMock<syncer::MockSyncService> mock_sync_service_;
  TestingPrefServiceSimple pref_service_;
  std::map<syncer::DataType, syncer::LocalDataDescription>
      returned_descriptions_;
};

#endif  // CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_SERVICE_TEST_HELPER_H_

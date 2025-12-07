// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/batch_upload/batch_upload_service_test_helper.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/strings/to_string.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_delegate.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/profiles/batch_upload_ui_delegate.h"

namespace {

syncer::LocalDataItemModel MakeDummyLocalDataModel(size_t id) {
  syncer::LocalDataItemModel model;
  std::string id_string = base::ToString(id);
  model.id = id_string;
  model.title = "title_" + id_string;
  model.subtitle = "subtitle" + id_string;
  return model;
}

}  // namespace

BatchUploadServiceTestHelper::BatchUploadServiceTestHelper() {
  // The real call is asynchronous, the mock calls the callback right away which
  // is simpler for testing.
  ON_CALL(*GetSyncServiceMock(), GetLocalDataDescriptions)
      .WillByDefault(
          [this](syncer::DataTypeSet types,
                 base::OnceCallback<void(
                     std::map<syncer::DataType, syncer::LocalDataDescription>)>
                     callback) {
            std::move(callback).Run(returned_descriptions_);
          });
}

BatchUploadServiceTestHelper::~BatchUploadServiceTestHelper() = default;

void BatchUploadServiceTestHelper::SetupBatchUploadTestingFactoryInProfile(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    std::unique_ptr<BatchUploadDelegate> delegate) {
  BatchUploadServiceFactory::GetInstance()->SetTestingFactory(
      profile,
      base::BindOnce(
          &BatchUploadServiceTestHelper::CreateBatchUploadServiceInternal,
          base::Unretained(this), identity_manager, std::move(delegate)));
}

std::unique_ptr<KeyedService>
BatchUploadServiceTestHelper::CreateBatchUploadServiceInternal(
    signin::IdentityManager* identity_manager,
    std::unique_ptr<BatchUploadDelegate> delegate,
    content::BrowserContext* browser_context) {
  if (!identity_manager) {
    identity_manager = IdentityManagerFactory::GetForProfile(
        Profile::FromBrowserContext(browser_context));
  }

  return CreateBatchUploadService(identity_manager, std::move(delegate));
}

// static
std::unique_ptr<BatchUploadService>
BatchUploadServiceTestHelper::CreateBatchUploadService(
    signin::IdentityManager* identity_manager,
    std::unique_ptr<BatchUploadDelegate> delegate) {
  return std::make_unique<BatchUploadService>(
      identity_manager, GetSyncServiceMock(), &pref_service_,
      std::move(delegate));
}

const syncer::LocalDataDescription&
BatchUploadServiceTestHelper::SetReturnDescriptions(syncer::DataType type,
                                                    size_t item_count) {
  syncer::LocalDataDescription& description = returned_descriptions_[type];
  description.type = type;
  description.local_data_models.clear();
  for (size_t i = 0; i < item_count; ++i) {
    description.local_data_models.push_back(MakeDummyLocalDataModel(i));
  }
  return description;
}

void BatchUploadServiceTestHelper::
    SetLocalDataDescriptionForAllAvailableTypes() {
  for (syncer::DataType type : BatchUploadService::AvailableTypesOrder()) {
    SetReturnDescriptions(type, 1);
  }
}

void BatchUploadServiceTestHelper::ClearReturnDescriptions() {
  returned_descriptions_.clear();
}

syncer::LocalDataDescription&
BatchUploadServiceTestHelper::GetReturnDescription(syncer::DataType type) {
  CHECK(returned_descriptions_.contains(type));
  return returned_descriptions_[type];
}

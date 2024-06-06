// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/storage_selector/storage_selector.h"

#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/types/expected.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "components/reporting/compression/compression_module.h"
#include "components/reporting/encryption/encryption_module.h"
#include "components/reporting/storage/storage_configuration.h"
#include "components/reporting/storage/storage_module.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

BASE_FEATURE(kControlledDegradationFeature,
             "ControlledDegradation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// static
bool StorageSelector::is_uploader_required() {
  return true;  // Local storage must have an uploader.
}

// static
bool StorageSelector::is_use_missive() {
  return false;  // Use Local storage.
}

// static
void StorageSelector::CreateLocalStorageModule(
    const base::FilePath& local_reporting_path,
    std::string_view verification_key,
    CompressionInformation::CompressionAlgorithm compression_algorithm,
    UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
    base::OnceCallback<void(StatusOr<scoped_refptr<StorageModuleInterface>>)>
        cb) {
  CHECK(!StorageSelector::is_use_missive()) << "Can only be used in local mode";
  StorageModule::Create(
      StorageOptions()
          .set_directory(local_reporting_path)
          .set_signature_verification_public_key(verification_key),
      base::FeatureList::IsEnabled(kLegacyStorageEnabledFeature)
          ? "SECURITY,IMMEDIATE,FAST_BATCH,SLOW_BATCH,BACKGROUND_BATCH,"
            "MANUAL_BATCH,MANUAL_BATCH_LACROS"
          : "UNDEFINED_PRIORITY",
      QueuesContainer::Create(
          base::FeatureList::IsEnabled(kControlledDegradationFeature)),
      EncryptionModule::Create(),
      CompressionModule::Create(512, compression_algorithm),
      std::move(async_start_upload_cb),
      // Callback wrapper changes result type from `StorageModule` to
      // `StorageModuleInterface`.
      base::BindOnce(
          [](base::OnceCallback<void(
                 StatusOr<scoped_refptr<StorageModuleInterface>>)> cb,
             StatusOr<scoped_refptr<StorageModule>> result) {
            std::move(cb).Run(std::move(result));
          },
          std::move(cb)));
}

// static
ReportSuccessfulUploadCallback
StorageSelector::GetLocalReportSuccessfulUploadCb(
    scoped_refptr<StorageModuleInterface> storage_module) {
  return base::BindRepeating(
      [](scoped_refptr<StorageModuleInterface> storage_module,
         SequenceInformation sequence_information, bool force) {
        static_cast<StorageModule*>(storage_module.get())
            ->ReportSuccess(std::move(sequence_information), force,
                            base::DoNothing());
      },
      storage_module);
}

// static
EncryptionKeyAttachedCallback StorageSelector::GetLocalEncryptionKeyAttachedCb(
    scoped_refptr<StorageModuleInterface> storage_module) {
  return base::BindRepeating(
      [](scoped_refptr<StorageModuleInterface> storage_module,
         SignedEncryptionInfo signed_encryption_key) {
        static_cast<StorageModule*>(storage_module.get())
            ->UpdateEncryptionKey(std::move(signed_encryption_key));
      },
      storage_module);
}
}  // namespace reporting

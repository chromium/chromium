// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_client_test_util.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/policy/messaging_layer/public/report_client.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/storage/test_storage_module.h"

namespace reporting {

// static
std::unique_ptr<ReportingClient::TestEnvironment>
ReportingClient::TestEnvironment::CreateWithLocalStorage(
    const base::FilePath& reporting_path,
    std::string_view verification_key) {
  return base::WrapUnique(new TestEnvironment(base::BindRepeating(
      [](const base::FilePath& reporting_path,
         std::string_view verification_key,
         base::OnceCallback<void(
             StatusOr<scoped_refptr<StorageModuleInterface>>)>
             storage_created_cb) {
        ReportingClient::CreateLocalStorageModule(
            reporting_path, verification_key,
            CompressionInformation::COMPRESSION_SNAPPY,
            base::BindRepeating(&ReportingClient::AsyncStartUploader),
            std::move(storage_created_cb));
      },
      reporting_path, verification_key)));
}

// static
std::unique_ptr<ReportingClient::TestEnvironment>
ReportingClient::TestEnvironment::CreateWithStorageModule(
    scoped_refptr<StorageModuleInterface> storage) {
  return base::WrapUnique(new TestEnvironment(base::BindRepeating(
      [](scoped_refptr<StorageModuleInterface> storage,
         base::OnceCallback<void(
             StatusOr<scoped_refptr<StorageModuleInterface>>)>
             storage_created_cb) {
        std::move(storage_created_cb).Run(storage);
      },
      storage)));
}

ReportingClient::TestEnvironment::TestEnvironment(
    ReportingClient::StorageModuleCreateCallback storage_create_cb)
    : saved_storage_create_cb_(
          std::move(ReportingClient::GetInstance()->storage_create_cb_)) {
  ReportingClient::GetInstance()->storage_create_cb_ = storage_create_cb;
}

ReportingClient::TestEnvironment::~TestEnvironment() {
  ReportingClient::GetInstance()->storage_create_cb_ =
      std::move(saved_storage_create_cb_);
  base::Singleton<ReportingClient>::OnExit(nullptr);
}
}  // namespace reporting

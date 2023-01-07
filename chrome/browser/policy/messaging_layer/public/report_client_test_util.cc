// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_client_test_util.h"

#include "base/memory/singleton.h"

namespace reporting {

ReportingClient::TestEnvironment::TestEnvironment(
    const base::FilePath& reporting_path,
    base::StringPiece verification_key)
    : saved_storage_create_cb_(
          std::move(ReportingClient::GetInstance()->storage_create_cb_)) {
  ReportingClient::GetInstance()->storage_create_cb_ = base::BindRepeating(
      [](const base::FilePath& reporting_path,
         base::StringPiece verification_key,
         base::OnceCallback<void(
             StatusOr<scoped_refptr<StorageModuleInterface>>)>
             storage_created_cb) {
        ReportingClient::CreateLocalStorageModule(
            reporting_path, verification_key,
            CompressionInformation::COMPRESSION_SNAPPY,
            base::BindRepeating(&ReportingClient::AsyncStartUploader),
            std::move(storage_created_cb));
      },
      reporting_path, verification_key);
}

ReportingClient::TestEnvironment::~TestEnvironment() {
  ReportingClient::GetInstance()->storage_create_cb_ =
      std::move(saved_storage_create_cb_);
  base::Singleton<ReportingClient>::OnExit(nullptr);
}

}  // namespace reporting

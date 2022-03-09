// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "report_client_test_util.h"

#include "base/memory/singleton.h"

namespace reporting {

ReportingClient::TestEnvironment::TestEnvironment(
    const base::FilePath& reporting_path,
    base::StringPiece verification_key,
    policy::CloudPolicyClient* client)
    : saved_storage_create_cb_(
          std::move(ReportingClient::GetInstance()->storage_create_cb_)),
      saved_build_cloud_policy_client_cb_(std::move(
          ReportingClient::GetInstance()->build_cloud_policy_client_cb_)) {
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
  ReportingClient::GetInstance()->build_cloud_policy_client_cb_ =
      base::BindRepeating(
          [](policy::CloudPolicyClient* client,
             CloudPolicyClientResultCb build_cb) {
            std::move(build_cb).Run(std::move(client));
          },
          std::move(client));
}

ReportingClient::TestEnvironment::~TestEnvironment() {
  ReportingClient::GetInstance()->storage_create_cb_ =
      std::move(saved_storage_create_cb_);
  ReportingClient::GetInstance()->build_cloud_policy_client_cb_ =
      std::move(saved_build_cloud_policy_client_cb_);
  base::Singleton<ReportingClient>::OnExit(nullptr);
}

}  // namespace reporting

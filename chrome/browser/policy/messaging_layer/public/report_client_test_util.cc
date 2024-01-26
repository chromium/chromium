// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_client_test_util.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/task/bind_post_task.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/messaging_layer/public/report_client.h"
#include "chrome/browser/policy/messaging_layer/storage_selector/storage_selector.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/storage/test_storage_module.h"

namespace reporting {

#if !BUILDFLAG(IS_CHROMEOS)
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
        StorageSelector::CreateLocalStorageModule(
            reporting_path, verification_key,
            CompressionInformation::COMPRESSION_SNAPPY,
            base::BindPostTask(
                ReportQueueProvider::GetInstance()->sequenced_task_runner(),
                base::BindRepeating(
                    &ReportingClient::AsyncStartUploader,
                    ReportQueueProvider::GetInstance()->GetWeakPtr())),
            std::move(storage_created_cb));
      },
      reporting_path, verification_key)));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

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
    // Below we convert ReportingClient::SmartPtr to std::unique_ptr.
    : client_(ReportingClient::Create(
                  base::SequencedTaskRunner::GetCurrentDefault())
                  .release()) {
  client_->storage_create_cb_ = storage_create_cb;
}

ReportingClient::TestEnvironment::~TestEnvironment() = default;
}  // namespace reporting

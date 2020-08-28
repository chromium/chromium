// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/policy/messaging_layer/storage/storage.h"
#include "chrome/browser/policy/messaging_layer/storage/storage_module.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"

namespace reporting {

StorageModule::StorageModule() = default;

StorageModule::~StorageModule() = default;

void StorageModule::AddRecord(EncryptedRecord record,
                              Priority priority,
                              base::OnceCallback<void(Status)> callback) {
  storage_->Write(priority, std::move(record), std::move(callback));
}

void StorageModule::ReportSuccess(
    SequencingInformation sequencing_information) {
  LOG(ERROR) << "ReportSuccess isn't implemented";
}

// static
void StorageModule::Create(
    const Storage::Options& options,
    Storage::StartUploadCb start_upload_cb,
    base::OnceCallback<void(StatusOr<scoped_refptr<StorageModule>>)> callback) {
  scoped_refptr<StorageModule> instance =
      // Cannot base::MakeRefCounted, since constructor is protected.
      base::WrapRefCounted(new StorageModule());
  Storage::Create(
      options, start_upload_cb,
      base::BindOnce(
          [](scoped_refptr<StorageModule> instance,
             base::OnceCallback<void(StatusOr<scoped_refptr<StorageModule>>)>
                 callback,
             StatusOr<scoped_refptr<Storage>> storage) {
            if (!storage.ok()) {
              std::move(callback).Run(storage.status());
              return;
            }
            instance->storage_ = std::move(storage.ValueOrDie());
            std::move(callback).Run(std::move(instance));
          },
          std::move(instance), std::move(callback)));
}

}  // namespace reporting

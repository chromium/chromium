// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/encryption/encryption_module.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/strings/string_piece.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/proto/record.pb.h"

namespace reporting {

namespace {

// Temporary: enable/disable encryption.
const base::Feature kEncryptedReportingFeature{
    EncryptionModule::kEncryptedReporting, base::FEATURE_DISABLED_BY_DEFAULT};

// Helper function for asynchronous encryption.
void AddToRecord(base::StringPiece record,
                 Encryptor::Handle* handle,
                 base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb) {
  handle->AddToRecord(
      record,
      base::BindOnce(
          [](Encryptor::Handle* handle,
             base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb,
             Status status) {
            if (!status.ok()) {
              std::move(cb).Run(status);
              return;
            }
            base::ThreadPool::PostTask(
                FROM_HERE,
                base::BindOnce(&Encryptor::Handle::CloseRecord,
                               base::Unretained(handle), std::move(cb)));
          },
          base::Unretained(handle), std::move(cb)));
}

}  // namespace

const char EncryptionModule::kEncryptedReporting[] = "EncryptedReporting";

EncryptionModule::EncryptionModule() {
  auto encryptor_result = Encryptor::Create();
  DCHECK(encryptor_result.ok());
  encryptor_ = std::move(encryptor_result.ValueOrDie());
}

EncryptionModule::~EncryptionModule() = default;

void EncryptionModule::EncryptRecord(
    base::StringPiece record,
    base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb) const {
  if (!base::FeatureList::IsEnabled(kEncryptedReportingFeature)) {
    // Encryptor disabled.
    EncryptedRecord encrypted_record;
    encrypted_record.mutable_encrypted_wrapped_record()->assign(record.begin(),
                                                                record.end());
    // encryption_info is not set.
    std::move(cb).Run(encrypted_record);
    return;
  }

  // Encryptor enabled: start encryption of the record as a whole.
  encryptor_->OpenRecord(base::BindOnce(
      [](base::StringPiece record,
         base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb,
         StatusOr<Encryptor::Handle*> handle_result) {
        if (!handle_result.ok()) {
          std::move(cb).Run(handle_result.status());
          return;
        }
        base::ThreadPool::PostTask(
            FROM_HERE,
            base::BindOnce(&AddToRecord, std::string(record),
                           base::Unretained(handle_result.ValueOrDie()),
                           std::move(cb)));
      },
      std::string(record), std::move(cb)));
}

void EncryptionModule::UpdateAsymmetricKey(
    base::StringPiece new_key,
    base::OnceCallback<void(Status)> response_cb) {
  encryptor_->UpdateAsymmetricKey(new_key, std::move(response_cb));
}

}  // namespace reporting

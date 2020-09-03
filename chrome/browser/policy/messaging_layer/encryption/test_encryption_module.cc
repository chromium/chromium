// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/encryption/test_encryption_module.h"

#include "base/callback.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/proto/record.pb.h"

using ::testing::Invoke;

namespace reporting {
namespace test {

TestEncryptionModuleStrict::TestEncryptionModuleStrict() {
  ON_CALL(*this, EncryptRecord)
      .WillByDefault(
          Invoke([](base::StringPiece record,
                    base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb) {
            EncryptedRecord encrypted_record;
            encrypted_record.set_encrypted_wrapped_record(std::string(record));
            // encryption_info is not set.
            std::move(cb).Run(encrypted_record);
          }));
}

void TestEncryptionModuleStrict::UpdateAsymmetricKey(
    base::StringPiece new_key,
    base::OnceCallback<void(Status)> response_cb) {
  std::move(response_cb)
      .Run(Status(error::UNIMPLEMENTED,
                  "Test Encryption Module does not accept any keys"));
}

TestEncryptionModuleStrict::~TestEncryptionModuleStrict() = default;

}  // namespace test
}  // namespace reporting

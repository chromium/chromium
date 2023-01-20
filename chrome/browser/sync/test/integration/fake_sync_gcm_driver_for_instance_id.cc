// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/fake_sync_gcm_driver_for_instance_id.h"

#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "components/gcm_driver/crypto/gcm_encryption_result.h"

FakeSyncGCMDriver::FakeSyncGCMDriver(
    Profile* profile,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner)
    : instance_id::FakeGCMDriverForInstanceID(
          profile->GetPath().Append(FILE_PATH_LITERAL("gcm_test_store")),
          blocking_task_runner) {}

void FakeSyncGCMDriver::EncryptMessage(const std::string& app_id,
                                       const std::string& authorized_entity,
                                       const std::string& p256dh,
                                       const std::string& auth_secret,
                                       const std::string& message,
                                       EncryptMessageCallback callback) {
  // Pretend that message has been encrypted. Some tests rely on unencrypted
  // content to check results.
  std::move(callback).Run(gcm::GCMEncryptionResult::ENCRYPTED_DRAFT_08,
                          message);
}

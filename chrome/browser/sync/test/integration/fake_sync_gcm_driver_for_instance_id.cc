// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/fake_sync_gcm_driver_for_instance_id.h"

#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "components/gcm_driver/crypto/gcm_encryption_result.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/keyed_service/core/keyed_service.h"

// static
std::unique_ptr<KeyedService> FakeSyncGCMDriver::Build(
    content::BrowserContext* context) {
  auto service = std::make_unique<gcm::FakeGCMProfileService>();
  Profile* profile = Profile::FromBrowserContext(context);

  // Allow blocking to initialize GCM client from the disk.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
  service->SetDriverForTesting(
      std::make_unique<FakeSyncGCMDriver>(profile, blocking_task_runner));
  return service;
}

FakeSyncGCMDriver::FakeSyncGCMDriver(
    Profile* profile,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner)
    : instance_id::FakeGCMDriverForInstanceID(
          profile->GetPath().Append(FILE_PATH_LITERAL("gcm_test_store")),
          blocking_task_runner),
      profile_(profile) {}

std::string FakeSyncGCMDriver::GenerateTokenImpl(
    const std::string& app_id,
    const std::string& authorized_entity,
    const std::string& scope) {
  // TODO(crbug.com/1331206): Implement deleting token and generating a new one
  // for the same profile.
  return app_id + "_" + authorized_entity + "_" + scope + "_" +
         profile_->GetDebugName();
}

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

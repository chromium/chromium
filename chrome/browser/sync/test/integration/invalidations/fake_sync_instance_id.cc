// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/invalidations/fake_sync_instance_id.h"

#include "base/strings/stringprintf.h"
#include "components/gcm_driver/instance_id/instance_id.h"

FakeSyncInstanceID::FakeSyncInstanceID(const std::string& app_id,
                                       gcm::GCMDriver* gcm_driver)
    : instance_id::InstanceID(app_id, gcm_driver),
      token_(GenerateNextToken()) {}

void FakeSyncInstanceID::GetToken(const std::string& authorized_entity,
                                  const std::string& scope,
                                  base::TimeDelta time_to_live,
                                  std::set<Flags> flags,
                                  GetTokenCallback callback) {
  std::move(callback).Run(token_, instance_id::InstanceID::Result::SUCCESS);
}

// Deleting the InstanceID also clears any associated token.
void FakeSyncInstanceID::DeleteIDImpl(DeleteIDCallback callback) {
  token_ = GenerateNextToken();
  std::move(callback).Run(instance_id::InstanceID::Result::SUCCESS);
}

std::string FakeSyncInstanceID::GenerateNextToken() {
  static int next_token_id_ = 1;
  return base::StringPrintf("token %d", next_token_id_++);
}

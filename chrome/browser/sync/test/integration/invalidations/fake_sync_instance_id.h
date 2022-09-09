// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_INVALIDATIONS_FAKE_SYNC_INSTANCE_ID_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_INVALIDATIONS_FAKE_SYNC_INSTANCE_ID_H_

#include <set>
#include <string>

#include "base/time/time.h"
#include "components/gcm_driver/instance_id/instance_id.h"

namespace gcm {
class GCMDriver;
}  // namespace gcm

class FakeSyncInstanceID : public instance_id::InstanceID {
 public:
  FakeSyncInstanceID(const std::string& app_id, gcm::GCMDriver* gcm_driver);

  FakeSyncInstanceID(const FakeSyncInstanceID&) = delete;
  FakeSyncInstanceID& operator=(const FakeSyncInstanceID&) = delete;

  ~FakeSyncInstanceID() override = default;

  void GetID(GetIDCallback callback) override {}

  void GetCreationTime(GetCreationTimeCallback callback) override {}

  void GetToken(const std::string& authorized_entity,
                const std::string& scope,
                base::TimeDelta time_to_live,
                std::set<Flags> flags,
                GetTokenCallback callback) override;

  void ValidateToken(const std::string& authorized_entity,
                     const std::string& scope,
                     const std::string& token,
                     ValidateTokenCallback callback) override {}

  void DeleteToken(const std::string& authorized_entity,
                   const std::string& scope,
                   DeleteTokenCallback callback) override {}

 protected:
  void DeleteTokenImpl(const std::string& authorized_entity,
                       const std::string& scope,
                       DeleteTokenCallback callback) override {}

  void DeleteIDImpl(DeleteIDCallback callback) override;

 private:
  static std::string GenerateNextToken();

  std::string token_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_INVALIDATIONS_FAKE_SYNC_INSTANCE_ID_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/presence/credential_storage/nearby_presence_credential_storage.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::nearby::presence {

class NearbyPresenceCredentialStorageTest : public testing::Test {
 public:
  NearbyPresenceCredentialStorageTest() = default;

  ~NearbyPresenceCredentialStorageTest() override = default;

  // testing::Test:
  void SetUp() override {
    credential_storage_ = std::make_unique<NearbyPresenceCredentialStorage>();
  }

 protected:
  std::unique_ptr<NearbyPresenceCredentialStorage> credential_storage_;
};

TEST_F(NearbyPresenceCredentialStorageTest, Initialize) {
  EXPECT_TRUE(credential_storage_);
}

}  // namespace ash::nearby::presence

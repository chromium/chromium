// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cryptauth/client_app_metadata_provider_service.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// Note: Actual service code does not currently have a test, since it would be a
// "change detector test" (i.e., it would simply verify that the values provided
// are present in the ClientAppMetadata).

TEST(ClientAppMetadataProviderServiceTest, VersionCodeToInt64) {
  EXPECT_EQ(7403690001L,
            ClientAppMetadataProviderService::ConvertVersionCodeToInt64(
                "74.0.3690.1"));
  EXPECT_EQ(10001234567L,
            ClientAppMetadataProviderService::ConvertVersionCodeToInt64(
                "100.0.1234.567"));
  EXPECT_EQ(0L, ClientAppMetadataProviderService::ConvertVersionCodeToInt64(
                    "NotAVersionStringAtAll"));
}

}  // namespace ash

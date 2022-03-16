// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pki_metadata_component_installer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

#include "base/logging.h"

namespace component_updater {

class PKIMetadataComponentInstallerTest : public ::testing::Test {
 public:
  PKIMetadataComponentInstallerTest() = default;
};

TEST_F(PKIMetadataComponentInstallerTest, TestProtoBytesConversion) {
  std::vector<std::vector<uint8_t>> test_bytes = {
      {0xec, 0x72, 0x29, 0x69, 0xcb, 0x64, 0x20, 0x0a, 0xb6, 0x63, 0x8f,
       0x68, 0xac, 0x53, 0x8e, 0x40, 0xab, 0xab, 0x5b, 0x19, 0xa6, 0x48,
       0x56, 0x61, 0x04, 0x2a, 0x10, 0x61, 0xc4, 0x61, 0x27, 0x76}};

  std::string bytes_as_string(&test_bytes[0][0], &test_bytes[0][0] + 32);
  std::vector<std::string> repeated_bytes = {bytes_as_string};

  EXPECT_EQ(PKIMetadataComponentInstallerPolicy::BytesArrayFromProtoBytes(
                google::protobuf::RepeatedPtrField<std::string>(
                    repeated_bytes.begin(), repeated_bytes.end())),
            test_bytes);
}

}  // namespace component_updater

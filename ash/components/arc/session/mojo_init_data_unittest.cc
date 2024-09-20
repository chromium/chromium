// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/mojo_init_data.h"

#include <string.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/mojom/arc_bridge.mojom.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

TEST(MojoInitDataTest, AsIovecVectorWithExchangeVersionFeatureDisabled) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndDisableFeature(kArcExchangeVersionOnMojoHandshake);

  MojoInitData mojo_init_data;
  std::vector<iovec> iov = mojo_init_data.AsIOvecVector();

  // The length of vector should be 3.
  ASSERT_EQ(iov.size(), 3u);

  // Check `protocol_version`.
  EXPECT_EQ(iov[0].iov_len, sizeof(uint8_t));
  const uint8_t protocol_version =
      *static_cast<const uint8_t*>(iov[0].iov_base);
  EXPECT_EQ(protocol_version, 0u);

  // Check `kTokenLength`.
  EXPECT_EQ(iov[1].iov_len, sizeof(uint8_t));
  const uint8_t token_length = *static_cast<const uint8_t*>(iov[1].iov_base);
  EXPECT_EQ(token_length, 32u);

  // Check `token_`.
  EXPECT_EQ(iov[2].iov_len, token_length);
}

TEST(MojoInitDataTest, AsIovecVectorWithExchangeVersionFeatureEnabled) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(kArcExchangeVersionOnMojoHandshake);

  MojoInitData mojo_init_data;
  std::vector<iovec> iov = mojo_init_data.AsIOvecVector();

  // The length of vector should be 5.
  ASSERT_EQ(iov.size(), 5u);

  // Check `protocol_version`.
  EXPECT_EQ(iov[0].iov_len, sizeof(uint8_t));
  const uint8_t protocol_version =
      *static_cast<const uint8_t*>(iov[0].iov_base);
  EXPECT_EQ(protocol_version, 1u);

  // Check `kTokenLength`.
  EXPECT_EQ(iov[1].iov_len, sizeof(uint8_t));
  const uint8_t token_length = *static_cast<const uint8_t*>(iov[1].iov_base);
  EXPECT_EQ(token_length, 32u);

  // Check `token_`.
  EXPECT_EQ(iov[2].iov_len, token_length);

  // Check `kNumInterfaces`.
  EXPECT_EQ(iov[3].iov_len, sizeof(uint32_t));
  const uint32_t num_interfaces =
      *static_cast<const uint32_t*>(iov[3].iov_base);

  // Check the size of the array containing uuid and versions.
  EXPECT_EQ(iov[4].iov_len,
            sizeof(MojoInitData::InterfaceVersion) * num_interfaces);

  // SAFETY: There is no option to construct an array from raw pointer + size.
  // Allocation of `iov[4].iov_len` bytes (= `num_interfaces` elements) is
  // guaranteed.
  const auto interface_versions = UNSAFE_BUFFERS(base::span(
      static_cast<const MojoInitData::InterfaceVersion*>(iov[4].iov_base),
      num_interfaces));

  std::optional<uint32_t> arc_bridge_host_version = std::nullopt;
  for (uint32_t index = 0; index < num_interfaces; index++) {
    // Check if the uuids are sorted.
    if (index > 0) {
      EXPECT_LT(interface_versions[index - 1].uuid,
                interface_versions[index].uuid);
    }
    if (interface_versions[index].uuid == mojom::ArcBridgeHost::Uuid_) {
      arc_bridge_host_version = interface_versions[index].version;
    }
  }
  // Check if `num_interfaces` includes the uuid and version of `ArcBridgeHost`.
  EXPECT_EQ(arc_bridge_host_version, mojom::ArcBridgeHost::Version_);
}

}  // namespace
}  // namespace arc

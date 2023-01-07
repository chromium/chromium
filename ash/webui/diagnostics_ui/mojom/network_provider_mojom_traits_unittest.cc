// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/mojom/network_provider_mojom_traits.h"

#include <map>

#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom-shared.h"
#include "base/containers/fixed_flat_map.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-shared.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {
namespace {
template <typename MojoEnum, typename SourceEnum, size_t N>
void TestToMojom(const base::fixed_flat_map<MojoEnum, SourceEnum, N>& enums) {
  // The mojo enum is not sparse.
  EXPECT_EQ(enums.size() - 1, static_cast<size_t>(MojoEnum::kMaxValue));

  for (auto enum_pair : enums) {
    EXPECT_EQ(
        enum_pair.first,
        (mojo::EnumTraits<MojoEnum, SourceEnum>::ToMojom(enum_pair.second)))
        << "enum " << enum_pair.first << " != " << enum_pair.second;
  }
}

template <typename MojoEnum, typename SourceEnum, size_t N>
void TestFromMojom(const base::fixed_flat_map<MojoEnum, SourceEnum, N>& enums) {
  // The mojo enum is not sparse.
  EXPECT_EQ(enums.size() - 1, static_cast<uint32_t>(MojoEnum::kMaxValue));

  for (auto enum_pair : enums) {
    SourceEnum mojo_to_source;
    EXPECT_TRUE((mojo::EnumTraits<MojoEnum, SourceEnum>::FromMojom(
        enum_pair.first, &mojo_to_source)));
    EXPECT_EQ(mojo_to_source, enum_pair.second)
        << "enum " << enum_pair.first << " != " << enum_pair.second;
  }
}
}  // namespace
namespace network_config_mojom = ::chromeos::network_config::mojom;

class DiagnosticsNetworkProviderMojomTraitsTest : public testing::Test {
 public:
  DiagnosticsNetworkProviderMojomTraitsTest() = default;
  ~DiagnosticsNetworkProviderMojomTraitsTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(DiagnosticsNetworkProviderMojomTraitsTest, SecurityTypeStateMatches) {
  constexpr auto enums = base::MakeFixedFlatMap<
      mojom::SecurityType, network_config_mojom::SecurityType>({
      {mojom::SecurityType::kNone, network_config_mojom::SecurityType::kNone},
      {mojom::SecurityType::kWep8021x,
       network_config_mojom::SecurityType::kWep8021x},
      {mojom::SecurityType::kWepPsk,
       network_config_mojom::SecurityType::kWepPsk},
      {mojom::SecurityType::kWpaEap,
       network_config_mojom::SecurityType::kWpaEap},
      {mojom::SecurityType::kWpaPsk,
       network_config_mojom::SecurityType::kWpaPsk},
  });

  // Ensure mapping between types behaves as expected.
  TestToMojom(enums);
  TestFromMojom(enums);
}

TEST_F(DiagnosticsNetworkProviderMojomTraitsTest,
       AuthenticationTypeStateMatches) {
  constexpr auto enums =
      base::MakeFixedFlatMap<mojom::AuthenticationType,
                             network_config_mojom::AuthenticationType>({
          {mojom::AuthenticationType::kNone,
           network_config_mojom::AuthenticationType::kNone},
          {mojom::AuthenticationType::k8021x,
           network_config_mojom::AuthenticationType::k8021x},
      });

  // Ensure mapping between types behaves as expected.
  TestToMojom(enums);
  TestFromMojom(enums);
}
}  // namespace diagnostics
}  // namespace ash

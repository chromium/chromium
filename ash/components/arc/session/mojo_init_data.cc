// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/mojo_init_data.h"

#include <fcntl.h>

#include <array>
#include <tuple>
#include <type_traits>
#include <vector>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/mojom/arc_bridge.mojom.h"
#include "ash/components/arc/mojom/power.mojom.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"

namespace arc {

// Comparison of `MojoInitData::InterfaceVersion` is needed to sort
// `kInterfaceVersions`.
constexpr bool MojoInitData::InterfaceVersion::operator<(
    const MojoInitData::InterfaceVersion& other) const {
  return std::tie(uuid, version) < std::tie(other.uuid, other.version);
}
static_assert(std::is_standard_layout_v<MojoInitData::InterfaceVersion>,
              "MojoInitData::InterfaceVersion must be in standard layout to "
              "correctly convert it to bytes.");

namespace {

// A randomly-generated 32-byte string along with its length are sent at the
// beginning of the connection. ARC uses the length as a protocol version
// identifier.
constexpr uint8_t kTokenLength = 32;
std::string GenerateRandomToken() {
  uint8_t random_bytes[kTokenLength / 2];
  base::RandBytes(random_bytes);
  return base::HexEncode(random_bytes);
}

// When `protocol_version_` = 1, the version of each interface is sent.
// These elements are not used for `protocol_version_` = 0.

// Initialize `kInterfaceVersions` as a constant, sorted array.
static constexpr auto kInterfaceVersions = []() {
  auto data = std::to_array<MojoInitData::InterfaceVersion>(
      {{mojom::ArcBridgeHost::Uuid_, mojom::ArcBridgeHost::Version_},
       {mojom::PowerHost::Uuid_, mojom::PowerHost::Version_}});
  std::sort(data.begin(), data.end());
  return data;
}();
static constexpr uint32_t kNumInterfaces = kInterfaceVersions.size();

}  // namespace

MojoInitData::MojoInitData()
    : protocol_version_(
          base::FeatureList::IsEnabled(kArcExchangeVersionOnMojoHandshake) ? 1
                                                                           : 0),
      token_(GenerateRandomToken()) {}

MojoInitData::~MojoInitData() = default;

// Returns a vector containing the pointer to each data.
// Do NOT use the returned value after `MojoInitData` object is destructed since
// all variables will be released along with it.
std::vector<iovec> MojoInitData::AsIOvecVector() {
  switch (protocol_version_) {
    case 0:
      return std::vector<iovec>{
          {const_cast<uint8_t*>(&protocol_version_), sizeof(protocol_version_)},
          {const_cast<uint8_t*>(&kTokenLength), sizeof(kTokenLength)},
          {const_cast<char*>(token_.data()), token_.size()}};
    case 1:
      return std::vector<iovec>{
          {const_cast<uint8_t*>(&protocol_version_), sizeof(protocol_version_)},
          {const_cast<uint8_t*>(&kTokenLength), sizeof(kTokenLength)},
          {const_cast<char*>(token_.data()), token_.size()},
          // Add uuids and versions of interfaces.
          {const_cast<uint32_t*>(&kNumInterfaces), sizeof(kNumInterfaces)},
          {const_cast<MojoInitData::InterfaceVersion*>(
               kInterfaceVersions.data()),
           sizeof(MojoInitData::InterfaceVersion) * kNumInterfaces}};
    default:
      NOTREACHED();
  }
}

}  // namespace arc

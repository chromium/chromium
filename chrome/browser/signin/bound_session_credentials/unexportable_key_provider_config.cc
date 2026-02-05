// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/unexportable_key_provider_config.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"
#include "chrome/common/chrome_version.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "crypto/hash.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace unexportable_keys {
namespace {

#if BUILDFLAG(IS_MAC)
// Returns the first 64 bits of the SHA-256 hash of the given `data` as a
// lowercase hex string.
std::string HexEncodeLowerSha64(base::span<const uint8_t> data) {
  return base::HexEncodeLower(
      base::as_byte_span(crypto::hash::Sha256(data)).first<8>());
}

std::string HexEncodeLowerSha64(std::string_view data) {
  return HexEncodeLowerSha64(base::as_byte_span(data));
}

// Checks the absence of prefixes in the given `strs` vector. That is, returns
// if there is no `i` and `j` (different from `i`) such that `strs[i]` is a
// prefix of `strs[j]`.
consteval bool IsPrefixFree(std::vector<std::string_view> strs) {
  std::ranges::sort(strs);
  return std::ranges::adjacent_find(
             strs, [](std::string_view lhs, std::string_view rhs) {
               return lhs.starts_with(rhs);
             }) == strs.end();
}

// Returns the string representation of the given `purpose`. The strings are
// used on macOS to group related keys in the Keychain so they can be
// queried and deleted together.
//
// The strings must not be prefixes of each other.
std::string_view PurposeToString(KeyPurpose purpose) {
  using enum KeyPurpose;

  static constexpr auto kPurposeMap =
      base::MakeFixedFlatMap<KeyPurpose, std::string_view>({
          {kRefreshTokenBinding, "lst"},
          {kDeviceBoundSessionCredentials, "dbsc-standard"},
          {kDeviceBoundSessionCredentialsPrototype, "dbsc-prototype"},
      });

  static_assert(IsPrefixFree(base::ToVector(
                    kPurposeMap, [](const auto& pair) { return pair.second; })),
                "Purpose strings must not be prefixes of each other.");

  return kPurposeMap.at(purpose);
}

#endif  // BUILDFLAG(IS_MAC)

}  // namespace

crypto::UnexportableKeyProvider::Config GetDefaultConfig() {
  return {
#if BUILDFLAG(IS_MAC)
      .keychain_access_group = MAC_TEAM_IDENTIFIER_STRING
      "." MAC_BUNDLE_IDENTIFIER_STRING ".unexportable-keys",
#endif  // BUILDFLAG(IS_MAC)
  };
}

crypto::UnexportableKeyProvider::Config GetConfigForUserDataDir(
    const base::FilePath& user_data_dir) {
  crypto::UnexportableKeyProvider::Config config = GetDefaultConfig();
#if BUILDFLAG(IS_MAC)
  config.application_tag = base::StrCat({
      config.keychain_access_group,
      ".",
      HexEncodeLowerSha64(user_data_dir.value()),
  });
#endif  // BUILDFLAG(IS_MAC)
  return config;
}

crypto::UnexportableKeyProvider::Config GetConfigForProfilePath(
    const base::FilePath& profile_path) {
  crypto::UnexportableKeyProvider::Config config =
      GetConfigForUserDataDir(profile_path.DirName());
#if BUILDFLAG(IS_MAC)
  base::StrAppend(&config.application_tag, {
                                               ".",
                                               profile_path.BaseName().value(),
                                           });
#endif  // BUILDFLAG(IS_MAC)
  return config;
}

crypto::UnexportableKeyProvider::Config GetConfigForProfile(
    const Profile& profile) {
  crypto::UnexportableKeyProvider::Config config =
      GetConfigForProfilePath(profile.GetPath());
#if BUILDFLAG(IS_MAC)
  // For original profiles the creation time is stored on disk and could be
  // modified outside of Chrome, so we use a sentinel value for more robustness.
  const base::Time time = profile.IsOffTheRecord() ? profile.GetCreationTime()
                                                   : base::Time::UnixEpoch();

  base::StrAppend(&config.application_tag,
                  {
                      ".",
                      HexEncodeLowerSha64(base::byte_span_from_ref(
                          time.InMillisecondsSinceUnixEpoch())),
                  });
#endif  // BUILDFLAG(IS_MAC)
  return config;
}

crypto::UnexportableKeyProvider::Config
GetConfigForStoragePartitionPathAndPurpose(
    const Profile& profile,
    const base::FilePath& relative_partition_path,
    KeyPurpose purpose) {
  CHECK(!relative_partition_path.IsAbsolute());

  crypto::UnexportableKeyProvider::Config config = GetConfigForProfile(profile);
#if BUILDFLAG(IS_MAC)
  base::StrAppend(&config.application_tag,
                  {
                      ".",
                      HexEncodeLowerSha64(relative_partition_path.value()),
                      ".",
                      PurposeToString(purpose),
                  });
#endif  // BUILDFLAG(IS_MAC)
  return config;
}

std::string GetApplicationTag(crypto::UnexportableKeyProvider::Config config) {
#if BUILDFLAG(IS_MAC)
  return std::move(config.application_tag);
#else
  return std::string();
#endif  // BUILDFLAG(IS_MAC)
}

size_t FilterUnexportableKeysByActiveApplicationTags(
    std::vector<UnexportableKeyId>& key_ids,
    UnexportableKeyService& key_service,
    const base::flat_set<std::string>& active_application_tag_prefixes) {
  return std::erase_if(key_ids, [&](UnexportableKeyId key_id) -> bool {
    ASSIGN_OR_RETURN(std::string key_tag, key_service.GetKeyTag(key_id),
                     [](auto) { return true; });
    // Since `active_application_tag_prefixes` is sorted, a possible prefix of
    // `key_tag` must come right before `key_tag` if it was in the set.
    auto it = active_application_tag_prefixes.upper_bound(key_tag);
    return it != active_application_tag_prefixes.begin() &&
           key_tag.starts_with(*std::prev(it));
  });
}

}  // namespace unexportable_keys

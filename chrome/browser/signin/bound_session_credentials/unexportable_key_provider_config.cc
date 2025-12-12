// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/unexportable_key_provider_config.h"

#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"
#include "chrome/common/chrome_version.h"
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

std::string_view PurposeToString(KeyPurpose purpose) {
  switch (purpose) {
    case KeyPurpose::kRefreshTokenBinding:
      return "lst";
    case KeyPurpose::kDeviceBoundSessionCredentials:
      return "dbsc";
    case KeyPurpose::kDeviceBoundSessionCredentialsPrototype:
      return "dbsc-prototype";
  }

  NOTREACHED();
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

crypto::UnexportableKeyProvider::Config GetConfigForProfileAndPurpose(
    const Profile& profile,
    KeyPurpose purpose) {
  crypto::UnexportableKeyProvider::Config config = GetConfigForProfile(profile);
#if BUILDFLAG(IS_MAC)
  base::StrAppend(&config.application_tag, {
                                               ".",
                                               PurposeToString(purpose),
                                           });
#endif  // BUILDFLAG(IS_MAC)
  return config;
}

}  // namespace unexportable_keys

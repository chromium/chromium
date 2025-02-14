// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/client_certificates/cert_utils.h"

#include <utility>

#include "chrome/common/chrome_version.h"
#include "components/enterprise/client_certificates/core/ec_private_key_factory.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "components/enterprise/client_certificates/core/unexportable_private_key_factory.h"

namespace client_certificates {

namespace {

#if BUILDFLAG(IS_MAC)
constexpr char kUnexportableKeyKeychainAccessGroup[] =
    MAC_TEAM_IDENTIFIER_STRING "." MAC_BUNDLE_IDENTIFIER_STRING ".devicetrust";
#endif  // BUILDFLAG(IS_MAC)

}  // namespace

std::unique_ptr<PrivateKeyFactory> CreatePrivateKeyFactory() {
  PrivateKeyFactory::PrivateKeyFactoriesMap sub_factories;
  crypto::UnexportableKeyProvider::Config config;
#if BUILDFLAG(IS_MAC)
  config.keychain_access_group = kUnexportableKeyKeychainAccessGroup;
#endif  // BUILDFLAG(IS_MAC)
  auto unexportable_key_factory =
      UnexportablePrivateKeyFactory::TryCreate(std::move(config));
  if (unexportable_key_factory) {
    sub_factories.insert_or_assign(PrivateKeySource::kUnexportableKey,
                                   std::move(unexportable_key_factory));
  }
  sub_factories.insert_or_assign(PrivateKeySource::kSoftwareKey,
                                 std::make_unique<ECPrivateKeyFactory>());

  return PrivateKeyFactory::Create(std::move(sub_factories));
}

}  // namespace client_certificates

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/profile_network_context_service_test_utils.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

enum class AmbientAuthProfileBit {
  INCOGNITO = 1 << 0,
  GUEST = 1 << 1,
};

bool AmbientAuthenticationTestHelper::IsIncognitoAllowedInPolicy(
    int policy_value) {
  return policy_value & static_cast<int>(AmbientAuthProfileBit::INCOGNITO);
}

bool AmbientAuthenticationTestHelper::IsGuestAllowedInPolicy(int policy_value) {
  return policy_value & static_cast<int>(AmbientAuthProfileBit::GUEST);
}

bool AmbientAuthenticationTestHelper::IsAmbientAuthAllowedForProfile(
    Profile* profile) {
  ProfileNetworkContextService* profile_network_context_service =
      ProfileNetworkContextServiceFactory::GetForContext(profile);
  base::FilePath empty_relative_partition_path;
  network::mojom::NetworkContextParams network_context_params;
  cert_verifier::mojom::CertVerifierCreationParams
      cert_verifier_creation_params;
  profile_network_context_service->ConfigureNetworkContextParams(
      /*in_memory=*/false, empty_relative_partition_path,
      &network_context_params, &cert_verifier_creation_params);
  return network_context_params.http_auth_static_network_context_params
             ->allow_default_credentials ==
         net::HttpAuthPreferences::ALLOW_DEFAULT_CREDENTIALS;
}

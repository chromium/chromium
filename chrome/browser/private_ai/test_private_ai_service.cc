// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_ai/test_private_ai_service.h"

#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace private_ai {

TestBlindSignAuthFactory::TestBlindSignAuthFactory() = default;
TestBlindSignAuthFactory::~TestBlindSignAuthFactory() = default;

std::unique_ptr<quiche::BlindSignAuthInterface>
TestBlindSignAuthFactory::CreateBlindSignAuth(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory) {
  auto bsa = std::make_unique<phosphor::MockBlindSignAuth>();
  bsa_ = bsa.get();
  return bsa;
}

TestPrivateAiService::TestPrivateAiService(
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    Profile* profile,
    TestBlindSignAuthFactory* test_bsa_factory,
    std::unique_ptr<phosphor::BlindSignAuthFactory> bsa_factory)
    : PrivateAiService(identity_manager,
                       pref_service,
                       profile,
                       std::move(bsa_factory)),
      test_bsa_factory_(test_bsa_factory) {}

}  // namespace private_ai

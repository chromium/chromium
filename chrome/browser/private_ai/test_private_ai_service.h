// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVATE_AI_TEST_PRIVATE_AI_SERVICE_H_
#define CHROME_BROWSER_PRIVATE_AI_TEST_PRIVATE_AI_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/private_ai/private_ai_service.h"
#include "components/private_ai/phosphor/blind_sign_auth_factory.h"
#include "components/private_ai/phosphor/mock_blind_sign_auth.h"

class PrefService;
class Profile;

namespace network {
class PendingSharedURLLoaderFactory;
}

namespace quiche {
class BlindSignAuthInterface;
}

namespace signin {
class IdentityManager;
}

namespace private_ai {

class TestBlindSignAuthFactory : public phosphor::BlindSignAuthFactory {
 public:
  TestBlindSignAuthFactory();
  ~TestBlindSignAuthFactory() override;

  std::unique_ptr<quiche::BlindSignAuthInterface> CreateBlindSignAuth(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory) override;

  phosphor::MockBlindSignAuth* mock_bsa() { return bsa_; }

 private:
  raw_ptr<phosphor::MockBlindSignAuth> bsa_ = nullptr;
};

class TestPrivateAiService : public PrivateAiService {
 public:
  TestPrivateAiService(
      signin::IdentityManager* identity_manager,
      PrefService* pref_service,
      Profile* profile,
      // This factory is owned by `PrivateAiService`, so we need to keep a
      // raw pointer to it.
      TestBlindSignAuthFactory* test_bsa_factory,
      std::unique_ptr<phosphor::BlindSignAuthFactory> bsa_factory);

  ~TestPrivateAiService() override = default;

  phosphor::MockBlindSignAuth* mock_bsa() {
    return test_bsa_factory_->mock_bsa();
  }

 private:
  raw_ptr<TestBlindSignAuthFactory> test_bsa_factory_;
};

}  // namespace private_ai

#endif  // CHROME_BROWSER_PRIVATE_AI_TEST_PRIVATE_AI_SERVICE_H_

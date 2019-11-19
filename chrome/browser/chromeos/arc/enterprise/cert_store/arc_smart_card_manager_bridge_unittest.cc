// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/arc/enterprise/cert_store/arc_cert_installer.h"
#include "chrome/browser/chromeos/arc/enterprise/cert_store/arc_smart_card_manager_bridge.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider.h"
#include "chrome/common/net/x509_certificate_model_nss.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/remote_commands/remote_commands_queue.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/test_ssl_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

using testing::_;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArg;

namespace {

MATCHER_P(EqualsClientCertIdentityList, cert_names, "") {
  if (cert_names.size() != arg.size())
    return false;
  for (size_t i = 0; i < arg.size(); ++i) {
    if (!arg[i])
      return false;

    std::string cert_name =
        x509_certificate_model::GetCertNameOrNickname(arg[i].get());
    if (cert_name != cert_names[i])
      return false;
  }
  return true;
}

// Fake class for CertificateProvider.
class FakeCertificateProvider : public chromeos::CertificateProvider {
 public:
  void GetCertificates(
      base::OnceCallback<void(net::ClientCertIdentityList)> callback) override {
    std::move(callback).Run(std::move(certificates_));
  }

  // Returns true if the certificates for |cert_names| are created successfully.
  bool SetCertificates(std::vector<std::string> cert_names) {
    certificates_ = net::ClientCertIdentityList();
    for (const auto& cert_name : cert_names) {
      if (!AddCert(cert_name))
        return false;
    }
    return true;
  }

 private:
  // Returns true if the certificate for |name| is created successfully.
  bool AddCert(const std::string& name) {
    if (name.empty())
      return false;
    std::unique_ptr<crypto::RSAPrivateKey> key(
        crypto::RSAPrivateKey::Create(1024));
    scoped_refptr<net::SSLPrivateKey> ssl_private_key =
        net::WrapRSAPrivateKey(key.get());
    if (!ssl_private_key)
      return false;

    std::string cn = "CN=" + name;
    std::string der_cert;
    if (!net::x509_util::CreateSelfSignedCert(
            key->key(), net::x509_util::DIGEST_SHA256, cn, 1,
            base::Time::UnixEpoch(), base::Time::UnixEpoch(), {}, &der_cert)) {
      return false;
    }
    scoped_refptr<net::X509Certificate> cert =
        net::X509Certificate::CreateFromBytes(der_cert.data(), der_cert.size());
    if (!cert)
      return false;
    certificates_.push_back(
        std::make_unique<net::FakeClientCertIdentity>(cert, ssl_private_key));
    return true;
  }

  net::ClientCertIdentityList certificates_;
};

class MockArcCertInstaller : public ArcCertInstaller {
 public:
  MockArcCertInstaller(Profile* profile,
                       std::unique_ptr<policy::RemoteCommandsQueue> queue)
      : ArcCertInstaller(profile, std::move(queue)) {}
  MOCK_METHOD2(InstallArcCerts,
               std::set<std::string>(
                   const std::vector<net::ScopedCERTCertificate>& certs,
                   InstallArcCertsCallback callback));
};

class MockArcPolicyBridge : public ArcPolicyBridge {
 public:
  MockArcPolicyBridge(content::BrowserContext* context,
                      ArcBridgeService* bridge_service,
                      policy::PolicyService* policy_service)
      : ArcPolicyBridge(context, bridge_service, policy_service) {}
  MOCK_METHOD3(OnPolicyUpdated,
               void(const policy::PolicyNamespace& ns,
                    const policy::PolicyMap& previous,
                    const policy::PolicyMap& current));
};

std::unique_ptr<KeyedService> BuildPolicyBridge(
    ArcBridgeService* bridge_service,
    content::BrowserContext* profile) {
  return std::make_unique<MockArcPolicyBridge>(profile, bridge_service,
                                               nullptr);
}

}  // namespace

class ArcSmartCardManagerBridgeTest : public testing::Test {
 public:
  ArcSmartCardManagerBridgeTest()
      : bridge_service_(std::make_unique<ArcBridgeService>()) {}

  void SetUp() override {
    provider_ = new FakeCertificateProvider();
    installer_ = new StrictMock<MockArcCertInstaller>(
        &profile_, std::make_unique<policy::RemoteCommandsQueue>());
    policy_bridge_ = static_cast<MockArcPolicyBridge*>(
        ArcPolicyBridge::GetFactory()->SetTestingFactoryAndUse(
            &profile_,
            base::BindRepeating(&BuildPolicyBridge, bridge_service_.get())));
    bridge_ = std::make_unique<ArcSmartCardManagerBridge>(
        &profile_, bridge_service_.get(), base::WrapUnique(provider_),
        base::WrapUnique(installer_));
  }

  void TearDown() override {
    provider_ = nullptr;
    installer_ = nullptr;
    bridge_.reset();
  }

  FakeCertificateProvider* provider() { return provider_; }
  MockArcCertInstaller* installer() { return installer_; }
  MockArcPolicyBridge* policy_bridge() { return policy_bridge_; }
  ArcSmartCardManagerBridge* bridge() { return bridge_.get(); }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;

  std::unique_ptr<ArcBridgeService> bridge_service_;
  TestingProfile profile_;

  FakeCertificateProvider* provider_;  // Owned by |bridge_|.
  MockArcCertInstaller* installer_;    // Owned by |bridge_|.
  MockArcPolicyBridge* policy_bridge_;  // Not owned.

  std::unique_ptr<ArcSmartCardManagerBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(ArcSmartCardManagerBridgeTest);
};

// Tests that refreshing smart card certs completes successfully if there is no
// smart card certs.
TEST_F(ArcSmartCardManagerBridgeTest, NoSmartCardTest) {
  const std::vector<std::string> cert_names = {};
  ASSERT_TRUE(provider()->SetCertificates(cert_names));
  EXPECT_CALL(*installer(),
              InstallArcCerts(EqualsClientCertIdentityList(cert_names), _))
      .WillOnce(
          WithArg<1>(Invoke([](base::OnceCallback<void(bool result)> callback) {
            std::move(callback).Run(true);
            return std::set<std::string>();
          })));
  bridge()->Refresh(base::BindOnce([](bool result) { EXPECT_TRUE(result); }));
}

// Tests that refreshing smart card certs completes successfully if there are
// two smart card certs available.
TEST_F(ArcSmartCardManagerBridgeTest, BasicSmartCardTest) {
  const std::vector<std::string> cert_names = {"fake1", "fake2"};

  ASSERT_TRUE(provider()->SetCertificates(cert_names));
  EXPECT_CALL(*installer(),
              InstallArcCerts(EqualsClientCertIdentityList(cert_names), _))
      .WillOnce(WithArg<1>(
          Invoke([&cert_names](base::OnceCallback<void(bool result)> callback) {
            std::move(callback).Run(true);
            return std::set<std::string>(cert_names.begin(), cert_names.end());
          })));
  EXPECT_CALL(*policy_bridge(), OnPolicyUpdated(_, _, _));
  bridge()->Refresh(base::BindOnce([](bool result) { EXPECT_TRUE(result); }));
}

}  // namespace arc

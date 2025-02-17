// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/client_certificates/browser_context_delegate.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/prefs.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;

namespace client_certificates {

namespace {

scoped_refptr<net::X509Certificate> LoadTestCert() {
  static constexpr char kTestCertFileName[] = "client_1.pem";
  return net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                 kTestCertFileName);
}

class MockNetworkContext : public network::TestNetworkContext {
 public:
  explicit MockNetworkContext(
      mojo::PendingReceiver<network::mojom::NetworkContext> receiver)
      : receiver_(this, std::move(receiver)) {}
  MOCK_METHOD(void,
              FlushMatchingCachedClientCert,
              (const scoped_refptr<net::X509Certificate>&),
              (override));

 private:
  mojo::Receiver<network::mojom::NetworkContext> receiver_;
};

}  // namespace

class BrowserContextDelegateTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("profile_name");

    mojo::PendingRemote<network::mojom::NetworkContext> network_context_remote;
    mock_network_context_ = std::make_unique<MockNetworkContext>(
        network_context_remote.InitWithNewPipeAndPassReceiver());
    auto* storage_partition = profile_->GetDefaultStoragePartition();
    storage_partition->SetNetworkContextForTesting(
        std::move(network_context_remote));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<MockNetworkContext> mock_network_context_;
};

TEST_F(BrowserContextDelegateTest,
       OnClientCertificateDeleted_CallsToFlushClientCertificates) {
  auto test_cert = LoadTestCert();
  scoped_refptr<net::X509Certificate> flushed_cert;
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_network_context_.get(),
              FlushMatchingCachedClientCert(testing::_))
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&flushed_cert),
                               [&run_loop] { run_loop.Quit(); }));

  BrowserContextDelegate().OnClientCertificateDeleted(test_cert);
  run_loop.Run();

  std::vector<std::string> test_cert_pem;
  std::vector<std::string> flushed_cert_pem;
  EXPECT_TRUE(flushed_cert);
  EXPECT_TRUE(flushed_cert->GetPEMEncodedChain(&flushed_cert_pem));
  EXPECT_TRUE(test_cert->GetPEMEncodedChain(&test_cert_pem));
  EXPECT_EQ(test_cert_pem, flushed_cert_pem);
}

TEST_F(BrowserContextDelegateTest, GetIdentityName) {
  EXPECT_EQ(kManagedBrowserIdentityName,
            BrowserContextDelegate().GetIdentityName());
}

TEST_F(BrowserContextDelegateTest, GetTemporaryIdentityName) {
  EXPECT_EQ(kTemporaryManagedBrowserIdentityName,
            BrowserContextDelegate().GetTemporaryIdentityName());
}

TEST_F(BrowserContextDelegateTest, GetPolicyPref) {
  EXPECT_EQ(prefs::kProvisionManagedClientCertificateForBrowserPrefs,
            BrowserContextDelegate().GetPolicyPref());
}

}  // namespace client_certificates

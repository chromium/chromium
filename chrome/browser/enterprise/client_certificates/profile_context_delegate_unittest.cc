// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/client_certificates/profile_context_delegate.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/features.h"
#include "components/enterprise/client_certificates/core/metrics_util.h"
#include "components/enterprise/client_certificates/core/prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace client_certificates {

namespace {

constexpr char kTestProfileId[] = "test_profile_id";

scoped_refptr<net::X509Certificate> LoadTestCert() {
  static constexpr char kTestCertFileName[] = "client_1.pem";
  return net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                 kTestCertFileName);
}

scoped_refptr<net::X509Certificate> CreateCert(const std::string& common_name,
                                               base::Time not_before,
                                               base::Time not_after) {
  auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();
  leaf->SetSubjectCommonName(common_name);
  leaf->SetValidity(not_before, not_after);
  return leaf->GetX509Certificate();
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

class ProfileContextDelegateTest : public testing::Test {
 public:
  void SetUp() override {
    mojo::PendingRemote<network::mojom::NetworkContext> network_context_remote;
    mock_network_context_ = std::make_unique<MockNetworkContext>(
        network_context_remote.InitWithNewPipeAndPassReceiver());
    profile_ = std::make_unique<TestingProfile>();
    auto* storage_partition = profile_->GetDefaultStoragePartition();
    storage_partition->SetNetworkContextForTesting(
        std::move(network_context_remote));
    profile_id_service_ =
        std::make_unique<enterprise::ProfileIdService>(kTestProfileId);
  }

  ProfileContextDelegate CreateDelegate() {
    return ProfileContextDelegate(profile_.get(), profile_id_service_.get());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kClientCertCommonNameAttribute};
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<MockNetworkContext> mock_network_context_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<enterprise::ProfileIdService> profile_id_service_;
};

TEST_F(ProfileContextDelegateTest,
       OnClientCertificateDeleted_CallsToFlushClientCertificates) {
  auto test_cert = LoadTestCert();
  scoped_refptr<net::X509Certificate> flushed_cert;
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_network_context_.get(),
              FlushMatchingCachedClientCert(testing::_))
      .Times(1)
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&flushed_cert),
                               [&run_loop] { run_loop.Quit(); }));

  CreateDelegate().OnClientCertificateDeleted(test_cert);
  run_loop.Run();

  std::vector<std::string> test_cert_pem;
  std::vector<std::string> flushed_cert_pem;
  EXPECT_TRUE(flushed_cert);
  EXPECT_TRUE(flushed_cert->GetPEMEncodedChain(&flushed_cert_pem));
  EXPECT_TRUE(test_cert->GetPEMEncodedChain(&test_cert_pem));
  EXPECT_EQ(test_cert_pem, flushed_cert_pem);
}

TEST_F(ProfileContextDelegateTest, GetIdentityName) {
  EXPECT_EQ(kManagedProfileIdentityName, CreateDelegate().GetIdentityName());
}

TEST_F(ProfileContextDelegateTest, GetTemporaryIdentityName) {
  EXPECT_EQ(kTemporaryManagedProfileIdentityName,
            CreateDelegate().GetTemporaryIdentityName());
}

TEST_F(ProfileContextDelegateTest, GetPolicyPref) {
  EXPECT_EQ(prefs::kProvisionManagedClientCertificateForUserPrefs,
            CreateDelegate().GetPolicyPref());
}

TEST_F(ProfileContextDelegateTest, GetConfigPolicyPref) {
  EXPECT_EQ(prefs::kManagedClientCertificateForUserConfigPrefs,
            CreateDelegate().GetConfigPolicyPref());
}

TEST_F(ProfileContextDelegateTest, GetExpectedCommonName_DefaultIdentifier) {
  profile_->GetPrefs()->SetDict(
      prefs::kManagedClientCertificateForUserConfigPrefs,
      base::DictValue().Set(kCommonNameSourceKey, kCommonNameSourceProfileId));
  EXPECT_EQ(kTestProfileId, CreateDelegate().GetExpectedCommonName());
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(ProfileContextDelegateTest,
       GetExpectedCommonName_ComputerNameFallbackToProfileIdOnChromeOS) {
  profile_->GetPrefs()->SetDict(
      prefs::kManagedClientCertificateForUserConfigPrefs,
      base::DictValue().Set(kCommonNameSourceKey,
                            kCommonNameSourceComputerName));
  auto delegate = CreateDelegate();
  delegate.SetComputerNameForTesting("custom-workstation");
  EXPECT_EQ(kTestProfileId, delegate.GetExpectedCommonName());
}
#else
TEST_F(ProfileContextDelegateTest, GetExpectedCommonName_ComputerName) {
  profile_->GetPrefs()->SetDict(
      prefs::kManagedClientCertificateForUserConfigPrefs,
      base::DictValue().Set(kCommonNameSourceKey,
                            kCommonNameSourceComputerName));
  auto delegate = CreateDelegate();
  delegate.SetComputerNameForTesting("custom-workstation");
  EXPECT_EQ("custom-workstation", delegate.GetExpectedCommonName());
}
#endif

TEST_F(ProfileContextDelegateTest,
       GetExpectedCommonName_ComputerNameEmptyFallbackToProfileId) {
  profile_->GetPrefs()->SetDict(
      prefs::kManagedClientCertificateForUserConfigPrefs,
      base::DictValue().Set(kCommonNameSourceKey,
                            kCommonNameSourceComputerName));
  auto delegate = CreateDelegate();
  delegate.SetComputerNameForTesting("   \t\r\n   ");
  EXPECT_EQ(kTestProfileId, delegate.GetExpectedCommonName());
}

TEST_F(ProfileContextDelegateTest,
       GetExpectedCommonName_FeatureDisabledIgnoresComputerNamePolicy) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kClientCertCommonNameAttribute);

  profile_->GetPrefs()->SetDict(
      prefs::kManagedClientCertificateForUserConfigPrefs,
      base::DictValue().Set(kCommonNameSourceKey,
                            kCommonNameSourceComputerName));
  auto delegate = CreateDelegate();
  delegate.SetComputerNameForTesting("custom-workstation");
  EXPECT_EQ(kTestProfileId, delegate.GetExpectedCommonName());
}

TEST_F(ProfileContextDelegateTest,
       ShouldCertificateBeRenewed_MatchingProfileId) {
  base::HistogramTester histogram_tester;
  auto delegate = CreateDelegate();
  auto cert = CreateCert(kTestProfileId, base::Time::Now() - base::Days(1),
                         base::Time::Now() + base::Days(30));
  EXPECT_FALSE(delegate.ShouldCertificateBeRenewed(
      *cert, delegate.GetExpectedCommonName()));
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ClientCertificate.Profile.CertificateValidation.Result",
      CertificateValidationResult::kValid, 1);
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(ProfileContextDelegateTest,
       ShouldCertificateBeRenewed_MatchingDeviceName) {
  constexpr char kTestDeviceName[] = "custom-workstation";
  profile_->GetPrefs()->SetDict(
      prefs::kManagedClientCertificateForUserConfigPrefs,
      base::DictValue().Set(kCommonNameSourceKey,
                            kCommonNameSourceComputerName));
  auto delegate = CreateDelegate();
  delegate.SetComputerNameForTesting(kTestDeviceName);

  base::HistogramTester histogram_tester;
  auto cert = CreateCert(kTestDeviceName, base::Time::Now() - base::Days(1),
                         base::Time::Now() + base::Days(30));
  EXPECT_FALSE(delegate.ShouldCertificateBeRenewed(
      *cert, delegate.GetExpectedCommonName()));
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ClientCertificate.Profile.CertificateValidation.Result",
      CertificateValidationResult::kValid, 1);
}
#endif

TEST_F(
    ProfileContextDelegateTest,
    ShouldCertificateBeRenewed_DeviceNameRejectedWhenPolicyIsDefaultIdentifier) {
  constexpr char kTestDeviceName[] = "custom-workstation";
  profile_->GetPrefs()->SetDict(
      prefs::kManagedClientCertificateForUserConfigPrefs,
      base::DictValue().Set(kCommonNameSourceKey, kCommonNameSourceProfileId));
  auto delegate = CreateDelegate();
  delegate.SetComputerNameForTesting(kTestDeviceName);

  base::HistogramTester histogram_tester;
  auto cert = CreateCert(kTestDeviceName, base::Time::Now() - base::Days(1),
                         base::Time::Now() + base::Days(30));
  EXPECT_TRUE(delegate.ShouldCertificateBeRenewed(
      *cert, delegate.GetExpectedCommonName()));
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ClientCertificate.Profile.CertificateValidation.Result",
      CertificateValidationResult::kIdentityMismatch, 1);
}

}  // namespace client_certificates

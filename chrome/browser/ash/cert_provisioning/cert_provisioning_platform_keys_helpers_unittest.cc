// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_platform_keys_helpers.h"

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/time/time.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_test_helpers.h"
#include "chrome/browser/ash/platform_keys/mock_platform_keys_service.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "content/public/test/browser_task_environment.h"
#include "net/cert/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceCallback;
using chromeos::platform_keys::Status;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Key;

namespace ash {
namespace cert_provisioning {
namespace {

class PlatformKeysHelpersTest : public ::testing::Test {
 public:
  PlatformKeysHelpersTest() : certificate_helper_(&platform_keys_service_) {}
  PlatformKeysHelpersTest(const PlatformKeysHelpersTest&) = delete;
  PlatformKeysHelpersTest& operator=(const PlatformKeysHelpersTest&) = delete;
  ~PlatformKeysHelpersTest() override = default;

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ProfileHelperForTesting profile_helper_;
  platform_keys::MockPlatformKeysService platform_keys_service_;
  CertificateHelperForTesting certificate_helper_;
};

//================= CertProvisioningCertIteratorTest ===========================

class CertProvisioningCertIteratorTest : public PlatformKeysHelpersTest {};

class IteratorCallbackObserver {
 public:
  CertIteratorForEachCallback GetForEachCallback() {
    return base::BindRepeating(&IteratorCallbackObserver::ForEachCallback,
                               base::Unretained(this));
  }

  CertIteratorOnFinishedCallback GetOnFinishedCallback() {
    return base::BindOnce(&IteratorCallbackObserver::OnFinishedCallback,
                          base::Unretained(this));
  }

  MOCK_METHOD(void,
              ForEachCallback,
              (scoped_refptr<net::X509Certificate> cert,
               const CertProfileId& cert_id,
               Status status));

  MOCK_METHOD(void, OnFinishedCallback, (Status status));
};

TEST_F(CertProvisioningCertIteratorTest, NoCertificates) {
  const CertScope kCertScope = CertScope::kDevice;

  base::RunLoop run_loop;
  IteratorCallbackObserver callback_observer;

  EXPECT_CALL(callback_observer, OnFinishedCallback(Status::kSuccess))
      .Times(1)
      .WillOnce(Invoke(&run_loop, &base::RunLoop::Quit));

  CertIterator cert_iterator(kCertScope, &platform_keys_service_);
  cert_iterator.IterateAll(callback_observer.GetForEachCallback(),
                           callback_observer.GetOnFinishedCallback());
  run_loop.Run();
}

TEST_F(CertProvisioningCertIteratorTest, OneCertificate) {
  const CertScope kCertScope = CertScope::kDevice;
  const char kCertProfileId[] = "cert_profile_id_1";
  auto cert = certificate_helper_.AddCert(kCertScope, kCertProfileId);

  base::RunLoop run_loop;
  IteratorCallbackObserver callback_observer;

  {
    testing::InSequence seq;
    EXPECT_CALL(callback_observer,
                ForEachCallback(/*cert=*/cert,
                                /*cert_id=*/kCertProfileId, Status::kSuccess))
        .Times(1);
    EXPECT_CALL(callback_observer, OnFinishedCallback(Status::kSuccess))
        .Times(1)
        .WillOnce(Invoke(&run_loop, &base::RunLoop::Quit));
  }

  CertIterator cert_iterator(kCertScope, &platform_keys_service_);
  cert_iterator.IterateAll(callback_observer.GetForEachCallback(),
                           callback_observer.GetOnFinishedCallback());
  run_loop.Run();
}

TEST_F(CertProvisioningCertIteratorTest, ManyCertificates) {
  const CertScope kCertScope = CertScope::kDevice;
  std::vector<std::string> ids = {"id1, ids2, id3, id4"};

  base::RunLoop run_loop;
  IteratorCallbackObserver callback_observer;

  testing::ExpectationSet expect_set;
  for (const auto& id : ids) {
    auto cert = certificate_helper_.AddCert(kCertScope, id);
    expect_set += EXPECT_CALL(callback_observer,
                              ForEachCallback(/*cert=*/cert,
                                              /*cert_id=*/id, Status::kSuccess))
                      .Times(1);
  }

  EXPECT_CALL(callback_observer, OnFinishedCallback(Status::kSuccess))
      .Times(1)
      .After(expect_set)
      .WillOnce(Invoke(&run_loop, &base::RunLoop::Quit));

  CertIterator cert_iterator(kCertScope, &platform_keys_service_);
  cert_iterator.IterateAll(callback_observer.GetForEachCallback(),
                           callback_observer.GetOnFinishedCallback());
  run_loop.Run();
}

TEST_F(CertProvisioningCertIteratorTest, CertificateWithError) {
  const CertScope kCertScope = CertScope::kDevice;
  const Status kErrorStatus = Status::kErrorInternal;

  certificate_helper_.AddCert(kCertScope, /*cert_profile_id=*/"id1");
  certificate_helper_.AddCert(kCertScope, /*cert_profile_id=*/"id2");
  certificate_helper_.AddCert(kCertScope, /*cert_profile_id=*/std::nullopt,
                              kErrorStatus);
  certificate_helper_.AddCert(kCertScope, /*cert_profile_id=*/"id3");
  certificate_helper_.AddCert(kCertScope, /*cert_profile_id=*/"id4");

  base::RunLoop run_loop;
  IteratorCallbackObserver callback_observer;
  EXPECT_CALL(callback_observer, OnFinishedCallback(kErrorStatus))
      .Times(1)
      .WillOnce(Invoke(&run_loop, &base::RunLoop::Quit));

  CertIterator cert_iterator(kCertScope, &platform_keys_service_);
  cert_iterator.IterateAll(callback_observer.GetForEachCallback(),
                           callback_observer.GetOnFinishedCallback());
  run_loop.Run();
}

//================= CertProvisioningCertGetter =================================

class CertProvisioningCertGetter : public PlatformKeysHelpersTest {};

using CertMap =
    base::flat_map<CertProfileId, scoped_refptr<net::X509Certificate>>;

class GetterCallbackObserver {
 public:
  LatestCertsWithIdsGetterCallback GetCallback() {
    return base::BindOnce(&GetterCallbackObserver::Callback,
                          base::Unretained(this));
  }

  const CertMap& GetMap() { return cert_map_; }
  Status GetStatus() const { return status_; }

  void WaitForCallback() { loop_.Run(); }

 protected:
  void Callback(CertMap certs_with_ids, Status status) {
    cert_map_ = std::move(certs_with_ids);
    status_ = status;
    loop_.Quit();
  }

  base::RunLoop loop_;
  CertMap cert_map_;
  Status status_ = Status::kSuccess;
};

TEST_F(CertProvisioningCertGetter, NoCertificates) {
  const CertScope kCertScope = CertScope::kDevice;

  GetterCallbackObserver callback_observer;
  LatestCertsWithIdsGetter cert_getter(kCertScope, &platform_keys_service_);
  cert_getter.GetCertsWithIds(callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_TRUE(callback_observer.GetMap().empty());
  EXPECT_EQ(callback_observer.GetStatus(), Status::kSuccess);
}

TEST_F(CertProvisioningCertGetter, SingleCertificateWithId) {
  const CertScope kCertScope = CertScope::kDevice;
  const char kCertProfileId[] = "cert_profile_id_1";
  CertMap cert_map;

  cert_map[kCertProfileId] =
      certificate_helper_.AddCert(kCertScope, kCertProfileId);

  GetterCallbackObserver callback_observer;
  LatestCertsWithIdsGetter cert_getter(kCertScope, &platform_keys_service_);
  cert_getter.GetCertsWithIds(callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_EQ(callback_observer.GetMap(), cert_map);
  EXPECT_EQ(callback_observer.GetStatus(), Status::kSuccess);
}

TEST_F(CertProvisioningCertGetter, ManyCertificatesWithId) {
  const CertScope kCertScope = CertScope::kDevice;
  std::vector<std::string> ids{"cert_profile_id_0", "cert_profile_id_1",
                               "cert_profile_id_2"};
  CertMap cert_map;

  for (const auto& id : ids) {
    cert_map[id] = certificate_helper_.AddCert(kCertScope, id);
  }

  GetterCallbackObserver callback_observer;
  LatestCertsWithIdsGetter cert_getter(kCertScope, &platform_keys_service_);
  cert_getter.GetCertsWithIds(callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_EQ(callback_observer.GetMap(), cert_map);
  EXPECT_EQ(callback_observer.GetStatus(), Status::kSuccess);
}

TEST_F(CertProvisioningCertGetter, ManyCertificatesWithoutId) {
  const CertScope kCertScope = CertScope::kDevice;
  size_t cert_count = 4;
  for (size_t i = 0; i < cert_count; ++i) {
    certificate_helper_.AddCert(kCertScope, /*cert_profile_id=*/std::nullopt);
  }

  GetterCallbackObserver callback_observer;
  LatestCertsWithIdsGetter cert_getter(kCertScope, &platform_keys_service_);
  cert_getter.GetCertsWithIds(callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_TRUE(callback_observer.GetMap().empty());
  EXPECT_EQ(callback_observer.GetStatus(), Status::kSuccess);
}

TEST_F(CertProvisioningCertGetter, CertificatesWithAndWithoutIds) {
  const CertScope kCertScope = CertScope::kDevice;
  CertMap cert_map;

  size_t cert_without_id_count = 4;
  for (size_t i = 0; i < cert_without_id_count; ++i) {
    certificate_helper_.AddCert(kCertScope, /*cert_profile_id=*/std::nullopt);
  }

  std::vector<std::string> ids{"cert_profile_id_0", "cert_profile_id_1",
                               "cert_profile_id_2"};
  for (const auto& id : ids) {
    cert_map[id] = certificate_helper_.AddCert(kCertScope, id);
  }

  GetterCallbackObserver callback_observer;
  LatestCertsWithIdsGetter cert_getter(kCertScope, &platform_keys_service_);
  cert_getter.GetCertsWithIds(callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_EQ(callback_observer.GetMap(), cert_map);
  EXPECT_EQ(callback_observer.GetStatus(), Status::kSuccess);
}

//================= CertProvisioningCertDeleterTest ============================

class CertProvisioningCertDeleterTest : public PlatformKeysHelpersTest {};

class DeleterCallbackObserver {
 public:
  CertDeleterCallback GetCallback() {
    return base::BindOnce(&DeleterCallbackObserver::Callback,
                          base::Unretained(this));
  }

  Status GetStatus() { return status_; }
  void WaitForCallback() { loop_.Run(); }

 protected:
  void Callback(Status status) {
    status_ = status;
    loop_.Quit();
  }

  base::RunLoop loop_;
  Status status_;
};

TEST_F(CertProvisioningCertDeleterTest, NoCertificates) {
  const CertScope kCertScope = CertScope::kDevice;
  base::flat_set<CertProfileId> cert_ids_to_keep;

  EXPECT_CALL(platform_keys_service_, RemoveCertificate).Times(0);

  DeleterCallbackObserver callback_observer;
  CertDeleter cert_deleter(kCertScope, &platform_keys_service_);
  cert_deleter.DeleteCerts(std::move(cert_ids_to_keep),
                           callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_EQ(callback_observer.GetStatus(), Status::kSuccess);
}

TEST_F(CertProvisioningCertDeleterTest, SomeCertsWithoutPolicy) {
  const CertScope kCertScope = CertScope::kDevice;
  std::vector<std::string> cert_ids_to_delete{"id1", "id2", "id3"};
  base::flat_set<CertProfileId> cert_ids_to_keep{"id4", "id5", "id6"};

  for (const auto& id : cert_ids_to_delete) {
    auto cert = certificate_helper_.AddCert(kCertScope, id);
    EXPECT_CALL(platform_keys_service_,
                RemoveCertificate(GetPlatformKeysTokenId(kCertScope), cert,
                                  /*callback=*/_))
        .Times(1)
        .WillOnce(RunOnceCallback<2>(Status::kSuccess));
  }

  for (const auto& id : cert_ids_to_keep) {
    certificate_helper_.AddCert(kCertScope, id);
  }

  DeleterCallbackObserver callback_observer;
  CertDeleter cert_deleter(kCertScope, &platform_keys_service_);
  cert_deleter.DeleteCerts(std::move(cert_ids_to_keep),
                           callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_EQ(callback_observer.GetStatus(), Status::kSuccess);
}

TEST_F(CertProvisioningCertDeleterTest, CertWasRenewed) {
  const CertScope kCertScope = CertScope::kDevice;
  const char kRenewedCertId[] = "id1";
  const char kCertId2[] = "id2";

  base::Time t1 = base::Time::Now();
  base::Time t2 = t1 + base::Days(30);
  base::Time t3 = t2 + base::Days(30);

  auto cert = certificate_helper_.AddCert(kCertScope, kRenewedCertId,
                                          Status::kSuccess, t1, t2);
  EXPECT_CALL(platform_keys_service_,
              RemoveCertificate(GetPlatformKeysTokenId(kCertScope), cert,
                                /*callback=*/_))
      .Times(1)
      .WillOnce(RunOnceCallback<2>(Status::kSuccess));

  certificate_helper_.AddCert(kCertScope, kRenewedCertId, Status::kSuccess, t2,
                              t3);
  certificate_helper_.AddCert(kCertScope, kCertId2);

  DeleterCallbackObserver callback_observer;
  CertDeleter cert_deleter(kCertScope, &platform_keys_service_);
  cert_deleter.DeleteCerts(/*cert_ids_to_keep=*/{kRenewedCertId, kCertId2},
                           callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_EQ(callback_observer.GetStatus(), Status::kSuccess);
}

TEST_F(CertProvisioningCertDeleterTest, PropogateError) {
  const CertScope kCertScope = CertScope::kDevice;
  const Status kErrorStatus = Status::kErrorInternal;

  certificate_helper_.AddCert(kCertScope, "id1");
  EXPECT_CALL(platform_keys_service_, RemoveCertificate)
      .WillOnce(RunOnceCallback<2>(kErrorStatus));

  DeleterCallbackObserver callback_observer;
  CertDeleter cert_deleter(kCertScope, &platform_keys_service_);
  cert_deleter.DeleteCerts(/*cert_ids_to_keep=*/{},  // Delete all certs.
                           callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_EQ(callback_observer.GetStatus(), kErrorStatus);
}

}  // namespace
}  // namespace cert_provisioning
}  // namespace ash

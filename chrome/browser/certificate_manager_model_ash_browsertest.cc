// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_manager_model.h"

#include <string>

#include "base/files/file_util.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/crosapi/cert_database_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/test/test_data_directory.h"

// The correct password for the client.p12 file.
const char16_t kPassword[] = u"12345";

struct FakeObserver : CertificateManagerModel::Observer {
  void CertificatesRefreshed() override {}
};

class CertificateManagerModelBrowserTestBase
    : public MixinBasedInProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    logged_in_user_mixin_.LogInUser();

    base::test::TestFuture<std::unique_ptr<CertificateManagerModel>>
        model_waiter;
    CertificateManagerModel::Create(browser()->profile(), &fake_observer_,
                                    model_waiter.GetCallback());
    certificate_manager_model_ = model_waiter.Take();
  }

  void TearDownOnMainThread() override { certificate_manager_model_.reset(); }

  std::string ReadTestFile(const std::string& file_name) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath file_path =
        net::GetTestCertsDirectory().AppendASCII(file_name);
    std::string file_data;
    if (!base::ReadFileToString(file_path, &file_data)) {
      ADD_FAILURE() << "Couldn't read " << file_path;
      return {};
    }
    return file_data;
  }

  const std::string& GetPkcs12() {
    static const base::NoDestructor<std::string> pkcs12(
        ReadTestFile("client.p12"));
    return *pkcs12;
  }

 protected:
  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      ash::LoggedInUserMixin::LogInType::kConsumer};

  FakeObserver fake_observer_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<crypto::ScopedTestNSSDB> public_slot_ =
      std::make_unique<crypto::ScopedTestNSSDB>();
  std::unique_ptr<CertificateManagerModel> certificate_manager_model_;
};

// Test that when OnPkcs12CertDualWritten() is called from Lacros, the
// kNssChapsDualWrittenCertsExist preference is stored.
IN_PROC_BROWSER_TEST_F(CertificateManagerModelBrowserTestBase,
                       LacrosCallStoresThePref) {
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kNssChapsDualWrittenCertsExist));

  crosapi::CertDatabaseAsh* const cert_database_ash =
      crosapi::CrosapiManager::Get()->crosapi_ash()->cert_database_ash();
  cert_database_ash->OnPkcs12CertDualWritten();

  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kNssChapsDualWrittenCertsExist));
}

class CertificateManagerModelEnablePkcs12DualWrite
    : public CertificateManagerModelBrowserTestBase {
 public:
  CertificateManagerModelEnablePkcs12DualWrite() {
    feature_list_.InitAndEnableFeature(
        chromeos::features::kEnablePkcs12ToChapsDualWrite);
  }
};

// Test ImportFromPKCS12 with dual-write enabled.
IN_PROC_BROWSER_TEST_F(CertificateManagerModelEnablePkcs12DualWrite,
                       DualWriteIsEnabled) {
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kNssChapsDualWrittenCertsExist));

  // Non-extractable certs should not be dual-written and should not set the
  // related preference.
  {
    base::test::TestFuture<int> import_waiter;
    certificate_manager_model_->ImportFromPKCS12(
        public_slot_->slot(), GetPkcs12(), kPassword,
        /*is_extractable=*/false, import_waiter.GetCallback());
    EXPECT_EQ(import_waiter.Get(), net::OK);
    EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
        prefs::kNssChapsDualWrittenCertsExist));
  }

  // Extractable certs should be dual-written and should set the related
  // preference.
  {
    base::test::TestFuture<int> import_waiter;
    certificate_manager_model_->ImportFromPKCS12(
        public_slot_->slot(), GetPkcs12(), kPassword,
        /*is_extractable=*/true, import_waiter.GetCallback());
    EXPECT_EQ(import_waiter.Get(), net::OK);
    EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
        prefs::kNssChapsDualWrittenCertsExist));
  }
}

class CertificateManagerModelDisablePkcs12DualWrite
    : public CertificateManagerModelBrowserTestBase {
 public:
  CertificateManagerModelDisablePkcs12DualWrite() {
    feature_list_.InitAndDisableFeature(
        chromeos::features::kEnablePkcs12ToChapsDualWrite);
  }
};

// Test ImportFromPKCS12 with dual-write disabled. Everything should work as
// usual, kNssChapsDualWrittenCertsExist preference should not be set for all
// cases.
IN_PROC_BROWSER_TEST_F(CertificateManagerModelDisablePkcs12DualWrite,
                       DualWriteIsDisabled) {
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kNssChapsDualWrittenCertsExist));

  {
    base::test::TestFuture<int> import_waiter;
    certificate_manager_model_->ImportFromPKCS12(
        public_slot_->slot(), GetPkcs12(), kPassword,
        /*is_extractable=*/false, import_waiter.GetCallback());
    EXPECT_EQ(import_waiter.Get(), net::OK);
    EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
        prefs::kNssChapsDualWrittenCertsExist));
  }

  {
    base::test::TestFuture<int> import_waiter;
    certificate_manager_model_->ImportFromPKCS12(
        public_slot_->slot(), GetPkcs12(), kPassword,
        /*is_extractable=*/true, import_waiter.GetCallback());
    EXPECT_EQ(import_waiter.Get(), net::OK);
    EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
        prefs::kNssChapsDualWrittenCertsExist));
  }
}

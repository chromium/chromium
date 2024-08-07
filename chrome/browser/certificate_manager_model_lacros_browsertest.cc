// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_manager_model.h"

#include <string>

#include "base/files/file_util.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_init_params.h"
#include "content/public/test/browser_test.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Mock;

struct FakeObserver : CertificateManagerModel::Observer {
  void CertificatesRefreshed() override {}
};

class MockCertDatabase : public crosapi::mojom::CertDatabase {
 public:
  MOCK_METHOD(void,
              GetCertDatabaseInfo,
              (GetCertDatabaseInfoCallback callback),
              (override));
  MOCK_METHOD(void,
              OnCertsChangedInLacros,
              (crosapi::mojom::CertDatabaseChangeType),
              (override));
  MOCK_METHOD(void,
              AddAshCertDatabaseObserver,
              (mojo::PendingRemote<crosapi::mojom::AshCertDatabaseObserver>),
              (override));
  MOCK_METHOD(void,
              SetCertsProvidedByExtension,
              (const std::string& extension_id,
               const chromeos::certificate_provider::CertificateInfoList&),
              (override));
  MOCK_METHOD(void, OnPkcs12CertDualWritten, (), (override));
};

class CertificateManagerModelBrowserTestBase : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Emulate a high enough version of ash to allow the tests calling
    // OnPkcs12CertDualWritten().
    crosapi::mojom::BrowserInitParamsPtr init_params =
        crosapi::mojom::BrowserInitParams::New();
    init_params->interface_versions.emplace();  // Populate the std::optional.
    init_params->interface_versions
        .value()[crosapi::mojom::CertDatabase::Uuid_] = 5;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));

    // Redirect the calls to mojom::CertDatabase interface to the mock object.
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        cert_db_receiver_.BindNewPipeAndPassRemote());

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
  FakeObserver fake_observer_;
  MockCertDatabase mock_cert_db_;
  mojo::Receiver<crosapi::mojom::CertDatabase> cert_db_receiver_{
      &mock_cert_db_};

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<crypto::ScopedTestNSSDB> public_slot_ =
      std::make_unique<crypto::ScopedTestNSSDB>();
  std::unique_ptr<CertificateManagerModel> certificate_manager_model_;
};

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/client_certificate_fetcher.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/browser_task_environment.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_signals {
namespace {
const char kRequestingUrl[] = "https://www.example.com";

class MockClientCertStore : public net::ClientCertStore {
 public:
  void GetClientCerts(const net::SSLCertRequestInfo& cert_request_info,
                      ClientCertListCallback callback) override {
    callback_ = std::move(callback);
  }

  void SimulateCallback(net::ClientCertIdentityList certs) {
    std::move(callback_).Run(std::move(certs));
  }

  ClientCertListCallback callback_;
};

class FetchCertificateCallbackWrapper {
 public:
  void OnFetchCertificateFinished(
      std::unique_ptr<net::ClientCertIdentity> cert) {
    cert_ = std::move(cert);
    ++callbacks_called_;
  }

  int callbacks_called_{0};
  std::unique_ptr<net::ClientCertIdentity> cert_;
};

}  // namespace

class ClientCertificateFetcherTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    profile_ = profile_manager_->CreateTestingProfile("TestProfile");
  }

  void TearDown() override {
    HostContentSettingsMap* m =
        HostContentSettingsMapFactory::GetForProfile(profile());
    m->ClearSettingsForOneType(ContentSettingsType::AUTO_SELECT_CERTIFICATE);
  }

  net::ClientCertIdentityList GetDefaultClientCertList() {
    EXPECT_EQ(0UL, client_certs_.size());

    client_certs_.push_back(
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_1.pem"));
    client_certs_.push_back(
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_2.pem"));

    return net::FakeClientCertIdentityListFromCertificateList(client_certs_);
  }

  void SetPolicyValueInContentSettings(base::Value::List filters) {
    HostContentSettingsMap* m =
        HostContentSettingsMapFactory::GetForProfile(profile());

    base::Value::Dict root;
    root.Set("filters", std::move(filters));

    m->SetWebsiteSettingDefaultScope(
        GURL(kRequestingUrl), GURL(),
        ContentSettingsType::AUTO_SELECT_CERTIFICATE,
        base::Value(std::move(root)));
  }

  base::Value::Dict CreateFilterValue(const std::string& issuer,
                                      const std::string& subject) {
    EXPECT_FALSE(issuer.empty() && subject.empty());

    base::Value::Dict filter;
    if (!issuer.empty()) {
      base::Value::Dict issuer_value;
      issuer_value.Set("CN", issuer);
      filter.Set("ISSUER", std::move(issuer_value));
    }

    if (!subject.empty()) {
      base::Value::Dict subject_value;
      subject_value.Set("CN", subject);
      filter.Set("SUBJECT", std::move(subject_value));
    }

    return filter;
  }

  TestingProfile* profile() { return profile_; }

  std::vector<scoped_refptr<net::X509Certificate>>& client_certs() {
    return client_certs_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

  std::vector<scoped_refptr<net::X509Certificate>> client_certs_;
};

TEST_F(ClientCertificateFetcherTest, NoCertStoreImmediatelyCallsBack) {
  ClientCertificateFetcher fetcher(nullptr, profile());
  FetchCertificateCallbackWrapper wrapper;

  fetcher.FetchAutoSelectedCertificateForUrl(
      GURL(kRequestingUrl),
      base::BindOnce(
          &FetchCertificateCallbackWrapper::OnFetchCertificateFinished,
          base::Unretained(&wrapper)));

  EXPECT_EQ(1, wrapper.callbacks_called_);
  EXPECT_EQ(nullptr, wrapper.cert_);
}

TEST_F(ClientCertificateFetcherTest, NoMatchingCertStoreCallsBackNull) {
  std::unique_ptr<MockClientCertStore> cert_store =
      std::make_unique<MockClientCertStore>();

  // Keep a raw pointer to simulate running the callback.
  MockClientCertStore* cert_store_ptr = cert_store.get();
  ClientCertificateFetcher fetcher(std::move(cert_store), profile());
  FetchCertificateCallbackWrapper wrapper;

  fetcher.FetchAutoSelectedCertificateForUrl(
      GURL(kRequestingUrl),
      base::BindOnce(
          &FetchCertificateCallbackWrapper::OnFetchCertificateFinished,
          base::Unretained(&wrapper)));

  net::ClientCertIdentityList certs;
  cert_store_ptr->SimulateCallback(std::move(certs));

  EXPECT_EQ(1, wrapper.callbacks_called_);
  EXPECT_EQ(nullptr, wrapper.cert_);
}

TEST_F(ClientCertificateFetcherTest, ReturnsFirstCertIfMatching) {
  std::unique_ptr<MockClientCertStore> cert_store =
      std::make_unique<MockClientCertStore>();

  base::Value::List filters;
  filters.Append(CreateFilterValue("", "Client Cert A"));

  SetPolicyValueInContentSettings(std::move(filters));

  // Keep a raw pointer to simulate running the callback.
  MockClientCertStore* cert_store_ptr = cert_store.get();
  ClientCertificateFetcher fetcher(std::move(cert_store), profile());
  FetchCertificateCallbackWrapper wrapper;

  fetcher.FetchAutoSelectedCertificateForUrl(
      GURL(kRequestingUrl),
      base::BindOnce(
          &FetchCertificateCallbackWrapper::OnFetchCertificateFinished,
          base::Unretained(&wrapper)));

  net::ClientCertIdentityList certs;
  cert_store_ptr->SimulateCallback(GetDefaultClientCertList());

  EXPECT_EQ(1, wrapper.callbacks_called_);
  EXPECT_NE(nullptr, wrapper.cert_);
  EXPECT_TRUE(wrapper.cert_->certificate()->EqualsIncludingChain(
      client_certs()[0].get()));
}

TEST_F(ClientCertificateFetcherTest, ReturnsSecondCertIfMatching) {
  std::unique_ptr<MockClientCertStore> cert_store =
      std::make_unique<MockClientCertStore>();

  base::Value::List filters;
  filters.Append(CreateFilterValue("E CA", ""));

  SetPolicyValueInContentSettings(std::move(filters));

  // Keep a raw pointer to simulate running the callback.
  MockClientCertStore* cert_store_ptr = cert_store.get();
  ClientCertificateFetcher fetcher(std::move(cert_store), profile());
  FetchCertificateCallbackWrapper wrapper;

  fetcher.FetchAutoSelectedCertificateForUrl(
      GURL(kRequestingUrl),
      base::BindOnce(
          &FetchCertificateCallbackWrapper::OnFetchCertificateFinished,
          base::Unretained(&wrapper)));

  net::ClientCertIdentityList certs;
  cert_store_ptr->SimulateCallback(GetDefaultClientCertList());

  EXPECT_EQ(1, wrapper.callbacks_called_);
  EXPECT_NE(nullptr, wrapper.cert_);
  EXPECT_TRUE(wrapper.cert_->certificate()->EqualsIncludingChain(
      client_certs()[1].get()));
}

TEST_F(ClientCertificateFetcherTest, ReturnsNoCertIfNoFiltersMatch) {
  std::unique_ptr<MockClientCertStore> cert_store =
      std::make_unique<MockClientCertStore>();

  base::Value::List filters;
  filters.Append(CreateFilterValue("E CA", "Bad Subject"));

  SetPolicyValueInContentSettings(std::move(filters));

  // Keep a raw pointer to simulate running the callback.
  MockClientCertStore* cert_store_ptr = cert_store.get();
  ClientCertificateFetcher fetcher(std::move(cert_store), profile());
  FetchCertificateCallbackWrapper wrapper;

  fetcher.FetchAutoSelectedCertificateForUrl(
      GURL(kRequestingUrl),
      base::BindOnce(
          &FetchCertificateCallbackWrapper::OnFetchCertificateFinished,
          base::Unretained(&wrapper)));

  net::ClientCertIdentityList certs;
  cert_store_ptr->SimulateCallback(GetDefaultClientCertList());

  EXPECT_EQ(1, wrapper.callbacks_called_);
  EXPECT_EQ(nullptr, wrapper.cert_);
}

TEST_F(ClientCertificateFetcherTest, ReturnsNoCertIfNoFilters) {
  std::unique_ptr<MockClientCertStore> cert_store =
      std::make_unique<MockClientCertStore>();

  // Keep a raw pointer to simulate running the callback.
  MockClientCertStore* cert_store_ptr = cert_store.get();
  ClientCertificateFetcher fetcher(std::move(cert_store), profile());
  FetchCertificateCallbackWrapper wrapper;

  fetcher.FetchAutoSelectedCertificateForUrl(
      GURL(kRequestingUrl),
      base::BindOnce(
          &FetchCertificateCallbackWrapper::OnFetchCertificateFinished,
          base::Unretained(&wrapper)));

  net::ClientCertIdentityList certs;
  cert_store_ptr->SimulateCallback(GetDefaultClientCertList());

  EXPECT_EQ(1, wrapper.callbacks_called_);
  EXPECT_EQ(nullptr, wrapper.cert_);
}

}  // namespace enterprise_signals

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/system_client_cert_network_observer.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/byte_size.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chromeos/ash/components/network/policy_certificate_provider.h"
#include "chromeos/components/onc/certificate_scope.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_info.h"
#include "net/ssl/ssl_private_key.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr char kProxyHost[] = "proxy.example.com";
constexpr uint16_t kProxyPort = 443;
constexpr char kMatchingRule[] =
    R"({"pattern":"https://proxy.example.com","filter":{"ISSUER":{"CN":"B CA"}}})";
constexpr char kNonMatchingFilterRule[] =
    R"({"pattern":"https://proxy.example.com","filter":{"ISSUER":{"CN":"Other CA"}}})";
constexpr char kOtherHostRule[] =
    R"({"pattern":"https://other.example.com","filter":{"ISSUER":{"CN":"B CA"}}})";

class TestClientCertificateResponder
    : public network::mojom::ClientCertificateResponder {
 public:
  enum class Outcome {
    kContinueWithCertificate,
    kContinueWithoutCertificate,
    kCancelRequest,
  };

  TestClientCertificateResponder() = default;
  ~TestClientCertificateResponder() override = default;

  mojo::PendingRemote<network::mojom::ClientCertificateResponder> Bind() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  Outcome WaitForOutcome() { return outcome_future_.Get(); }

  const scoped_refptr<net::X509Certificate>& cert() const { return cert_; }

  const std::vector<uint16_t>& algorithm_preferences() const {
    return algorithm_preferences_;
  }

  mojo::Remote<network::mojom::SSLPrivateKey>& ssl_private_key() {
    return ssl_private_key_;
  }

  // network::mojom::ClientCertificateResponder:
  void ContinueWithCertificate(
      const scoped_refptr<net::X509Certificate>& x509_certificate,
      const std::string& provider_name,
      const std::vector<uint16_t>& algorithm_preferences,
      mojo::PendingRemote<network::mojom::SSLPrivateKey> ssl_private_key)
      override {
    cert_ = x509_certificate;
    algorithm_preferences_ = algorithm_preferences;
    ssl_private_key_.Bind(std::move(ssl_private_key));
    outcome_future_.SetValue(Outcome::kContinueWithCertificate);
  }

  void ContinueWithoutCertificate() override {
    outcome_future_.SetValue(Outcome::kContinueWithoutCertificate);
  }

  void CancelRequest() override {
    outcome_future_.SetValue(Outcome::kCancelRequest);
  }

 private:
  mojo::Receiver<network::mojom::ClientCertificateResponder> receiver_{this};
  base::test::TestFuture<Outcome> outcome_future_;
  scoped_refptr<net::X509Certificate> cert_;
  std::vector<uint16_t> algorithm_preferences_;
  mojo::Remote<network::mojom::SSLPrivateKey> ssl_private_key_;
};

class TestAuthChallengeResponder
    : public network::mojom::AuthChallengeResponder {
 public:
  mojo::PendingRemote<network::mojom::AuthChallengeResponder> Bind() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  std::optional<net::AuthCredentials> WaitForCredentials() {
    return credentials_future_.Get();
  }

  // network::mojom::AuthChallengeResponder:
  void OnAuthCredentials(
      const std::optional<net::AuthCredentials>& credentials) override {
    credentials_future_.SetValue(credentials);
  }

 private:
  mojo::Receiver<network::mojom::AuthChallengeResponder> receiver_{this};
  base::test::TestFuture<std::optional<net::AuthCredentials>>
      credentials_future_;
};

class FakePolicyCertificateProvider : public PolicyCertificateProvider {
 public:
  FakePolicyCertificateProvider() = default;
  ~FakePolicyCertificateProvider() override {
    for (auto& observer : observer_list_) {
      observer.OnPolicyCertificateProviderDestroying();
    }
  }

  void AddPolicyProvidedCertsObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemovePolicyProvidedCertsObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  net::CertificateList GetAllServerAndAuthorityCertificates(
      const chromeos::onc::CertificateScope& scope) const override {
    EXPECT_EQ(chromeos::onc::CertificateScope::Default(), scope);
    return server_and_authority_certificates_;
  }

  net::CertificateList GetAllAuthorityCertificates(
      const chromeos::onc::CertificateScope& scope) const override {
    NOTREACHED();
  }

  net::CertificateList GetWebTrustedCertificates(
      const chromeos::onc::CertificateScope& scope) const override {
    NOTREACHED();
  }

  net::CertificateList GetCertificatesWithoutWebTrust(
      const chromeos::onc::CertificateScope& scope) const override {
    NOTREACHED();
  }

  const std::set<std::string>& GetExtensionIdsWithPolicyCertificates()
      const override {
    NOTREACHED();
  }

  void SetServerAndAuthorityCertificates(net::CertificateList certs) {
    server_and_authority_certificates_ = std::move(certs);
  }

  void NotifyCertsChanged() {
    for (auto& observer : observer_list_) {
      observer.OnPolicyProvidedCertsChanged();
    }
  }

 private:
  base::ObserverList<PolicyCertificateProvider::Observer,
                     /*check_empty=*/true>::Unchecked observer_list_;
  net::CertificateList server_and_authority_certificates_;
};

class FakeClientCertStore : public net::ClientCertStore {
 public:
  explicit FakeClientCertStore(net::ClientCertIdentityList certs)
      : certs_(std::move(certs)) {}
  ~FakeClientCertStore() override = default;

  void GetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback) override {
    std::move(callback).Run(std::move(certs_));
  }

 private:
  net::ClientCertIdentityList certs_;
};

class DelayedClientCertStore : public net::ClientCertStore {
 public:
  explicit DelayedClientCertStore(
      base::OnceCallback<void(ClientCertListCallback)> on_get_certs)
      : on_get_certs_(std::move(on_get_certs)) {}
  ~DelayedClientCertStore() override = default;

  void GetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback) override {
    std::move(on_get_certs_).Run(std::move(callback));
  }

 private:
  base::OnceCallback<void(ClientCertListCallback)> on_get_certs_;
};

class DelayedClientCertIdentity : public net::ClientCertIdentity {
 public:
  using KeyCallback =
      base::OnceCallback<void(scoped_refptr<net::SSLPrivateKey>)>;

  DelayedClientCertIdentity(
      scoped_refptr<net::X509Certificate> cert,
      scoped_refptr<net::SSLPrivateKey> key,
      base::OnceCallback<void(KeyCallback, scoped_refptr<net::SSLPrivateKey>)>
          on_acquire_key)
      : net::ClientCertIdentity(std::move(cert)),
        key_(std::move(key)),
        on_acquire_key_(std::move(on_acquire_key)) {}
  ~DelayedClientCertIdentity() override = default;

  void AcquirePrivateKey(KeyCallback private_key_callback) override {
    std::move(on_acquire_key_)
        .Run(std::move(private_key_callback), std::move(key_));
  }

 private:
  scoped_refptr<net::SSLPrivateKey> key_;
  base::OnceCallback<void(KeyCallback, scoped_refptr<net::SSLPrivateKey>)>
      on_acquire_key_;
};

class SystemClientCertNetworkObserverTest : public testing::Test {
 protected:
  void SetUp() override {
    ash::test::UserSessionTestEnvironment::RegisterLocalStatePrefs(
        local_state_.registry());
    session_env_ =
        std::make_unique<ash::test::UserSessionTestEnvironment>(&local_state_);

    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy_provider_.Init();
    policy_service_ = std::make_unique<policy::PolicyServiceImpl>(
        policy::PolicyServiceImpl::Providers{&policy_provider_});
    policy_cert_provider_ = std::make_unique<FakePolicyCertificateProvider>();

    client_cert_ = net::FakeClientCertIdentity::CreateFromCertAndKeyFiles(
        net::GetTestCertsDirectory(), "client_1.pem", "client_1.pk8");
    ASSERT_TRUE(client_cert_);

    observer_ = CreateObserver();
  }

  std::unique_ptr<SystemClientCertNetworkObserver> CreateObserver() {
    return std::make_unique<SystemClientCertNetworkObserver>(
        session_manager::SessionManager::Get(),
        user_manager::UserManager::Get(), policy_service_.get(),
        policy_cert_provider_.get(),
        base::BindRepeating(&SystemClientCertNetworkObserverTest::OnCacheFlush,
                            base::Unretained(this)),
        base::DoNothing(), FakeStoreFactory());
  }

  SystemClientCertNetworkObserver::ClientCertStoreFactory FakeStoreFactory() {
    return base::BindRepeating(
        &SystemClientCertNetworkObserverTest::CreateFakeStore,
        base::Unretained(this));
  }

  void TearDown() override {
    observer_.reset();
    custom_store_factory_.Reset();
    policy_cert_provider_.reset();
    policy_service_.reset();
    policy_provider_.Shutdown();
    session_env_.reset();
  }

  void SetPolicyRules(std::vector<std::string> rules) {
    base::ListValue list;
    for (auto& rule : rules) {
      list.Append(std::move(rule));
    }
    policy::PolicyMap policies;
    policies.Set(policy::key::kDeviceLoginScreenAutoSelectCertificateForUrls,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD, base::Value(std::move(list)),
                 nullptr);
    policy_provider_.UpdateChromePolicy(policies);
  }

  scoped_refptr<net::SSLCertRequestInfo> CreateCertRequest(
      const std::string& host = kProxyHost,
      uint16_t port = kProxyPort) {
    auto cert_info = base::MakeRefCounted<net::SSLCertRequestInfo>();
    cert_info->host_and_port = net::HostPortPair(host, port);
    return cert_info;
  }

  TestClientCertificateResponder::Outcome RequestCert(
      scoped_refptr<net::X509Certificate>* out_cert = nullptr,
      const std::string& host = kProxyHost) {
    TestClientCertificateResponder responder;
    observer_->OnCertificateRequested(
        /*window_id=*/std::nullopt, CreateCertRequest(host), responder.Bind());
    auto outcome = responder.WaitForOutcome();
    if (out_cert) {
      *out_cert = responder.cert();
    }
    return outcome;
  }

  std::unique_ptr<net::ClientCertStore> CreateFakeStore(
      net::CertificateList device_onc_certs) {
    last_device_onc_certs_ = device_onc_certs;
    if (custom_store_factory_) {
      return custom_store_factory_.Run(std::move(device_onc_certs));
    }
    net::ClientCertIdentityList certs;
    if (client_cert_) {
      certs.push_back(client_cert_->Copy());
    }
    return std::make_unique<FakeClientCertStore>(std::move(certs));
  }

  void OnCacheFlush() { ++flush_count_; }

  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment> session_env_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  std::unique_ptr<policy::PolicyServiceImpl> policy_service_;
  std::unique_ptr<FakePolicyCertificateProvider> policy_cert_provider_;
  std::unique_ptr<net::FakeClientCertIdentity> client_cert_;
  net::CertificateList last_device_onc_certs_;
  SystemClientCertNetworkObserver::ClientCertStoreFactory custom_store_factory_;
  std::unique_ptr<SystemClientCertNetworkObserver> observer_;
  int flush_count_ = 0;
};

TEST_F(SystemClientCertNetworkObserverTest, CancelsWhenPolicyUnsetOrEmpty) {
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kCancelRequest,
            RequestCert());

  SetPolicyRules({});
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kCancelRequest,
            RequestCert());

  // Setting a matching rule and then clearing the policy map cancels again.
  SetPolicyRules({kMatchingRule});
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kContinueWithCertificate,
            RequestCert());
  policy_provider_.UpdateChromePolicy(policy::PolicyMap());
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kCancelRequest,
            RequestCert());
}

TEST_F(SystemClientCertNetworkObserverTest,
       SelectsMatchingCertOnLoginScreenAndCancelsOnMismatch) {
  SetPolicyRules({kMatchingRule});
  EXPECT_EQ(1, flush_count_);

  scoped_refptr<net::X509Certificate> selected_cert;
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kContinueWithCertificate,
            RequestCert(&selected_cert));
  ASSERT_TRUE(selected_cert);
  EXPECT_TRUE(selected_cert->EqualsExcludingChain(client_cert_->certificate()));

  // Non-matching host cancels the certificate request.
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kCancelRequest,
            RequestCert(/*out_cert=*/nullptr, "unmatched.example.com"));

  // Matching host with a non-matching certificate filter
  // (`matching_certificates` empty in `OnClientCertsRetrieved()`) also cancels
  // the request.
  SetPolicyRules({kNonMatchingFilterRule});
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kCancelRequest,
            RequestCert());
}

TEST_F(SystemClientCertNetworkObserverTest, CancelsWhenClientCertStoreIsEmpty) {
  SetPolicyRules({kMatchingRule});
  client_cert_.reset();

  EXPECT_EQ(TestClientCertificateResponder::Outcome::kCancelRequest,
            RequestCert());
}

TEST_F(SystemClientCertNetworkObserverTest,
       SelectsFirstMatchingCertWhenMultipleMatch) {
  std::unique_ptr<net::FakeClientCertIdentity> client_cert_2 =
      net::FakeClientCertIdentity::CreateFromCertAndKeyFiles(
          net::GetTestCertsDirectory(), "client_2.pem", "client_2.pk8");
  ASSERT_TRUE(client_cert_2);

  custom_store_factory_ = base::BindLambdaForTesting(
      [&](net::CertificateList) -> std::unique_ptr<net::ClientCertStore> {
        net::ClientCertIdentityList certs;
        certs.push_back(client_cert_2->Copy());
        certs.push_back(client_cert_->Copy());
        return std::make_unique<FakeClientCertStore>(std::move(certs));
      });

  SetPolicyRules({R"({"pattern":"https://proxy.example.com","filter":{}})"});

  scoped_refptr<net::X509Certificate> selected_cert;
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kContinueWithCertificate,
            RequestCert(&selected_cert));
  ASSERT_TRUE(selected_cert);
  EXPECT_TRUE(
      selected_cert->EqualsExcludingChain(client_cert_2->certificate()));
}

TEST_F(SystemClientCertNetworkObserverTest,
       BlocksAndFlushesOnUnaffiliatedSessionsAndAllowsOnAffiliationUpdate) {
  SetPolicyRules({kMatchingRule});
  flush_count_ = 0;

  const AccountId account_id =
      AccountId::FromUserEmailGaiaId("user@example.com", GaiaId("12345"));
  const AccountId secondary_id =
      AccountId::FromUserEmailGaiaId("other@gmail.com", GaiaId("67890"));
  ASSERT_TRUE(session_env_->AddRegularUser(account_id));
  ASSERT_TRUE(session_env_->AddRegularUser(secondary_id));

  // 1. User logs in with `is_affiliated == false` (mirroring first-time or
  // ephemeral login), while profile is not yet created (initial policy fetch
  // window): client cert selection remains allowed.
  session_env_->LogIn(account_id);
  user_manager::UserManager::Get()->SetUserPolicyStatus(
      account_id, /*is_managed=*/false, /*is_affiliated=*/false);
  EXPECT_EQ(0, flush_count_);
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kContinueWithCertificate,
            RequestCert());

  // 2. Profile creation finishes for an unaffiliated user: cache is flushed and
  // subsequent requests are cancelled.
  user_manager::UserManager::Get()->OnUserProfileCreated(account_id,
                                                         /*prefs=*/nullptr);
  EXPECT_EQ(1, flush_count_);
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kCancelRequest,
            RequestCert());

  // 3. Affiliation is updated to true: cache is flushed and requests are
  // allowed again.
  user_manager::UserManager::Get()->SetUserPolicyStatus(
      account_id, /*is_managed=*/true, /*is_affiliated=*/true);
  EXPECT_EQ(2, flush_count_);
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kContinueWithCertificate,
            RequestCert());

  // 4. Adding a secondary unaffiliated user flushes cache and blocks requests
  // once their profile is created.
  session_env_->LogIn(secondary_id);
  user_manager::UserManager::Get()->SetUserPolicyStatus(
      secondary_id, /*is_managed=*/false, /*is_affiliated=*/false);
  user_manager::UserManager::Get()->OnUserProfileCreated(secondary_id,
                                                         /*prefs=*/nullptr);
  EXPECT_EQ(3, flush_count_);
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kCancelRequest,
            RequestCert());
}

TEST_F(SystemClientCertNetworkObserverTest,
       AllowsUnknownAffiliationAfterProfileCreatedUntilDeterminedFalse) {
  SetPolicyRules({kMatchingRule});
  flush_count_ = 0;

  const AccountId account_id =
      AccountId::FromUserEmailGaiaId("user@example.com", GaiaId("12345"));
  user_manager::User* user = session_env_->AddRegularUser(account_id);
  ASSERT_TRUE(user);
  ASSERT_EQ(std::nullopt, user->is_affiliated());

  session_env_->LogIn(account_id);
  user_manager::UserManager::Get()->OnUserProfileCreated(account_id,
                                                         /*prefs=*/nullptr);
  ASSERT_TRUE(user->is_profile_created());
  ASSERT_EQ(std::nullopt, user->is_affiliated());

  // While affiliation is still undetermined (`std::nullopt`), client
  // certificates remain allowed.
  EXPECT_EQ(0, flush_count_);
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kContinueWithCertificate,
            RequestCert());

  // Once affiliation is determined to be `false`, the cache is flushed and
  // subsequent requests are cancelled.
  user_manager::UserManager::Get()->SetUserPolicyStatus(
      account_id, /*is_managed=*/false, /*is_affiliated=*/false);
  EXPECT_EQ(1, flush_count_);
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kCancelRequest,
            RequestCert());
}

TEST_F(SystemClientCertNetworkObserverTest, GuestLoginBlocksImmediately) {
  SetPolicyRules({kMatchingRule});
  flush_count_ = 0;

  user_manager::User* guest = session_env_->AddGuestUser();
  ASSERT_TRUE(guest);
  session_env_->LogIn(guest->GetAccountId());
  EXPECT_EQ(1, flush_count_);
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kCancelRequest,
            RequestCert());
}

TEST_F(SystemClientCertNetworkObserverTest,
       AllowsManagedGuestSessionWithoutFlushing) {
  SetPolicyRules({kMatchingRule});
  flush_count_ = 0;

  user_manager::User* user = session_env_->AddPublicAccountUser(
      "mgs@public-accounts.device-local.localhost");
  ASSERT_TRUE(user);
  EXPECT_EQ(true, user->is_affiliated());

  session_env_->LogIn(user->GetAccountId());
  user_manager::UserManager::Get()->OnUserProfileCreated(user->GetAccountId(),
                                                         /*prefs=*/nullptr);
  ASSERT_TRUE(user->is_profile_created());
  EXPECT_EQ(true, user->is_affiliated());
  EXPECT_EQ(0, flush_count_);
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kContinueWithCertificate,
            RequestCert());
}

TEST_F(SystemClientCertNetworkObserverTest,
       AllowsKioskWebAppSessionWithoutFlushing) {
  SetPolicyRules({kMatchingRule});
  flush_count_ = 0;

  user_manager::User* user = session_env_->AddKioskWebAppUser(
      "kiosk@web-kiosk-apps.device-local.localhost");
  ASSERT_TRUE(user);
  EXPECT_EQ(true, user->is_affiliated());

  session_env_->LogIn(user->GetAccountId());
  user_manager::UserManager::Get()->OnUserProfileCreated(user->GetAccountId(),
                                                         /*prefs=*/nullptr);
  ASSERT_TRUE(user->is_profile_created());
  EXPECT_EQ(true, user->is_affiliated());
  EXPECT_EQ(0, flush_count_);
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kContinueWithCertificate,
            RequestCert());
}

TEST_F(SystemClientCertNetworkObserverTest,
       InFlightAcquirePrivateKeyAbortedOnSessionOrPolicyChange) {
  SetPolicyRules({kMatchingRule});

  // Case 1: Pause at GetClientCerts, then update policy rules -> cancels
  // without poisoning SSLClientAuthCache.
  net::ClientCertStore::ClientCertListCallback pending_certs_cb;
  custom_store_factory_ = base::BindLambdaForTesting(
      [&](net::CertificateList) -> std::unique_ptr<net::ClientCertStore> {
        return std::make_unique<DelayedClientCertStore>(
            base::BindLambdaForTesting(
                [&](net::ClientCertStore::ClientCertListCallback cb) {
                  pending_certs_cb = std::move(cb);
                }));
      });
  TestClientCertificateResponder responder0;
  observer_->OnCertificateRequested(std::nullopt, CreateCertRequest(),
                                    responder0.Bind());
  ASSERT_TRUE(pending_certs_cb);
  SetPolicyRules({kOtherHostRule});
  net::ClientCertIdentityList certs0;
  certs0.push_back(client_cert_->Copy());
  std::move(pending_certs_cb).Run(std::move(certs0));
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kCancelRequest,
            responder0.WaitForOutcome());

  SetPolicyRules({kMatchingRule});

  DelayedClientCertIdentity::KeyCallback pending_key_cb;
  scoped_refptr<net::SSLPrivateKey> pending_key;
  custom_store_factory_ = base::BindLambdaForTesting(
      [&](net::CertificateList) -> std::unique_ptr<net::ClientCertStore> {
        net::ClientCertIdentityList certs;
        certs.push_back(std::make_unique<DelayedClientCertIdentity>(
            client_cert_->certificate(), client_cert_->ssl_private_key(),
            base::BindLambdaForTesting(
                [&](DelayedClientCertIdentity::KeyCallback cb,
                    scoped_refptr<net::SSLPrivateKey> key) {
                  pending_key_cb = std::move(cb);
                  pending_key = std::move(key);
                })));
        return std::make_unique<FakeClientCertStore>(std::move(certs));
      });

  // Case 2: Pause at AcquirePrivateKey, then update policy rules -> cancels
  // without poisoning SSLClientAuthCache.
  TestClientCertificateResponder responder1;
  observer_->OnCertificateRequested(std::nullopt, CreateCertRequest(),
                                    responder1.Bind());
  ASSERT_TRUE(pending_key_cb);
  SetPolicyRules({kOtherHostRule});
  std::move(pending_key_cb).Run(std::move(pending_key));
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kCancelRequest,
            responder1.WaitForOutcome());

  // Case 3: Restore matching rule, pause at AcquirePrivateKey, then log in as
  // Guest (disallowed session state) -> cancels.
  SetPolicyRules({kMatchingRule});
  TestClientCertificateResponder responder2;
  observer_->OnCertificateRequested(std::nullopt, CreateCertRequest(),
                                    responder2.Bind());
  ASSERT_TRUE(pending_key_cb);
  user_manager::User* guest = session_env_->AddGuestUser();
  ASSERT_TRUE(guest);
  session_env_->LogIn(guest->GetAccountId());
  std::move(pending_key_cb).Run(std::move(pending_key));
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kCancelRequest,
            responder2.WaitForOutcome());
}

TEST_F(SystemClientCertNetworkObserverTest,
       PrivateKeyWrapperValidatesSignatureAlgorithm) {
  SetPolicyRules({kMatchingRule});

  TestClientCertificateResponder responder;
  observer_->OnCertificateRequested(std::nullopt, CreateCertRequest(),
                                    responder.Bind());
  ASSERT_EQ(TestClientCertificateResponder::Outcome::kContinueWithCertificate,
            responder.WaitForOutcome());
  ASSERT_FALSE(responder.algorithm_preferences().empty());
  ASSERT_TRUE(responder.ssl_private_key().is_bound());

  const std::vector<uint8_t> payload = {1, 2, 3, 4};

  // 1. Signing with an advertised algorithm succeeds.
  {
    base::test::TestFuture<int32_t, const std::vector<uint8_t>&> sign_future;
    responder.ssl_private_key()->Sign(responder.algorithm_preferences().front(),
                                      payload, sign_future.GetCallback());
    EXPECT_EQ(net::OK, sign_future.Get<0>());
    EXPECT_FALSE(sign_future.Get<1>().empty());
  }

  // 2. Signing with an out-of-range `uint16_t` algorithm fails cleanly without
  // forwarding to `net::SSLPrivateKey::Sign()`.
  {
    base::test::TestFuture<int32_t, const std::vector<uint8_t>&> sign_future;
    responder.ssl_private_key()->Sign(0xffff, payload,
                                      sign_future.GetCallback());
    EXPECT_EQ(net::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED, sign_future.Get<0>());
    EXPECT_TRUE(sign_future.Get<1>().empty());
  }

  // 3. Signing with a valid TLS algorithm not supported by this RSA key also
  // fails cleanly.
  ASSERT_FALSE(std::ranges::contains(responder.algorithm_preferences(),
                                     SSL_SIGN_ECDSA_SECP256R1_SHA256));
  {
    base::test::TestFuture<int32_t, const std::vector<uint8_t>&> sign_future;
    responder.ssl_private_key()->Sign(SSL_SIGN_ECDSA_SECP256R1_SHA256, payload,
                                      sign_future.GetCallback());
    EXPECT_EQ(net::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED, sign_future.Get<0>());
    EXPECT_TRUE(sign_future.Get<1>().empty());
  }
}

TEST_F(SystemClientCertNetworkObserverTest,
       FlushDisconnectsBoundPrivateKeyReceivers) {
  SetPolicyRules({kMatchingRule});

  auto obtain_connected_responder =
      [&](TestClientCertificateResponder& responder) {
        observer_->OnCertificateRequested(std::nullopt, CreateCertRequest(),
                                          responder.Bind());
        ASSERT_EQ(
            TestClientCertificateResponder::Outcome::kContinueWithCertificate,
            responder.WaitForOutcome());
        responder.ssl_private_key().FlushForTesting();
        ASSERT_TRUE(responder.ssl_private_key().is_connected());
      };

  // 1. Policy change disconnects bound `SSLPrivateKey` pipes.
  TestClientCertificateResponder responder1;
  obtain_connected_responder(responder1);
  SetPolicyRules({kOtherHostRule});
  responder1.ssl_private_key().FlushForTesting();
  EXPECT_FALSE(responder1.ssl_private_key().is_connected());

  // 2. Policy-provided certificate update disconnects bound `SSLPrivateKey`
  // pipes.
  SetPolicyRules({kMatchingRule});
  TestClientCertificateResponder responder2;
  obtain_connected_responder(responder2);
  policy_cert_provider_->NotifyCertsChanged();
  responder2.ssl_private_key().FlushForTesting();
  EXPECT_FALSE(responder2.ssl_private_key().is_connected());

  // 3. Disallowed session transition (Guest login) disconnects bound
  // `SSLPrivateKey` pipes.
  TestClientCertificateResponder responder3;
  obtain_connected_responder(responder3);
  user_manager::User* guest = session_env_->AddGuestUser();
  ASSERT_TRUE(guest);
  session_env_->LogIn(guest->GetAccountId());
  responder3.ssl_private_key().FlushForTesting();
  EXPECT_FALSE(responder3.ssl_private_key().is_connected());
}

TEST_F(SystemClientCertNetworkObserverTest,
       PolicyProvidedCertsChangeFlushesAbortsInFlightAndHandlesDestruction) {
  SetPolicyRules({kMatchingRule});
  flush_count_ = 0;

  scoped_refptr<net::X509Certificate> onc_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(onc_cert);
  policy_cert_provider_->SetServerAndAuthorityCertificates({onc_cert});

  // 1. `device_onc_certs` from `PolicyCertificateProvider` are forwarded to the
  // `ClientCertStoreFactory`.
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kContinueWithCertificate,
            RequestCert());
  ASSERT_EQ(1u, last_device_onc_certs_.size());
  EXPECT_TRUE(last_device_onc_certs_[0]->EqualsExcludingChain(onc_cert.get()));

  // 2. `OnPolicyProvidedCertsChanged()` flushes the client cert cache and
  // cancels in-flight certificate lookups started under the previous cert list.
  net::ClientCertStore::ClientCertListCallback pending_certs_cb;
  custom_store_factory_ = base::BindLambdaForTesting(
      [&](net::CertificateList) -> std::unique_ptr<net::ClientCertStore> {
        return std::make_unique<DelayedClientCertStore>(
            base::BindLambdaForTesting(
                [&](net::ClientCertStore::ClientCertListCallback cb) {
                  pending_certs_cb = std::move(cb);
                }));
      });
  TestClientCertificateResponder responder;
  observer_->OnCertificateRequested(std::nullopt, CreateCertRequest(),
                                    responder.Bind());
  ASSERT_TRUE(pending_certs_cb);

  policy_cert_provider_->NotifyCertsChanged();
  EXPECT_EQ(1, flush_count_);

  net::ClientCertIdentityList certs;
  certs.push_back(client_cert_->Copy());
  std::move(pending_certs_cb).Run(std::move(certs));
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kCancelRequest,
            responder.WaitForOutcome());

  // 3. Mirror production teardown where `DeviceNetworkConfigurationUpdaterAsh`
  // is destroyed during `BrowserPolicyConnectorAsh::Shutdown()` before
  // `SystemClientCertNetworkObserver` is destroyed in
  // `PostMainMessageLoopRun()`.
  auto local_provider = std::make_unique<FakePolicyCertificateProvider>();
  local_provider->SetServerAndAuthorityCertificates({onc_cert});
  net::CertificateList captured_certs;
  SystemClientCertNetworkObserver local_observer(
      session_manager::SessionManager::Get(), user_manager::UserManager::Get(),
      policy_service_.get(), local_provider.get(), base::DoNothing(),
      base::DoNothing(),
      base::BindLambdaForTesting([&](net::CertificateList certs_from_provider)
                                     -> std::unique_ptr<net::ClientCertStore> {
        captured_certs = std::move(certs_from_provider);
        net::ClientCertIdentityList identities;
        identities.push_back(client_cert_->Copy());
        return std::make_unique<FakeClientCertStore>(std::move(identities));
      }));

  local_provider.reset();

  TestClientCertificateResponder post_teardown_responder;
  local_observer.OnCertificateRequested(std::nullopt, CreateCertRequest(),
                                        post_teardown_responder.Bind());
  EXPECT_EQ(TestClientCertificateResponder::Outcome::kContinueWithCertificate,
            post_teardown_responder.WaitForOutcome());
  EXPECT_TRUE(captured_certs.empty());
}

TEST_F(SystemClientCertNetworkObserverTest,
       NonCertificateObserverMethodsReturnSafeDefaults) {
  base::test::TestFuture<int> ssl_error_future;
  observer_->OnSSLCertificateError(
      GURL("https://example.com"), net::ERR_CERT_AUTHORITY_INVALID,
      net::SSLInfo(), /*fatal=*/true, ssl_error_future.GetCallback());
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID, ssl_error_future.Get());

  TestAuthChallengeResponder auth_responder;
  observer_->OnAuthRequired(
      std::nullopt, /*request_id=*/1, GURL("https://example.com"),
      /*first_auth_attempt=*/true,
      /*do_not_prompt_for_login=*/false, net::AuthChallengeInfo(),
      /*head_headers=*/nullptr, auth_responder.Bind());
  EXPECT_EQ(std::nullopt, auth_responder.WaitForCredentials());

  base::test::TestFuture<network::mojom::LocalNetworkAccessResult> lna_future;
  observer_->OnLocalNetworkAccessPermissionRequired(
      network::mojom::TransportType::kDirect,
      network::mojom::IPAddressSpace::kLocal, lna_future.GetCallback());
  EXPECT_EQ(network::mojom::LocalNetworkAccessResult::kDenied,
            lna_future.Get());

  base::test::TestFuture<bool> platform_lna_future;
  observer_->OnPlatformLocalNetworkPermissionRequired(
      platform_lna_future.GetCallback());
  EXPECT_FALSE(platform_lna_future.Get());

  base::test::TestFuture<void> clear_site_data_future;
  observer_->OnClearSiteData(GURL("https://example.com"), std::nullopt,
                             std::nullopt, "\"cookies\"", /*load_flags=*/0,
                             std::nullopt,
                             /*partitioned_state_allowed_only=*/false,
                             clear_site_data_future.GetCallback());
  EXPECT_TRUE(clear_site_data_future.Wait());

  base::test::TestFuture<void> loading_state_future;
  observer_->OnLoadingStateUpdate(network::mojom::LoadInfo::New(),
                                  loading_state_future.GetCallback());
  EXPECT_TRUE(loading_state_future.Wait());

  observer_->OnDataUseUpdate(/*network_traffic_annotation_id_hash=*/42,
                             base::ByteSize(100), base::ByteSize(200));

  base::test::TestFuture<int32_t, base::ByteSize, base::ByteSize>
      data_use_future;
  SystemClientCertNetworkObserver observer_with_data_use_cb(
      session_manager::SessionManager::Get(), user_manager::UserManager::Get(),
      policy_service_.get(), policy_cert_provider_.get(), base::DoNothing(),
      data_use_future.GetRepeatingCallback(), FakeStoreFactory());
  observer_with_data_use_cb.OnDataUseUpdate(
      /*network_traffic_annotation_id_hash=*/123, base::ByteSize(456),
      base::ByteSize(789));
  EXPECT_EQ(std::make_tuple(123, base::ByteSize(456), base::ByteSize(789)),
            data_use_future.Get());
}

}  // namespace
}  // namespace ash

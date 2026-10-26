// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/certificate_signals_collector_delegate_impl.h"

#include <memory>

#include "base/test/bind.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/client_cert_store_empty.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_signals {

class CertificateSignalsCollectorDelegateImplTest : public testing::Test {
 protected:
  // Creates a profile which has a ProfileNetworkContextService.
  std::unique_ptr<TestingProfile> CreateProfileWithNetworkContextService() {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        ProfileNetworkContextServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<ProfileNetworkContextService>(
              Profile::FromBrowserContext(context));
        }));
    return builder.Build();
  }

  content::BrowserTaskEnvironment task_environment_;
};

// Tests that the delegate returns the profile's client certificate store.
TEST_F(CertificateSignalsCollectorDelegateImplTest,
       CreateClientCertStore_ReturnsProfileStore) {
  std::unique_ptr<TestingProfile> profile =
      CreateProfileWithNetworkContextService();
  auto* profile_network_service =
      ProfileNetworkContextServiceFactory::GetForContext(profile.get());
  ASSERT_TRUE(profile_network_service);

  int create_call_count = 0;
  net::ClientCertStore* created_store = nullptr;
  profile_network_service->set_client_cert_store_factory_for_testing(
      base::BindLambdaForTesting(
          [&]() -> std::unique_ptr<net::ClientCertStore> {
            ++create_call_count;
            auto store = std::make_unique<net::ClientCertStoreEmpty>();
            created_store = store.get();
            return store;
          }));

  CertificateSignalsCollectorDelegateImpl delegate(profile.get());
  // Creating the delegate does not create the store.
  EXPECT_EQ(create_call_count, 0);

  std::unique_ptr<net::ClientCertStore> store =
      delegate.CreateClientCertStore();
  EXPECT_EQ(create_call_count, 1);
  ASSERT_TRUE(store);
  EXPECT_EQ(store.get(), created_store);
}

// Tests that the delegate returns nullptr when the profile has no
// ProfileNetworkContextService.
TEST_F(CertificateSignalsCollectorDelegateImplTest,
       CreateClientCertStore_NoProfileNetworkContextService) {
  TestingProfile profile;
  ASSERT_FALSE(ProfileNetworkContextServiceFactory::GetForContext(&profile));

  CertificateSignalsCollectorDelegateImpl delegate(&profile);
  EXPECT_FALSE(delegate.CreateClientCertStore());
}

}  // namespace enterprise_signals

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/cookie_config/cookie_store_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_util.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace content {
namespace {

constexpr char kCookieName[] = "Name";
constexpr char kCookieValue[] = "Value";

net::CookieList GetCookies(
    const mojo::Remote<network::mojom::CookieManager>& cookie_manager) {
  base::RunLoop run_loop;
  net::CookieList cookies_out;
  cookie_manager->GetAllCookies(
      base::BindLambdaForTesting([&](const net::CookieList& cookies) {
        cookies_out = cookies;
        run_loop.Quit();
      }));
  run_loop.Run();
  return cookies_out;
}

void SetCookie(
    const mojo::Remote<network::mojom::CookieManager>& cookie_manager) {
  base::Time t = base::Time::Now();
  auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      kCookieName, kCookieValue, "www.test.com", "/", t,
      t + base::TimeDelta::FromDays(1), base::Time(), true /* secure */,
      false /* http-only*/, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_DEFAULT, false /* same_party */);
  base::RunLoop run_loop;
  cookie_manager->SetCanonicalCookie(
      *cookie, net::cookie_util::SimulatedCookieSource(*cookie, "https"),
      net::CookieOptions(),
      base::BindLambdaForTesting(
          [&](net::CookieAccessResult result) { run_loop.Quit(); }));
  run_loop.Run();
}

void FlushCookies(
    const mojo::Remote<network::mojom::CookieManager>& cookie_manager) {
  base::RunLoop run_loop;
  cookie_manager->FlushCookieStore(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
  run_loop.Run();
}

// See |NetworkServiceBrowserTest| for content's version of tests.
class ChromeNetworkServiceBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface</*kNetworkServiceInProcess*/ bool> {
 public:
  ChromeNetworkServiceBrowserTest() {
    bool in_process = GetParam();
    // Verify that cookie encryption works both in-process and out of process.
    if (in_process)
      scoped_feature_list_.InitAndEnableFeature(
          features::kNetworkServiceInProcess);
  }

 protected:
  mojo::PendingRemote<network::mojom::NetworkContext> CreateNetworkContext(
      bool enable_encrypted_cookies) {
    mojo::PendingRemote<network::mojom::NetworkContext> network_context;
    network::mojom::NetworkContextParamsPtr context_params =
        network::mojom::NetworkContextParams::New();
    context_params->enable_encrypted_cookies = enable_encrypted_cookies;
    context_params->cookie_path =
        browser()->profile()->GetPath().Append(FILE_PATH_LITERAL("cookies"));
    context_params->cert_verifier_params = content::GetCertVerifierParams(
        cert_verifier::mojom::CertVerifierCreationParams::New());
    GetNetworkService()->CreateNetworkContext(
        network_context.InitWithNewPipeAndPassReceiver(),
        std::move(context_params));
    return network_context;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNetworkServiceBrowserTest);
};

IN_PROC_BROWSER_TEST_P(ChromeNetworkServiceBrowserTest, PRE_EncryptedCookies) {
  // First set a cookie with cookie encryption enabled.
  mojo::Remote<network::mojom::NetworkContext> context(
      CreateNetworkContext(/*enable_encrypted_cookies=*/true));
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  context->GetCookieManager(cookie_manager.BindNewPipeAndPassReceiver());

  SetCookie(cookie_manager);

  net::CookieList cookies = GetCookies(cookie_manager);
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ(kCookieName, cookies[0].Name());
  EXPECT_EQ(kCookieValue, cookies[0].Value());

  FlushCookies(cookie_manager);
}

// This flakes on Mac10.12 and Windows: http://crbug.com/868667
#if defined(OS_MAC) || defined(OS_WIN)
#define MAYBE_EncryptedCookies DISABLED_EncryptedCookies
#else
#define MAYBE_EncryptedCookies EncryptedCookies
#endif
IN_PROC_BROWSER_TEST_P(ChromeNetworkServiceBrowserTest,
                       MAYBE_EncryptedCookies) {
  net::CookieCryptoDelegate* crypto_delegate =
      cookie_config::GetCookieCryptoDelegate();
  std::string ciphertext;
  crypto_delegate->EncryptString(kCookieValue, &ciphertext);
  // These checks are only valid if crypto is enabled on the platform.
  if (!crypto_delegate->ShouldEncrypt() || ciphertext == kCookieValue)
    return;

  // Now attempt to read the cookie with encryption disabled.
  mojo::Remote<network::mojom::NetworkContext> context(
      CreateNetworkContext(/*enable_encrypted_cookies=*/false));
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  context->GetCookieManager(cookie_manager.BindNewPipeAndPassReceiver());

  net::CookieList cookies = GetCookies(cookie_manager);
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ(kCookieName, cookies[0].Name());
  EXPECT_EQ("", cookies[0].Value());
}

INSTANTIATE_TEST_SUITE_P(InProcess,
                         ChromeNetworkServiceBrowserTest,
                         ::testing::Values(true));

INSTANTIATE_TEST_SUITE_P(OutOfProcess,
                         ChromeNetworkServiceBrowserTest,
                         ::testing::Values(false));

}  // namespace
}  // namespace content

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_cookie_listener.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PrefetchProxyCookieListenerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    profile()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->GetCookieManager(cookie_manager_.BindNewPipeAndPassReceiver());
  }

  // Creates a |PrefetchProxyCookieListener| for the given url and
  // |cookie_manager_|. Confirms that it is non-null.
  std::unique_ptr<PrefetchProxyCookieListener> MakeCookieListener(
      const GURL& url) {
    std::unique_ptr<PrefetchProxyCookieListener> cookie_listener =
        PrefetchProxyCookieListener::MakeAndRegister(
            url,
            base::BindOnce(&PrefetchProxyCookieListenerTest::OnCookiesChanged,
                           base::Unretained(this)),
            cookie_manager_.get());
    EXPECT_TRUE(cookie_listener);
    return cookie_listener;
  }

  // Creates a host cookie for the given url, and then adds it to the default
  // partition using |cookie_manager_|.
  bool SetHostCookie(const GURL& url, const std::string& value) {
    std::unique_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
        url, value, base::Time::Now(), /*server_time=*/absl::nullopt,
        /*cookie_partition_key=*/absl::nullopt));
    EXPECT_TRUE(cookie.get());
    EXPECT_TRUE(cookie->IsHostCookie());

    return SetCookie(*cookie.get(), url);
  }

  // Creates a domain cookie for the given url, and then adds it to the default
  // partition using |cookie_manager_|.
  bool SetDomainCookie(const GURL& url,
                       const std::string& name,
                       const std::string& value,
                       const std::string& domain) {
    net::CookieInclusionStatus status;
    std::unique_ptr<net::CanonicalCookie> cookie(
        net::CanonicalCookie::CreateSanitizedCookie(
            url, name, value, domain, /*path=*/"", base::Time::Now(),
            base::Time::Now() + base::Hours(1), base::Time::Now(),
            /*secure=*/true, /*http_only=*/false,
            net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT,
            /*same_party=*/false, /*partition_key=*/absl::nullopt, &status));
    EXPECT_TRUE(cookie.get());
    EXPECT_TRUE(cookie->IsDomainCookie());
    EXPECT_TRUE(status.IsInclude());

    return SetCookie(*cookie.get(), url);
  }

  void SetExpectedURL(const GURL& url) { expected_url_ = url; }

  void SetOnCookiesChangedClosure(base::OnceClosure closure) {
    on_cookies_changed_closure_ = std::move(closure);
  }

 private:
  bool SetCookie(const net::CanonicalCookie& cookie, const GURL& url) {
    bool result = false;
    base::RunLoop run_loop;

    net::CookieOptions options;
    options.set_include_httponly();
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::MakeInclusive());

    cookie_manager_->SetCanonicalCookie(
        cookie, url, options,
        base::BindOnce(
            [](bool* result, base::RunLoop* run_loop,
               net::CookieAccessResult set_cookie_access_result) {
              *result = set_cookie_access_result.status.IsInclude();
              run_loop->Quit();
            },
            &result, &run_loop));
    run_loop.Run();
    return result;
  }

  void OnCookiesChanged(const GURL& url) {
    if (url != expected_url_)
      return;

    if (!on_cookies_changed_closure_)
      return;

    std::move(on_cookies_changed_closure_).Run();
  }

  // Cookie manager for all tests
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;

  GURL expected_url_;
  base::OnceClosure on_cookies_changed_closure_;
};

// Create a cookie listener and make no changes to any cookie. Check that the
// listener knows that the cookies haven't changed.
TEST_F(PrefetchProxyCookieListenerTest, NoCookieChanges) {
  std::unique_ptr<PrefetchProxyCookieListener> cookie_listener =
      MakeCookieListener(GURL("https://www.example.com/"));

  EXPECT_FALSE(cookie_listener->HaveCookiesChanged());
}

// Creates a cookie listener and adds a host cookie for a completely different
// domain. Checks that listener doesn't catch this cookie.
TEST_F(PrefetchProxyCookieListenerTest, ChangeHostCookiesForOtherDomain) {
  std::unique_ptr<PrefetchProxyCookieListener> cookie_listener =
      MakeCookieListener(GURL("https://www.example.com/"));

  EXPECT_TRUE(SetHostCookie(GURL("https://www.other.com/"), "testing"));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(cookie_listener->HaveCookiesChanged());
}

// Creates a cookie listener and adds a host cookie for the same URL. Checks
// that listener catches this cookie.
TEST_F(PrefetchProxyCookieListenerTest, ChangeHostCookiesForSameUrl) {
  std::unique_ptr<PrefetchProxyCookieListener> cookie_listener =
      MakeCookieListener(GURL("https://www.example.com/"));

  base::RunLoop run_loop;
  SetExpectedURL(GURL("https://www.example.com/"));
  SetOnCookiesChangedClosure(run_loop.QuitClosure());

  EXPECT_TRUE(SetHostCookie(GURL("https://www.example.com/"), "testing"));
  run_loop.Run();

  EXPECT_TRUE(cookie_listener->HaveCookiesChanged());
}

// Creates a cookie listener and adds a host cookie for a subdomain. Checks
// that listener doesn't catch this cookie.
TEST_F(PrefetchProxyCookieListenerTest, ChangeHostCookiesForSubdomain) {
  std::unique_ptr<PrefetchProxyCookieListener> cookie_listener =
      MakeCookieListener(GURL("https://specificdomain.generaldomain.com/"));

  EXPECT_TRUE(SetHostCookie(GURL("https://generaldomain.com/"), "testing"));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(cookie_listener->HaveCookiesChanged());
}

// Creates a cookie listener and adds a domain cookie for a completely different
// domain. Checks that the listener doesn't catch this cookie.
TEST_F(PrefetchProxyCookieListenerTest, ChangeDomainCookiesForOtherDomain) {
  std::unique_ptr<PrefetchProxyCookieListener> cookie_listener =
      MakeCookieListener(GURL("https://www.example.com/"));

  EXPECT_TRUE(SetDomainCookie(GURL("https://www.other.com/"), "testing",
                              "testing", "other.com"));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(cookie_listener->HaveCookiesChanged());
}

// Create a cookie listener and add a domain cookie for the same URL. Check
// that the listener catches this cookie.
TEST_F(PrefetchProxyCookieListenerTest, ChangeDomainCookiesForSameUrl) {
  std::unique_ptr<PrefetchProxyCookieListener> cookie_listener =
      MakeCookieListener(GURL("https://specificdomain.generaldomain.com/"));

  base::RunLoop run_loop;
  SetExpectedURL(GURL("https://specificdomain.generaldomain.com/"));
  SetOnCookiesChangedClosure(run_loop.QuitClosure());

  EXPECT_TRUE(SetDomainCookie(GURL("https://specificdomain.generaldomain.com/"),
                              "testing", "testing",
                              "specificdomain.generaldomain.com"));
  run_loop.Run();

  EXPECT_TRUE(cookie_listener->HaveCookiesChanged());
}

// Create a cookie listener and add a domain cookie for a subdomain of its URL.
// Check that the listener catches this cookie.
TEST_F(PrefetchProxyCookieListenerTest, ChangeDomainCookiesForSubdomain) {
  std::unique_ptr<PrefetchProxyCookieListener> cookie_listener =
      MakeCookieListener(GURL("https://specificdomain.generaldomain.com/"));

  base::RunLoop run_loop;
  SetExpectedURL(GURL("https://specificdomain.generaldomain.com/"));
  SetOnCookiesChangedClosure(run_loop.QuitClosure());

  EXPECT_TRUE(SetDomainCookie(GURL("https://generaldomain.com/"), "testing",
                              "testing", "generaldomain.com"));
  run_loop.Run();

  EXPECT_TRUE(cookie_listener->HaveCookiesChanged());
}

// Create a cookie listener and add a domain cookie for a more specific domain
// than the listener's URL. Check that the listener doesn't catch this cookie.
TEST_F(PrefetchProxyCookieListenerTest,
       ChangeDomainCookiesForMoreSpecificDomain) {
  std::unique_ptr<PrefetchProxyCookieListener> cookie_listener =
      MakeCookieListener(GURL("https://specificdomain.generaldomain.com/"));

  EXPECT_TRUE(SetDomainCookie(
      GURL("https://veryspecificdomain.specificdomain.generaldomain.com/"),
      "testing", "testing",
      "veryspecificdomain.specificdomain.generaldomain.com"));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(cookie_listener->HaveCookiesChanged());
}

// Create a cookie listener, have it stop listening, then change the cookies for
// its given URL. Since the listener stopped before the cookies changed, it
// shouldn't get the change.
TEST_F(PrefetchProxyCookieListenerTest, StopListening) {
  GURL test_url("https://www.example.com/");

  std::unique_ptr<PrefetchProxyCookieListener> cookie_listener =
      MakeCookieListener(test_url);

  cookie_listener->StopListening();

  EXPECT_TRUE(SetHostCookie(test_url, "testing"));
  base::RunLoop().RunUntilIdle();

  // The listener that was stopped, will not catch the added cookie, but the
  // other listener will.
  EXPECT_FALSE(cookie_listener->HaveCookiesChanged());
}

}  // namespace

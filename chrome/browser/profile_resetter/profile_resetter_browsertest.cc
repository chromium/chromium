// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_resetter.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profile_resetter/profile_resetter_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace {

const char kCookieName[] = "A";
const char kCookieValue[] = "1";
const char kCookieHostname[] = "host1.com";

using content::BrowserThread;

// RemoveCookieTester provides the user with the ability to set and get a
// cookie for given profile.
class RemoveCookieTester {
 public:
  explicit RemoveCookieTester(Profile* profile);

  RemoveCookieTester(const RemoveCookieTester&) = delete;
  RemoveCookieTester& operator=(const RemoveCookieTester&) = delete;

  ~RemoveCookieTester();

  bool GetCookie(const std::string& host, net::CanonicalCookie* cookie);
  void AddCookie(const std::string& host,
                 const std::string& name,
                 const std::string& value);

 private:
  void GetCookieListCallback(
      const net::CookieAccessResultList& cookies,
      const net::CookieAccessResultList& excluded_cookies);
  void SetCanonicalCookieCallback(net::CookieAccessResult result);

  void BlockUntilNotified();
  void Notify();

  std::vector<net::CanonicalCookie> last_cookies_;
  bool waiting_callback_;
  raw_ptr<Profile> profile_;
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
  scoped_refptr<content::MessageLoopRunner> runner_;
};

RemoveCookieTester::RemoveCookieTester(Profile* profile)
    : waiting_callback_(false),
      profile_(profile) {
  network::mojom::NetworkContext* network_context =
      profile_->GetDefaultStoragePartition()->GetNetworkContext();
  network_context->GetCookieManager(
      cookie_manager_.BindNewPipeAndPassReceiver());
}

RemoveCookieTester::~RemoveCookieTester() {}

// Returns true and sets |*cookie| if the given cookie exists in
// the cookie store.
bool RemoveCookieTester::GetCookie(const std::string& host,
                                   net::CanonicalCookie* cookie) {
  last_cookies_.clear();
  DCHECK(!waiting_callback_);
  waiting_callback_ = true;
  net::CookieOptions cookie_options;
  cookie_manager_->GetCookieList(
      GURL("https://" + host + "/"), cookie_options,
      net::CookiePartitionKeyCollection(),
      base::BindOnce(&RemoveCookieTester::GetCookieListCallback,
                     base::Unretained(this)));
  BlockUntilNotified();
  DCHECK_GE(1u, last_cookies_.size());
  if (last_cookies_.empty())
    return false;
  *cookie = last_cookies_[0];
  return true;
}

void RemoveCookieTester::AddCookie(const std::string& host,
                                   const std::string& name,
                                   const std::string& value) {
  DCHECK(!waiting_callback_);
  waiting_callback_ = true;
  net::CookieOptions options;
  options.set_include_httponly();
  auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      name, value, host, "/", base::Time(), base::Time(), base::Time(),
      base::Time(), /*secure=*/true, /*httponly=*/false,
      net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM);
  cookie_manager_->SetCanonicalCookie(
      *cookie, net::cookie_util::SimulatedCookieSource(*cookie, "https"),
      options,
      base::BindOnce(&RemoveCookieTester::SetCanonicalCookieCallback,
                     base::Unretained(this)));
  BlockUntilNotified();
}

void RemoveCookieTester::GetCookieListCallback(
    const net::CookieAccessResultList& cookies,
    const net::CookieAccessResultList& excluded_cookies) {
  last_cookies_ = net::cookie_util::StripAccessResults(cookies);
  Notify();
}

void RemoveCookieTester::SetCanonicalCookieCallback(
    net::CookieAccessResult result) {
  ASSERT_TRUE(result.status.IsInclude());
  Notify();
}

void RemoveCookieTester::BlockUntilNotified() {
  DCHECK(!runner_.get());
  if (waiting_callback_) {
    runner_ = new content::MessageLoopRunner;
    runner_->Run();
    runner_.reset();
  }
}

void RemoveCookieTester::Notify() {
  DCHECK(waiting_callback_);
  waiting_callback_ = false;
  if (runner_.get())
    runner_->Quit();
}

class ProfileResetTest : public InProcessBrowserTest,
                         public ProfileResetterTestBase {
 protected:
  void SetUpOnMainThread() override {
    resetter_ = std::make_unique<ProfileResetter>(browser()->profile());
  }
};


IN_PROC_BROWSER_TEST_F(ProfileResetTest, ResetCookiesAndSiteData) {
  RemoveCookieTester tester(browser()->profile());
  tester.AddCookie(kCookieHostname, kCookieName, kCookieValue);
  net::CanonicalCookie cookie;
  ASSERT_TRUE(tester.GetCookie(kCookieHostname, &cookie));
  EXPECT_EQ(kCookieName, cookie.Name());
  EXPECT_EQ(kCookieValue, cookie.Value());

  ResetAndWait(ProfileResetter::COOKIES_AND_SITE_DATA);

  EXPECT_FALSE(tester.GetCookie(kCookieHostname, &cookie));
}

}  // namespace

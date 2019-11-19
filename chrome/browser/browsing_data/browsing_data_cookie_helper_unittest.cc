// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Test expectations for a given cookie.
class CookieExpectation {
 public:
  CookieExpectation() {}

  bool MatchesCookie(const net::CanonicalCookie& cookie) const {
    if (!domain_.empty() && domain_ != cookie.Domain())
      return false;
    if (!path_.empty() && path_ != cookie.Path())
      return false;
    if (!name_.empty() && name_ != cookie.Name())
      return false;
    if (!value_.empty() && value_ != cookie.Value())
      return false;
    return true;
  }

  GURL source_;
  std::string domain_;
  std::string path_;
  std::string name_;
  std::string value_;
  bool matched_ = false;
};

// Matches a CookieExpectation against a Cookie.
class CookieMatcher {
 public:
  explicit CookieMatcher(const net::CanonicalCookie& cookie)
      : cookie_(cookie) {}
  bool operator()(const CookieExpectation& expectation) {
    return expectation.MatchesCookie(cookie_);
  }
  net::CanonicalCookie cookie_;
};

// Unary predicate to determine whether an expectation has been matched.
bool ExpectationIsMatched(const CookieExpectation& expectation) {
  return expectation.matched_;
}

class BrowsingDataCookieHelperTest : public testing::Test {
 public:
  BrowsingDataCookieHelperTest()
      : testing_profile_(new TestingProfile()) {
  }

  void SetUp() override { cookie_expectations_.clear(); }

  // Adds an expectation for a cookie that satisfies the given parameters.
  void AddCookieExpectation(const char* source,
                            const char* domain,
                            const char* path,
                            const char* name,
                            const char* value) {
    CookieExpectation matcher;
    if (source)
      matcher.source_ = GURL(source);
    if (domain)
      matcher.domain_ = domain;
    if (path)
      matcher.path_ = path;
    if (name)
      matcher.name_ = name;
    if (value)
      matcher.value_ = value;
    cookie_expectations_.push_back(matcher);
  }

  // Checks the existing expectations, and then clears all existing
  // expectations.
  void CheckCookieExpectations() {
    ASSERT_EQ(cookie_expectations_.size(), cookie_list_.size());

    // For each cookie, look for a matching expectation.
    for (const auto& cookie : cookie_list_) {
      CookieMatcher matcher(cookie);
      auto match = std::find_if(cookie_expectations_.begin(),
                                cookie_expectations_.end(), matcher);
      if (match != cookie_expectations_.end())
        match->matched_ = true;
    }

    // Check that each expectation has been matched.
    unsigned long match_count = std::count_if(cookie_expectations_.begin(),
                                              cookie_expectations_.end(),
                                              ExpectationIsMatched);
    EXPECT_EQ(cookie_expectations_.size(), match_count);

    cookie_expectations_.clear();
  }

  void CreateCookiesForTest() {
    base::Optional<base::Time> server_time = base::nullopt;
    auto cookie1 = net::CanonicalCookie::Create(
        GURL("https://www.google.com"), "A=1", base::Time::Now(), server_time);
    auto cookie2 =
        net::CanonicalCookie::Create(GURL("https://www.gmail.google.com"),
                                     "B=1", base::Time::Now(), server_time);

    network::mojom::CookieManager* cookie_manager =
        storage_partition()->GetCookieManagerForBrowserProcess();
    cookie_manager->SetCanonicalCookie(*cookie1, "https",
                                       net::CookieOptions::MakeAllInclusive(),
                                       base::DoNothing());
    cookie_manager->SetCanonicalCookie(*cookie2, "https",
                                       net::CookieOptions::MakeAllInclusive(),
                                       base::DoNothing());
  }

  void CreateCookiesForDomainCookieTest() {
    base::Optional<base::Time> server_time = base::nullopt;
    auto cookie1 = net::CanonicalCookie::Create(
        GURL("https://www.google.com"), "A=1", base::Time::Now(), server_time);
    auto cookie2 = net::CanonicalCookie::Create(GURL("https://www.google.com"),
                                                "A=2; Domain=.www.google.com ",
                                                base::Time::Now(), server_time);

    network::mojom::CookieManager* cookie_manager =
        storage_partition()->GetCookieManagerForBrowserProcess();
    cookie_manager->SetCanonicalCookie(*cookie1, "https",
                                       net::CookieOptions::MakeAllInclusive(),
                                       base::DoNothing());
    cookie_manager->SetCanonicalCookie(*cookie2, "https",
                                       net::CookieOptions::MakeAllInclusive(),
                                       base::DoNothing());
  }

  void FetchCallback(const net::CookieList& cookies) {
    cookie_list_ = cookies;

    AddCookieExpectation(nullptr, "www.google.com", nullptr, "A", nullptr);
    AddCookieExpectation(nullptr, "www.gmail.google.com", nullptr, "B",
                         nullptr);
    CheckCookieExpectations();
  }

  void DomainCookieCallback(const net::CookieList& cookies) {
    cookie_list_ = cookies;

    AddCookieExpectation(nullptr, "www.google.com", nullptr, "A", "1");
    AddCookieExpectation(nullptr, ".www.google.com", nullptr, "A", "2");
    CheckCookieExpectations();
  }

  void DeleteCallback(const net::CookieList& cookies) {
    cookie_list_ = cookies;
    AddCookieExpectation(nullptr, "www.gmail.google.com", nullptr, "B",
                         nullptr);
    CheckCookieExpectations();
  }

  void CannedUniqueCallback(const net::CookieList& cookies) {
    cookie_list_ = cookies;
    AddCookieExpectation("http://www.google.com/", "www.google.com", "/", "A",
                         nullptr);
    CheckCookieExpectations();
  }

  void CannedReplaceCookieCallback(const net::CookieList& cookies) {
    cookie_list_ = cookies;
    AddCookieExpectation(
        "http://www.google.com/", "www.google.com", "/", "A", "2");
    AddCookieExpectation(
        "http://www.google.com/", "www.google.com", "/example/0", "A", "4");
    AddCookieExpectation(
        "http://www.google.com/", ".google.com", "/", "A", "6");
    AddCookieExpectation(
        "http://www.google.com/", ".google.com", "/example/1", "A", "8");
    AddCookieExpectation(
        "http://www.google.com/", ".www.google.com", "/", "A", "10");
    CheckCookieExpectations();
  }

  void CannedDomainCookieCallback(const net::CookieList& cookies) {
    cookie_list_ = cookies;
    AddCookieExpectation("http://www.google.com/", "www.google.com", nullptr,
                         "A", nullptr);
    AddCookieExpectation("http://www.google.com/", ".www.google.com", nullptr,
                         "A", nullptr);
    CheckCookieExpectations();
  }

  void CannedDifferentFramesCallback(const net::CookieList& cookie_list) {
    ASSERT_EQ(3U, cookie_list.size());
  }

  void DeleteCookie(BrowsingDataCookieHelper* helper,
                    const std::string& domain) {
    for (const auto& cookie : cookie_list_) {
      if (cookie.Domain() == domain)
        helper->DeleteCookie(cookie);
    }
  }

  content::StoragePartition* storage_partition() {
    return content::BrowserContext::GetDefaultStoragePartition(
        testing_profile_.get());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> testing_profile_;

  std::vector<CookieExpectation> cookie_expectations_;
  net::CookieList cookie_list_;
};

TEST_F(BrowsingDataCookieHelperTest, FetchData) {
  CreateCookiesForTest();
  scoped_refptr<BrowsingDataCookieHelper> cookie_helper(
      new BrowsingDataCookieHelper(storage_partition()));

  cookie_helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::FetchCallback,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(BrowsingDataCookieHelperTest, DomainCookie) {
  CreateCookiesForDomainCookieTest();
  scoped_refptr<BrowsingDataCookieHelper> cookie_helper(
      new BrowsingDataCookieHelper(storage_partition()));

  cookie_helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::DomainCookieCallback,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(BrowsingDataCookieHelperTest, DeleteCookie) {
  CreateCookiesForTest();
  scoped_refptr<BrowsingDataCookieHelper> cookie_helper(
      new BrowsingDataCookieHelper(storage_partition()));

  cookie_helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::FetchCallback,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  net::CanonicalCookie cookie = cookie_list_[0];
  cookie_helper->DeleteCookie(cookie);

  cookie_helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::DeleteCallback,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(BrowsingDataCookieHelperTest, CannedDeleteCookie) {
  CreateCookiesForTest();
  scoped_refptr<CannedBrowsingDataCookieHelper> helper(
      new CannedBrowsingDataCookieHelper(storage_partition()));

  ASSERT_TRUE(helper->empty());

  const GURL origin1("http://www.google.com");
  std::unique_ptr<net::CanonicalCookie> cookie1(net::CanonicalCookie::Create(
      origin1, "A=1", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie1);
  helper->AddChangedCookie(origin1, origin1, *cookie1);
  const GURL origin2("http://www.gmail.google.com");
  std::unique_ptr<net::CanonicalCookie> cookie2(net::CanonicalCookie::Create(
      origin2, "B=1", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie2);
  helper->AddChangedCookie(origin2, origin2, *cookie2);

  helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::FetchCallback,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, helper->GetCookieCount());

  DeleteCookie(helper.get(), origin1.host());

  EXPECT_EQ(1u, helper->GetCookieCount());
  helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::DeleteCallback,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(BrowsingDataCookieHelperTest, CannedDomainCookie) {
  const GURL origin("http://www.google.com");
  net::CookieList cookie;

  scoped_refptr<CannedBrowsingDataCookieHelper> helper(
      new CannedBrowsingDataCookieHelper(storage_partition()));

  ASSERT_TRUE(helper->empty());
  std::unique_ptr<net::CanonicalCookie> cookie1(net::CanonicalCookie::Create(
      origin, "A=1", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie1);
  helper->AddChangedCookie(origin, origin, *cookie1);
  std::unique_ptr<net::CanonicalCookie> cookie2(net::CanonicalCookie::Create(
      origin, "A=1; Domain=.www.google.com", base::Time::Now(),
      base::nullopt /* server_time */));
  ASSERT_TRUE(cookie2);
  helper->AddChangedCookie(origin, origin, *cookie2);

  helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::CannedDomainCookieCallback,
                 base::Unretained(this)));
  cookie = cookie_list_;

  helper->Reset();
  ASSERT_TRUE(helper->empty());

  helper->AddReadCookies(origin, origin, cookie);
  helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::CannedDomainCookieCallback,
                 base::Unretained(this)));
}

TEST_F(BrowsingDataCookieHelperTest, CannedUnique) {
  const GURL origin("http://www.google.com");

  scoped_refptr<CannedBrowsingDataCookieHelper> helper(
      new CannedBrowsingDataCookieHelper(storage_partition()));

  ASSERT_TRUE(helper->empty());
  std::unique_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
      origin, "A=1", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie);
  helper->AddChangedCookie(origin, origin, *cookie);
  helper->AddChangedCookie(origin, origin, *cookie);
  helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::CannedUniqueCallback,
                 base::Unretained(this)));

  net::CookieList cookie_list = cookie_list_;
  helper->Reset();
  ASSERT_TRUE(helper->empty());

  helper->AddReadCookies(origin, origin, cookie_list);
  helper->AddReadCookies(origin, origin, cookie_list);
  helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::CannedUniqueCallback,
                 base::Unretained(this)));
}

TEST_F(BrowsingDataCookieHelperTest, CannedReplaceCookie) {
  const GURL origin("http://www.google.com");

  scoped_refptr<CannedBrowsingDataCookieHelper> helper(
      new CannedBrowsingDataCookieHelper(storage_partition()));

  ASSERT_TRUE(helper->empty());
  std::unique_ptr<net::CanonicalCookie> cookie1(net::CanonicalCookie::Create(
      origin, "A=1", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie1);
  helper->AddChangedCookie(origin, origin, *cookie1);
  std::unique_ptr<net::CanonicalCookie> cookie2(net::CanonicalCookie::Create(
      origin, "A=2", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie2);
  helper->AddChangedCookie(origin, origin, *cookie2);
  std::unique_ptr<net::CanonicalCookie> cookie3(net::CanonicalCookie::Create(
      origin, "A=3; Path=/example/0", base::Time::Now(),
      base::nullopt /* server_time */));
  ASSERT_TRUE(cookie3);
  helper->AddChangedCookie(origin, origin, *cookie3);
  std::unique_ptr<net::CanonicalCookie> cookie4(net::CanonicalCookie::Create(
      origin, "A=4; Path=/example/0", base::Time::Now(),
      base::nullopt /* server_time */));
  ASSERT_TRUE(cookie4);
  helper->AddChangedCookie(origin, origin, *cookie4);
  std::unique_ptr<net::CanonicalCookie> cookie5(net::CanonicalCookie::Create(
      origin, "A=5; Domain=google.com", base::Time::Now(),
      base::nullopt /* server_time */));
  ASSERT_TRUE(cookie5);
  helper->AddChangedCookie(origin, origin, *cookie5);
  std::unique_ptr<net::CanonicalCookie> cookie6(net::CanonicalCookie::Create(
      origin, "A=6; Domain=google.com", base::Time::Now(),
      base::nullopt /* server_time */));
  ASSERT_TRUE(cookie6);
  helper->AddChangedCookie(origin, origin, *cookie6);
  std::unique_ptr<net::CanonicalCookie> cookie7(net::CanonicalCookie::Create(
      origin, "A=7; Domain=google.com; Path=/example/1", base::Time::Now(),
      base::nullopt /* server_time */));
  ASSERT_TRUE(cookie7);
  helper->AddChangedCookie(origin, origin, *cookie7);
  std::unique_ptr<net::CanonicalCookie> cookie8(net::CanonicalCookie::Create(
      origin, "A=8; Domain=google.com; Path=/example/1", base::Time::Now(),
      base::nullopt /* server_time */));
  ASSERT_TRUE(cookie8);
  helper->AddChangedCookie(origin, origin, *cookie8);

  std::unique_ptr<net::CanonicalCookie> cookie9(net::CanonicalCookie::Create(
      origin, "A=9; Domain=www.google.com", base::Time::Now(),
      base::nullopt /* server_time */));
  ASSERT_TRUE(cookie9);
  helper->AddChangedCookie(origin, origin, *cookie9);
  std::unique_ptr<net::CanonicalCookie> cookie10(net::CanonicalCookie::Create(
      origin, "A=10; Domain=www.google.com", base::Time::Now(),
      base::nullopt /* server_time */));
  ASSERT_TRUE(cookie10);
  helper->AddChangedCookie(origin, origin, *cookie10);

  helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::CannedReplaceCookieCallback,
                 base::Unretained(this)));

  net::CookieList cookie_list = cookie_list_;
  helper->Reset();
  ASSERT_TRUE(helper->empty());

  helper->AddReadCookies(origin, origin, cookie_list);
  helper->AddReadCookies(origin, origin, cookie_list);
  helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::CannedReplaceCookieCallback,
                 base::Unretained(this)));
}

TEST_F(BrowsingDataCookieHelperTest, CannedEmpty) {
  const GURL url_google("http://www.google.com");

  scoped_refptr<CannedBrowsingDataCookieHelper> helper(
      new CannedBrowsingDataCookieHelper(storage_partition()));

  ASSERT_TRUE(helper->empty());
  std::unique_ptr<net::CanonicalCookie> changed_cookie(
      net::CanonicalCookie::Create(url_google, "a=1", base::Time::Now(),
                                   base::nullopt /* server_time */));
  ASSERT_TRUE(changed_cookie);
  helper->AddChangedCookie(url_google, url_google, *changed_cookie);
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());

  net::CookieList cookies;
  std::unique_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
      url_google, "a=1", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie);
  cookies.push_back(*cookie);

  helper->AddReadCookies(url_google, url_google, cookies);
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

TEST_F(BrowsingDataCookieHelperTest, CannedDifferentFrames) {
  GURL frame1_url("http://www.google.com");
  GURL frame2_url("http://www.google.de");
  GURL request_url("http://www.google.com");

  scoped_refptr<CannedBrowsingDataCookieHelper> helper(
      new CannedBrowsingDataCookieHelper(storage_partition()));

  ASSERT_TRUE(helper->empty());
  std::unique_ptr<net::CanonicalCookie> cookie1(net::CanonicalCookie::Create(
      request_url, "a=1", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie1);
  helper->AddChangedCookie(frame1_url, request_url, *cookie1);
  std::unique_ptr<net::CanonicalCookie> cookie2(net::CanonicalCookie::Create(
      request_url, "b=1", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie2);
  helper->AddChangedCookie(frame1_url, request_url, *cookie2);
  std::unique_ptr<net::CanonicalCookie> cookie3(net::CanonicalCookie::Create(
      request_url, "c=1", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie3);
  helper->AddChangedCookie(frame2_url, request_url, *cookie3);

  helper->StartFetching(
      base::Bind(&BrowsingDataCookieHelperTest::CannedDifferentFramesCallback,
                 base::Unretained(this)));
}

TEST_F(BrowsingDataCookieHelperTest, CannedGetCookieCount) {
  // The URL in the omnibox is a frame URL. This is not necessarily the request
  // URL, since websites usually include other resources.
  GURL frame1_url("http://www.google.com");
  GURL frame2_url("http://www.google.de");
  // The request URL used for all cookies that are added to the |helper|.
  GURL request1_url("http://static.google.com/foo/res1.html");
  GURL request2_url("http://static.google.com/bar/res2.html");
  std::string cookie_domain(".www.google.com");

  scoped_refptr<CannedBrowsingDataCookieHelper> helper(
      new CannedBrowsingDataCookieHelper(storage_partition()));

  // Add two different cookies (distinguished by the tuple [cookie-name,
  // domain-value, path-value]) for a HTTP request to |frame1_url| and verify
  // that the cookie count is increased to two. The set-cookie-string consists
  // only of the cookie-pair. This means that the host and the default-path of
  // the |request_url| are used as domain-value and path-value for the added
  // cookies.
  EXPECT_EQ(0U, helper->GetCookieCount());
  std::unique_ptr<net::CanonicalCookie> cookie1(net::CanonicalCookie::Create(
      frame1_url, "A=1", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie1);
  helper->AddChangedCookie(frame1_url, frame1_url, *cookie1);
  EXPECT_EQ(1U, helper->GetCookieCount());
  std::unique_ptr<net::CanonicalCookie> cookie2(net::CanonicalCookie::Create(
      frame1_url, "B=1", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie2);
  helper->AddChangedCookie(frame1_url, frame1_url, *cookie2);
  EXPECT_EQ(2U, helper->GetCookieCount());

  // Use a different frame URL for adding another cookie that will replace one
  // of the previously added cookies. This could happen during an automatic
  // redirect e.g. |frame1_url| redirects to |frame2_url| and a cookie set by a
  // request to |frame1_url| is updated.
  // The cookie-name of |cookie3| must match the cookie-name of |cookie1|.
  std::unique_ptr<net::CanonicalCookie> cookie3(net::CanonicalCookie::Create(
      frame1_url, "A=2", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie3);
  helper->AddChangedCookie(frame2_url, frame1_url, *cookie3);
  EXPECT_EQ(2U, helper->GetCookieCount());

  // Add two more cookies that are set while loading resources. The two cookies
  // below have a differnt path-value since the request URLs have different
  // paths.
  std::unique_ptr<net::CanonicalCookie> cookie4(net::CanonicalCookie::Create(
      request1_url, "A=2", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie4);
  helper->AddChangedCookie(frame2_url, request1_url, *cookie4);
  EXPECT_EQ(3U, helper->GetCookieCount());
  std::unique_ptr<net::CanonicalCookie> cookie5(net::CanonicalCookie::Create(
      request2_url, "A=2", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie5);
  helper->AddChangedCookie(frame2_url, request2_url, *cookie5);
  EXPECT_EQ(4U, helper->GetCookieCount());

  // Host-only and domain cookies are treated as seperate items. This means that
  // the following two cookie-strings are stored as two separate cookies, even
  // though they have the same name and are send with the same request:
  //   "A=1;
  //   "A=3; Domain=www.google.com"
  // Add a domain cookie and check if it increases the cookie count.
  std::unique_ptr<net::CanonicalCookie> cookie6(net::CanonicalCookie::Create(
      frame1_url, "A=3; Domain=.www.google.com", base::Time::Now(),
      base::nullopt /* server_time */));
  ASSERT_TRUE(cookie6);
  helper->AddChangedCookie(frame2_url, frame1_url, *cookie6);
  EXPECT_EQ(5U, helper->GetCookieCount());
}

}  // namespace

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
#include "chrome/test/base/ui_test_utils.h"
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

#if BUILDFLAG(IS_CHROMEOS)
#include "base/containers/to_vector.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler_impl.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

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

RemoveCookieTester::~RemoveCookieTester() = default;

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

// PinnedTabsResetTest --------------------------------------------------------

class PinnedTabsResetTest : public InProcessBrowserTest,
                            public ProfileResetterTestBase {
 protected:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    resetter_ = std::make_unique<ProfileResetter>(browser()->profile());
  }

  content::WebContents* AddTab(const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

// TODO(434716727): The test is flaky on Mac machines.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ResetPinnedTabs DISABLED_ResetPinnedTabs
#else
#define MAYBE_ResetPinnedTabs ResetPinnedTabs
#endif
IN_PROC_BROWSER_TEST_F(PinnedTabsResetTest, MAYBE_ResetPinnedTabs) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  const GURL kTestURLs[] = {GURL("https://example.com/0"),
                            GURL("https://example.com/1"),
                            GURL("https://example.com/2"),
                            GURL("https://example.com/3"),
                            GURL("https://example.com/4")};

  // Start with one tab (about:blank). Navigating it.
  content::WebContents* initial_contents = tab_strip_model->GetWebContentsAt(0);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kTestURLs[0]));
  initial_contents = tab_strip_model->GetWebContentsAt(0);

  // Add 4 tabs
  content::WebContents* tab_contents1 = AddTab(kTestURLs[1]);
  content::WebContents* tab_contents2 = AddTab(kTestURLs[2]);
  content::WebContents* tab_contents3 = AddTab(kTestURLs[3]);
  content::WebContents* tab_contents4 = AddTab(kTestURLs[4]);

  // Current order: initial, tab_contents1, tab_contents2, tab_contents3,
  // tab_contents4

  // Pin tab_contents2 and tab_contents1
  tab_strip_model->SetTabPinned(
      tab_strip_model->GetIndexOfWebContents(tab_contents2), true);
  tab_strip_model->SetTabPinned(
      tab_strip_model->GetIndexOfWebContents(tab_contents1), true);

  // Expected order after pinning: tab_contents2, tab_contents1, initial,
  // tab_contents3, tab_contents4
  EXPECT_EQ(5, tab_strip_model->count());
  EXPECT_EQ(tab_contents2, tab_strip_model->GetWebContentsAt(0));
  EXPECT_EQ(tab_contents1, tab_strip_model->GetWebContentsAt(1));
  EXPECT_EQ(initial_contents, tab_strip_model->GetWebContentsAt(2));
  EXPECT_EQ(tab_contents3, tab_strip_model->GetWebContentsAt(3));
  EXPECT_EQ(tab_contents4, tab_strip_model->GetWebContentsAt(4));
  EXPECT_EQ(2, tab_strip_model->IndexOfFirstNonPinnedTab());

  // Note: unpinning in the function below occurs in reverse order, because
  // if we unpin the tab, it could be moved to the right, and traversing
  // in left-to-right order would skip some pinned tabs.
  ResetAndWait(ProfileResetter::PINNED_TABS);

  // The order should be preserved, just all unpinned.
  EXPECT_EQ(kTestURLs[2], tab_strip_model->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(kTestURLs[1], tab_strip_model->GetWebContentsAt(1)->GetURL());
  EXPECT_EQ(kTestURLs[0], tab_strip_model->GetWebContentsAt(2)->GetURL());
  EXPECT_EQ(kTestURLs[3], tab_strip_model->GetWebContentsAt(3)->GetURL());
  EXPECT_EQ(kTestURLs[4], tab_strip_model->GetWebContentsAt(4)->GetURL());

  EXPECT_EQ(0, tab_strip_model->IndexOfFirstNonPinnedTab());
}

#if BUILDFLAG(IS_CHROMEOS)
// Returns the configured static name servers from `shill_properties`, or an
// empty vector if no static name servers are configured.
std::vector<std::string> GetStaticNameServersFromShillProperties(
    const base::Value::Dict& shill_properties) {
  const base::Value::Dict* static_ip_config =
      shill_properties.FindDict(shill::kStaticIPConfigProperty);
  if (!static_ip_config) {
    return {};
  }
  const base::Value::List* nameservers =
      static_ip_config->FindList(shill::kNameServersProperty);
  if (!nameservers) {
    return {};
  }
  return base::ToVector(*nameservers, [](const base::Value& nameserver) {
    return nameserver.GetString();
  });
}

// DnsConfigResetTest --------------------------------------------------------

class DnsConfigResetTest : public InProcessBrowserTest,
                           public ProfileResetterTestBase {
 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    resetter_ = std::make_unique<ProfileResetter>(browser()->profile());
  }
};

IN_PROC_BROWSER_TEST_F(DnsConfigResetTest, ResetDnsConfigurations) {
  ash::ShillServiceClient::TestInterface* shill_service_client =
      ash::ShillServiceClient::Get()->GetTestInterface();

  // DNS settings.
  // Set the profile so this shows up as a configured network.
  const std::string kWifi1Path = "/service/wifi1";
  ash::NetworkHandler::Get()
      ->managed_network_configuration_handler()
      ->SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
                  base::Value::List(), base::Value::Dict());
  // Set a static NameServers config.
  base::Value::Dict static_ip_config;
  base::Value::List name_servers;
  name_servers.Append("8.8.3.1");
  name_servers.Append("8.8.2.1");
  name_servers.Append("0.0.0.0");
  name_servers.Append("0.0.0.0");
  static_ip_config.Set(shill::kNameServersProperty, std::move(name_servers));
  shill_service_client->SetServiceProperty(
      kWifi1Path, shill::kStaticIPConfigProperty,
      base::Value(std::move(static_ip_config)));

  // Verify that network exists and the custom name server has been applied.
  const base::Value::Dict* shill_properties =
      shill_service_client->GetServiceProperties(kWifi1Path);
  ASSERT_TRUE(shill_properties);
  EXPECT_THAT(GetStaticNameServersFromShillProperties(*shill_properties),
              testing::ElementsAre("8.8.3.1", "8.8.2.1", "0.0.0.0", "0.0.0.0"));

  ResetAndWait(ProfileResetter::DNS_CONFIGURATIONS);

  // Check DNS settings have changed to expected defaults.
  // Verify that the given network has it's NameServers field cleared.
  EXPECT_THAT(GetStaticNameServersFromShillProperties(*shill_properties),
              testing::IsEmpty());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

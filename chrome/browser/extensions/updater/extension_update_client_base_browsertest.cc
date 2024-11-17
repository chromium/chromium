// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/extension_update_client_base_browsertest.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/net/url_loader_post_interceptor.h"
#include "components/update_client/protocol_handler.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/updater/update_service.h"
#include "extensions/browser/updater/update_service_factory.h"
#include "extensions/common/extension_features.h"

namespace extensions {

namespace {

using ConfigFactoryCallback =
    ExtensionUpdateClientBaseTest::ConfigFactoryCallback;

class TestChromeUpdateClientConfig
    : public extensions::ChromeUpdateClientConfig {
 public:
  TestChromeUpdateClientConfig(content::BrowserContext* context,
                               const std::vector<GURL>& update_url,
                               const std::vector<GURL>& ping_url)
      : extensions::ChromeUpdateClientConfig(context, std::nullopt),
        update_url_(update_url),
        ping_url_(ping_url) {}

  TestChromeUpdateClientConfig(const TestChromeUpdateClientConfig&) = delete;
  TestChromeUpdateClientConfig& operator=(const TestChromeUpdateClientConfig&) =
      delete;

  // Overrides for update_client::Configurator.
  std::vector<GURL> UpdateUrl() const final { return update_url_; }

  std::vector<GURL> PingUrl() const final { return ping_url_; }

  bool EnabledCupSigning() const final { return false; }

  bool EnabledBackgroundDownloader() const final {
    // Some tests rely on URLFetcher network interceptors, which won't intercept
    // BITS requests.
    return false;
  }

  std::unique_ptr<update_client::ProtocolHandlerFactory>
  GetProtocolHandlerFactory() const final {
    return std::make_unique<update_client::ProtocolHandlerFactoryJSON>();
  }

 protected:
  ~TestChromeUpdateClientConfig() override = default;

 private:
  std::vector<GURL> update_url_;
  std::vector<GURL> ping_url_;
};

// This class implements a simple Chrome extra part that is used to
// set up the network interceptors in the test class. This would allow us to
// reliably inject the network interceptors before the browser starts running.
class TestChromeBrowserMainExtraParts : public ChromeBrowserMainExtraParts {
 public:
  explicit TestChromeBrowserMainExtraParts(ExtensionUpdateClientBaseTest* test)
      : test_(test) {}

  TestChromeBrowserMainExtraParts(const TestChromeBrowserMainExtraParts&) =
      delete;
  TestChromeBrowserMainExtraParts& operator=(
      const TestChromeBrowserMainExtraParts&) = delete;

  ~TestChromeBrowserMainExtraParts() override = default;

  // ChromeBrowserMainExtraParts:
  void PreProfileInit() override { test_->SetUpNetworkInterceptors(); }

 private:
  raw_ptr<ExtensionUpdateClientBaseTest> test_;
};

class UpdateClientCompleteEventWaiter
    : public update_client::UpdateClient::Observer {
 public:
  explicit UpdateClientCompleteEventWaiter(const std::string& id) : id_(id) {}

  UpdateClientCompleteEventWaiter(const UpdateClientCompleteEventWaiter&) =
      delete;
  UpdateClientCompleteEventWaiter& operator=(
      const UpdateClientCompleteEventWaiter&) = delete;

  ~UpdateClientCompleteEventWaiter() override = default;

  void OnEvent(const update_client::CrxUpdateItem& item) final {
    if (id_ == item.id &&
        (item.state == update_client::ComponentState::kUpdated ||
         item.state == update_client::ComponentState::kUpToDate ||
         item.state == update_client::ComponentState::kUpdateError)) {
      state_ = item.state;
      run_loop_.Quit();
    }
  }

  update_client::ComponentState Wait() {
    run_loop_.Run();
    return state_;
  }

 private:
  const std::string id_;
  update_client::ComponentState state_ =
      update_client::ComponentState::kUpdateError;
  base::RunLoop run_loop_;
};

}  // namespace

ExtensionUpdateClientBaseTest::ExtensionUpdateClientBaseTest()
    : https_server_for_update_(net::EmbeddedTestServer::TYPE_HTTPS),
      https_server_for_ping_(net::EmbeddedTestServer::TYPE_HTTPS) {}

ExtensionUpdateClientBaseTest::~ExtensionUpdateClientBaseTest() = default;

std::vector<GURL> ExtensionUpdateClientBaseTest::GetUpdateUrls() const {
  return {https_server_for_update_.GetURL("/updatehost/service/update")};
}

std::vector<GURL> ExtensionUpdateClientBaseTest::GetPingUrls() const {
  return {https_server_for_ping_.GetURL("/pinghost/service/ping")};
}

ConfigFactoryCallback
ExtensionUpdateClientBaseTest::ChromeUpdateClientConfigFactory() const {
  return base::BindRepeating(
      [](const std::vector<GURL>& update_url, const std::vector<GURL>& ping_url,
         content::BrowserContext* context)
          -> scoped_refptr<ChromeUpdateClientConfig> {
        return base::MakeRefCounted<TestChromeUpdateClientConfig>(
            context, update_url, ping_url);
      },
      GetUpdateUrls(), GetPingUrls());
}

void ExtensionUpdateClientBaseTest::SetUp() {
  ASSERT_TRUE(https_server_for_update_.InitializeAndListen());
  ASSERT_TRUE(https_server_for_ping_.InitializeAndListen());

  ChromeUpdateClientConfig::SetChromeUpdateClientConfigFactoryForTesting(
      ChromeUpdateClientConfigFactory());
  ExtensionBrowserTest::SetUp();
}

void ExtensionUpdateClientBaseTest::CreatedBrowserMainParts(
    content::BrowserMainParts* parts) {
  ExtensionBrowserTest::CreatedBrowserMainParts(parts);
  static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
      std::make_unique<TestChromeBrowserMainExtraParts>(this));
}

void ExtensionUpdateClientBaseTest::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();
  extensions::browsertest_util::CreateAndInitializeLocalCache();

  update_service_ =
      extensions::UpdateServiceFactory::GetForBrowserContext(profile());
  ASSERT_TRUE(update_service_);
}

void ExtensionUpdateClientBaseTest::TearDownOnMainThread() {
  get_interceptor_.reset();
}

void ExtensionUpdateClientBaseTest::SetUpNetworkInterceptors() {
  const auto update_urls = GetUpdateUrls();
  ASSERT_TRUE(!update_urls.empty());
  const GURL update_url = update_urls.front();

  update_interceptor_ =
      std::make_unique<update_client::URLLoaderPostInterceptor>(
          update_urls, &https_server_for_update_);
  https_server_for_update_.StartAcceptingConnections();

  const auto ping_urls = GetPingUrls();
  ASSERT_TRUE(!ping_urls.empty());
  const GURL ping_url = ping_urls.front();

  ping_interceptor_ = std::make_unique<update_client::URLLoaderPostInterceptor>(
      ping_urls, &https_server_for_ping_);
  https_server_for_ping_.StartAcceptingConnections();

  get_interceptor_ =
      std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
          &ExtensionUpdateClientBaseTest::OnRequest, base::Unretained(this)));
}

update_client::ComponentState
ExtensionUpdateClientBaseTest::WaitOnComponentUpdaterCompleteEvent(
    const std::string& id) {
  UpdateClientCompleteEventWaiter waiter(id);
  update_service_->AddUpdateClientObserver(&waiter);
  auto event = waiter.Wait();
  update_service_->RemoveUpdateClientObserver(&waiter);

  return event;
}

bool ExtensionUpdateClientBaseTest::OnRequest(
    content::URLLoaderInterceptor::RequestParams* params) {
  if (params->url_request.url.host() != "localhost") {
    return false;
  }

  get_interceptor_count_++;
  return callback_ && callback_.Run(params);
}

}  // namespace extensions

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATE_CLIENT_BASE_BROWSERTEST_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATE_CLIENT_BASE_BROWSERTEST_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/updater/chrome_update_client_config.h"
#include "components/update_client/update_client.h"
#include "content/public/test/url_loader_interceptor.h"

namespace base {
namespace test {
class ScopedFeatureList;
}  // namespace test
}  // namespace base

namespace content {
class BrowserMainParts;
}  // namespace content

namespace update_client {
class URLLoaderPostInterceptor;
}  // namespace update_client

namespace extensions {

class ChromeUpdateClientConfig;
class UpdateService;

// Base class to browser test extension updater using UpdateClient.
class ExtensionUpdateClientBaseTest : public ExtensionBrowserTest {
 public:
  using ConfigFactoryCallback = ChromeUpdateClientConfig::FactoryCallback;

  ExtensionUpdateClientBaseTest();
  ~ExtensionUpdateClientBaseTest() override;

  // ExtensionBrowserTest:
  void SetUp() override;
  void SetUpOnMainThread() override;
  void CreatedBrowserMainParts(content::BrowserMainParts* parts) final;
  void TearDownOnMainThread() override;

  // Injects a test configurator to the main extension browser client.
  // Override this function to inject your own custom configurator to the
  // extension browser client (through ConfigFactoryCallback).
  virtual ConfigFactoryCallback ChromeUpdateClientConfigFactory() const;

  // Wait for an update on extension |id| to finish.
  // The return value gives the result of the completed update operation
  // (error, update, no update) as defined in
  // |update_client::UpdateClient::Observer::Events|
  update_client::UpdateClient::Observer::Events
  WaitOnComponentUpdaterCompleteEvent(const std::string& id);

  // Creates network interceptors.
  // Override this function to provide your own network interceptors.
  virtual void SetUpNetworkInterceptors();

  virtual std::vector<GURL> GetUpdateUrls() const;
  virtual std::vector<GURL> GetPingUrls() const;

  void set_interceptor_hook(
      content::URLLoaderInterceptor::InterceptCallback callback) {
    callback_ = std::move(callback);
  }

  int get_interceptor_count() { return get_interceptor_count_; }

 protected:
  extensions::UpdateService* update_service_ = nullptr;
  std::unique_ptr<content::URLLoaderInterceptor> get_interceptor_;
  int get_interceptor_count_ = 0;
  content::URLLoaderInterceptor::InterceptCallback callback_;

  std::unique_ptr<update_client::URLLoaderPostInterceptor> update_interceptor_;
  std::unique_ptr<update_client::URLLoaderPostInterceptor> ping_interceptor_;

  net::EmbeddedTestServer https_server_for_update_;
  net::EmbeddedTestServer https_server_for_ping_;

 private:
  bool OnRequest(content::URLLoaderInterceptor::RequestParams* params);

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUpdateClientBaseTest);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATE_CLIENT_BASE_BROWSERTEST_H_

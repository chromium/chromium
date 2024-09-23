// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/safe_browsing/aw_ping_manager_factory.h"

#include "android_webview/browser/aw_browser_process.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

// static
AwPingManagerFactory* AwPingManagerFactory::GetInstance() {
  static base::NoDestructor<AwPingManagerFactory> instance;
  return instance.get();
}

// static
PingManager* AwPingManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PingManager*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

AwPingManagerFactory::AwPingManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "AwSafeBrowsingPingManager",
          BrowserContextDependencyManager::GetInstance()) {}

AwPingManagerFactory::~AwPingManagerFactory() = default;

std::unique_ptr<KeyedService>
AwPingManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // Never fetch the access token for android_webview since ESB is unsupported
  auto get_should_fetch_access_token =
      base::BindRepeating([]() { return false; });
  // Persisted report is not supported on WebView, because only download reports
  // are persisted and WebView doesn't have download protection.
  auto get_should_send_persisted_report =
      base::BindRepeating([]() { return false; });
  return PingManager::Create(
      safe_browsing::GetV4ProtocolConfig(GetProtocolConfigClientName(),
                                         /*disable_auto_update=*/false),
      GetURLLoaderFactory(), /*token_fetcher=*/nullptr,
      get_should_fetch_access_token,
      safe_browsing::WebUIInfoSingleton::GetInstance(),
      content::GetUIThreadTaskRunner({}),
      // TODO(crbug.com/40814717) If features get added that can alter
      // user population values in android_webview, we should consider
      // threading the user population through for client reports
      /*get_user_population_callback=*/base::NullCallback(),
      /*get_page_load_token_callback_=*/base::NullCallback(),
      /*hats_delegate=*/nullptr, /*persister_root_path=*/context->GetPath(),
      /*get_should_send_persisted_report=*/
      std::move(get_should_send_persisted_report));
}

std::string AwPingManagerFactory::GetProtocolConfigClientName() const {
  // Return a webview specific client name, see crbug.com/732373 for details.
  return "android_webview";
}

scoped_refptr<network::SharedURLLoaderFactory>
AwPingManagerFactory::GetURLLoaderFactory() const {
  if (testing_url_loader_factory_) {
    return testing_url_loader_factory_;
  }
  // TODO(crbug.com/40820267): Support separate SafeBrowsingNetworkContexts per
  // browser context instead of having the same one all contexts. If done
  // similar to the chrome/ implementation, GetURLLoaderFactory will take in a
  // browser context as a parameter.
  return android_webview::AwBrowserProcess::GetInstance()
      ->GetSafeBrowsingUIManager()
      ->GetURLLoaderFactory();
}

void AwPingManagerFactory::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  testing_url_loader_factory_ = url_loader_factory;
}

}  // namespace safe_browsing

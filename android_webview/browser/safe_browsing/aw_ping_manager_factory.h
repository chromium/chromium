// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_PING_MANAGER_FACTORY_H_
#define ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_PING_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/safe_browsing/core/browser/ping_manager.h"

namespace safe_browsing {

// Factory for creating the KeyedService PingManager for Android WebView.
// Lifetime: Singleton
class AwPingManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AwPingManagerFactory* GetInstance();
  static PingManager* GetForBrowserContext(content::BrowserContext* context);

  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

 private:
  friend class base::NoDestructor<AwPingManagerFactory>;

  AwPingManagerFactory();
  ~AwPingManagerFactory() override;

  // BrowserContextKeyedServiceFactory override:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  std::string GetProtocolConfigClientName() const;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() const;

  scoped_refptr<network::SharedURLLoaderFactory> testing_url_loader_factory_;
};

}  // namespace safe_browsing
#endif  // ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_PING_MANAGER_FACTORY_H_

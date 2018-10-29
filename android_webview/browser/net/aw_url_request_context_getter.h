// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NET_AW_URL_REQUEST_CONTEXT_GETTER_H_
#define ANDROID_WEBVIEW_BROWSER_NET_AW_URL_REQUEST_CONTEXT_GETTER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/browser_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory.h"

class PrefService;
class PrefRegistrySimple;

namespace net {
class FileNetLogObserver;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpAuthPreferences;
class HttpUserAgentSettings;
class NetLog;
class ProxyConfigServiceAndroid;
class ProxyConfigService;
class URLRequestContext;
class URLRequestJobFactory;
}

namespace android_webview {

class AwURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  AwURLRequestContextGetter(
      const base::FilePath& cache_path,
      const base::FilePath& channel_id_path,
      std::unique_ptr<net::ProxyConfigServiceAndroid> config_service,
      PrefService* pref_service,
      net::NetLog* net_log);

  static void set_check_cleartext_permitted(bool permitted);
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // net::URLRequestContextGetter implementation.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

  // Methods to set and clear proxy override
  void SetProxyOverride(const std::string& host,
                        int port,
                        const std::vector<std::string>& exclusion_list,
                        base::OnceClosure callback);
  void ClearProxyOverride(base::OnceClosure callback);

 private:
  friend class AwBrowserContext;
  friend class AwURLRequestContextGetterTest;
  ~AwURLRequestContextGetter() override;

  // Prior to GetURLRequestContext() being called, this is called to hand over
  // the objects that GetURLRequestContext() will later install into
  // |job_factory_|.  This ordering is enforced by having
  // AwBrowserContext::CreateRequestContext() call this method.
  // This method is necessary because the passed in objects are created
  // on the UI thread while |job_factory_| must be created on the IO thread.
  void SetHandlersAndInterceptors(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors);

  void InitializeURLRequestContext();

  // This is called to create a HttpAuthHandlerFactory that will handle
  // auth challenges for the new URLRequestContext
  std::unique_ptr<net::HttpAuthHandlerFactory> CreateAuthHandlerFactory(
      net::HostResolver* resolver);

  // Update methods for the auth related preferences
  void UpdateServerWhitelist();
  void UpdateAndroidAuthNegotiateAccountType();

  const base::FilePath cache_path_;
  const base::FilePath channel_id_path_;

  net::NetLog* net_log_;
  std::unique_ptr<net::ProxyConfigServiceAndroid> proxy_config_service_;
  net::ProxyConfigServiceAndroid* proxy_config_service_android_;
  std::unique_ptr<net::URLRequestJobFactory> job_factory_;
  std::unique_ptr<net::HttpUserAgentSettings> http_user_agent_settings_;
  std::unique_ptr<net::FileNetLogObserver> file_net_log_observer_;
  // http_auth_preferences_ holds the preferences for the negotiate
  // authenticator.
  std::unique_ptr<net::HttpAuthPreferences> http_auth_preferences_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;

  // Store HTTP Auth-related policies in this thread.
  StringPrefMember auth_android_negotiate_account_type_;
  StringPrefMember auth_server_whitelist_;

  // ProtocolHandlers and interceptors are stored here between
  // SetHandlersAndInterceptors() and the first GetURLRequestContext() call.
  content::ProtocolHandlerMap protocol_handlers_;
  content::URLRequestInterceptorScopedVector request_interceptors_;

  DISALLOW_COPY_AND_ASSIGN(AwURLRequestContextGetter);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NET_AW_URL_REQUEST_CONTEXT_GETTER_H_

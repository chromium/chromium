// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_PAC_PROCESSOR_H_
#define ANDROID_WEBVIEW_BROWSER_AW_PAC_PROCESSOR_H_

#include <android/multinetwork.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "net/log/net_log_with_source.h"
#include "services/proxy_resolver/proxy_host_resolver.h"
#include "services/proxy_resolver/proxy_resolver_v8_tracing.h"

namespace android_webview {

class Job;
class HostResolver;

class AwPacProcessor {
 public:
  AwPacProcessor();
  AwPacProcessor(const AwPacProcessor&) = delete;
  AwPacProcessor& operator=(const AwPacProcessor&) = delete;

  ~AwPacProcessor();
  void DestroyNative(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);

  jboolean SetProxyScript(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          const base::android::JavaParamRef<jstring>& jscript);
  bool SetProxyScript(std::string script);
  base::android::ScopedJavaLocalRef<jstring> MakeProxyRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jurl);
  bool MakeProxyRequest(std::string url, std::string* result);
  void SetNetworkAndLinkAddresses(
      JNIEnv* env,
      net_handle_t net_handle,
      const base::android::JavaParamRef<jobjectArray>& addresses);

 private:
  void Destroy(base::WaitableEvent* event);
  void SetProxyScriptNative(
      std::unique_ptr<net::ProxyResolverFactory::Request>* request,
      const std::string& script,
      net::CompletionOnceCallback complete);
  void MakeProxyRequestNative(
      std::unique_ptr<net::ProxyResolver::Request>* request,
      const std::string& url,
      net::ProxyInfo* proxy_info,
      net::CompletionOnceCallback complete);

  friend class Job;
  friend class SetProxyScriptJob;
  friend class MakeProxyRequestJob;

  std::unique_ptr<proxy_resolver::ProxyResolverV8Tracing> proxy_resolver_;
  std::unique_ptr<HostResolver> host_resolver_;

  std::set<raw_ptr<Job, SetExperimental>> jobs_;
};
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_PAC_PROCESSOR_H_

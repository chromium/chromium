// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_pac_processor.h"

#include <android/multinetwork.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <netdb.h>
#include <unistd.h>
#include <cstddef>
#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_isolation_key.h"
#include "net/proxy_resolution/pac_file_data.h"
#include "net/proxy_resolution/proxy_info.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwPacProcessor_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace android_webview {

namespace {
static_assert(NETWORK_UNSPECIFIED == 0,
              "Java side AwPacProcessor#NETWORK_UNSPECIFIED needs update");

typedef int (*getaddrinfofornetwork_ptr_t)(net_handle_t,
                                           const char*,
                                           const char*,
                                           const struct addrinfo*,
                                           struct addrinfo**);

// The definition of android_getaddrinfofornetwork is conditional
// on __ANDROID_API__ >= 23. It's not possible to just have a runtime check for
// the SDK level to guard a call that might not exist on older platform
// versions: all native function imports are resolved at load time and loading
// the library will fail if they're unresolvable. Therefore we need to search
// for the function via dlsym.
int AndroidGetAddrInfoForNetwork(net_handle_t network,
                                 const char* node,
                                 const char* service,
                                 const struct addrinfo* hints,
                                 struct addrinfo** res) {
  static getaddrinfofornetwork_ptr_t getaddrinfofornetwork = [] {
    getaddrinfofornetwork_ptr_t ptr =
        reinterpret_cast<getaddrinfofornetwork_ptr_t>(
            dlsym(RTLD_DEFAULT, "android_getaddrinfofornetwork"));
    DCHECK(ptr);
    return ptr;
  }();
  return getaddrinfofornetwork(network, node, service, hints, res);
}

scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
  struct ThreadHolder {
    base::Thread thread_;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

    ThreadHolder() : thread_("AwPacProcessor") {
      thread_.Start();
      task_runner_ = thread_.task_runner();
    }
  };
  static ThreadHolder thread_holder;

  return thread_holder.task_runner_;
}

proxy_resolver::ProxyResolverV8TracingFactory* GetProxyResolverFactory() {
  static std::unique_ptr<proxy_resolver::ProxyResolverV8TracingFactory>
      factory = proxy_resolver::ProxyResolverV8TracingFactory::Create();

  return factory.get();
}

}  // namespace

// TODO(amalova): We could use a separate thread or thread pool for executing
// blocking DNS queries, to get better performance.
class HostResolver : public proxy_resolver::ProxyHostResolver {
 public:
  std::unique_ptr<proxy_resolver::ProxyHostResolver::Request> CreateRequest(
      const std::string& hostname,
      net::ProxyResolveDnsOperation operation,
      const net::NetworkAnonymizationKey&) override {
    return std::make_unique<RequestImpl>(hostname, operation, net_handle_,
                                         link_addresses_);
  }

  void SetNetworkAndLinkAddresses(
      const net_handle_t net_handle,
      const std::vector<net::IPAddress>& link_addresses) {
    link_addresses_ = link_addresses;
    net_handle_ = net_handle;
  }

 private:
  net_handle_t net_handle_ = 0;
  std::vector<net::IPAddress> link_addresses_;

  class RequestImpl : public proxy_resolver::ProxyHostResolver::Request {
   public:
    RequestImpl(const std::string& hostname,
                net::ProxyResolveDnsOperation operation,
                net_handle_t net_handle,
                const std::vector<net::IPAddress>& link_addresses)
        : hostname_(hostname),
          operation_(operation),
          net_handle_(net_handle),
          link_addresses_(link_addresses) {}
    ~RequestImpl() override = default;

    int Start(net::CompletionOnceCallback callback) override {
      bool success = false;
      switch (operation_) {
        case net::ProxyResolveDnsOperation::DNS_RESOLVE:
          success = DnsResolveImpl(hostname_);
          break;
        case net::ProxyResolveDnsOperation::DNS_RESOLVE_EX:
          success = DnsResolveExImpl(hostname_);
          break;
        case net::ProxyResolveDnsOperation::MY_IP_ADDRESS:
          success = MyIpAddressImpl();
          break;
        case net::ProxyResolveDnsOperation::MY_IP_ADDRESS_EX:
          success = MyIpAddressExImpl();
          break;
      }
      return success ? net::OK : net::ERR_NAME_RESOLUTION_FAILED;
    }

    const std::vector<net::IPAddress>& GetResults() const override {
      return results_;
    }

   private:
    bool IsNetworkSpecified() { return net_handle_ != NETWORK_UNSPECIFIED; }

    bool MyIpAddressImpl() {
      // For network-aware queries the results are set from Java on
      // NetworkCallback#onLinkPropertiesChanged.
      // See SetNetworkAndLinkAddresses.
      if (IsNetworkSpecified()) {
        results_.push_back(link_addresses_.front());
        return true;
      }

      std::string my_hostname = GetHostName();
      if (my_hostname.empty())
        return false;
      return DnsResolveImpl(my_hostname);
    }

    bool MyIpAddressExImpl() {
      if (IsNetworkSpecified()) {
        results_ = link_addresses_;
        return true;
      }

      std::string my_hostname = GetHostName();
      if (my_hostname.empty())
        return false;
      return DnsResolveExImpl(my_hostname);
    }

    bool DnsResolveImpl(const std::string& host) {
      struct addrinfo hints;
      memset(&hints, 0, sizeof hints);
      hints.ai_family = AF_INET;

      struct addrinfo* res = nullptr;

      int result = IsNetworkSpecified()
                       ? AndroidGetAddrInfoForNetwork(net_handle_, host.c_str(),
                                                      nullptr, &hints, &res)
                       : getaddrinfo(host.c_str(), nullptr, &hints, &res);

      if (result != 0) {
        return false;
      }

      net::AddressList address_list = net::AddressList::CreateFromAddrinfo(res);
      results_.push_back(address_list.front().address());
      freeaddrinfo(res);

      return !results_.empty();
    }

    bool DnsResolveExImpl(const std::string& host) {
      struct addrinfo* res = nullptr;

      int result = IsNetworkSpecified()
                       ? AndroidGetAddrInfoForNetwork(net_handle_, host.c_str(),
                                                      nullptr, nullptr, &res)
                       : getaddrinfo(host.c_str(), nullptr, nullptr, &res);

      if (result != 0) {
        return false;
      }

      net::AddressList address_list = net::AddressList::CreateFromAddrinfo(res);
      for (net::IPEndPoint endpoint : address_list.endpoints()) {
        results_.push_back(endpoint.address());
      }
      freeaddrinfo(res);

      return !results_.empty();
    }

    std::string GetHostName() {
      char buffer[HOST_NAME_MAX + 1];

      if (gethostname(buffer, HOST_NAME_MAX + 1) != 0) {
        return std::string();
      }

      // It's unspecified whether gethostname NULL-terminates if the hostname
      // must be truncated and no error is returned if that happens.
      buffer[HOST_NAME_MAX] = '\0';
      return std::string(buffer);
    }

    const std::string hostname_;
    const net::ProxyResolveDnsOperation operation_;
    net_handle_t net_handle_;

    std::vector<net::IPAddress> results_;
    std::vector<net::IPAddress> link_addresses_;
  };
};

class Bindings : public proxy_resolver::ProxyResolverV8Tracing::Bindings {
 public:
  Bindings(HostResolver* host_resolver) : host_resolver_(host_resolver) {}

  void Alert(const std::u16string& message) override {}

  void OnError(int line_number, const std::u16string& message) override {}

  proxy_resolver::ProxyHostResolver* GetHostResolver() override {
    return host_resolver_;
  }

  net::NetLogWithSource GetNetLogWithSource() override {
    return net::NetLogWithSource();
  }

 private:
  raw_ptr<HostResolver> host_resolver_;
};


// Public methods of AwPacProcessor may be called on multiple threads.
// ProxyResolverV8TracingFactory/ProxyResolverV8Tracing
// expects its public interface to always be called on the same thread with
// Chromium task runner so it can post it back to that thread
// with the result of the queries.
//
// Job and its subclasses wrap queries from public methods of AwPacProcessor,
// post them on a special thread and blocks on WaitableEvent
// until the query is finished. |OnSignal| is passed to
// ProxyResolverV8TracingFactory/ProxyResolverV8Tracing methods.
// This callback is called once the request is processed and,
// it signals WaitableEvent and returns result to the calling thread.
//
// ProxyResolverV8TracingFactory/ProxyResolverV8Tracing behaviour is the
// following: if the corresponding request is destroyed,
// the query is cancelled and the callback is never called.
// That means that we need to signal WaitableEvent to unblock calling thread
// when we cancel Job. We keep track of unfinished Jobs in |jobs_|. This field
// is always accessed on the same thread.
//
// All Jobs must be cancelled prior to destruction of |proxy_resolver_| since
// its destructor asserts there are no pending requests.
class Job {
 public:
  virtual ~Job() = default;

  bool ExecSync() {
    GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&Job::Exec, base::Unretained(this)));
    event_.Wait();
    return net_error_ == net::OK;
  }

  void Exec() {
    processor_->jobs_.insert(this);
    std::move(task_).Run();
  }

  virtual void Cancel() = 0;

  void OnSignal(int net_error) {
    net_error_ = net_error;

    // Both SetProxyScriptJob#request_ and MakeProxyRequestJob#request_
    // have to be destroyed on the same thread on which they are created.
    // If we destroy them before callback is called the request_ is cancelled.
    // Reset them here on the correct thread when the job is already finished
    // so no cancellation occurs.
    Cancel();
  }

  base::OnceClosure task_;
  int net_error_ = net::ERR_ABORTED;
  base::WaitableEvent event_;
  raw_ptr<AwPacProcessor> processor_;
};

class SetProxyScriptJob : public Job {
 public:
  SetProxyScriptJob(AwPacProcessor* processor, std::string script) {
    processor_ = processor;
    task_ = base::BindOnce(
        &AwPacProcessor::SetProxyScriptNative, base::Unretained(processor_),
        &request_, std::move(script),
        base::BindOnce(&SetProxyScriptJob::OnSignal, base::Unretained(this)));
  }

  void Cancel() override {
    processor_->jobs_.erase(this);
    request_.reset();
    event_.Signal();
  }

 private:
  std::unique_ptr<net::ProxyResolverFactory::Request> request_;
};

class MakeProxyRequestJob : public Job {
 public:
  MakeProxyRequestJob(AwPacProcessor* processor, std::string url) {
    processor_ = processor;
    task_ = base::BindOnce(
        &AwPacProcessor::MakeProxyRequestNative, base::Unretained(processor_),
        &request_, std::move(url), &proxy_info_,
        base::BindOnce(&MakeProxyRequestJob::OnSignal, base::Unretained(this)));
  }

  void Cancel() override {
    processor_->jobs_.erase(this);
    request_.reset();
    event_.Signal();
  }

  net::ProxyInfo proxy_info() { return proxy_info_; }

 private:
  net::ProxyInfo proxy_info_;
  std::unique_ptr<net::ProxyResolver::Request> request_;
};

AwPacProcessor::AwPacProcessor() {
  host_resolver_ = std::make_unique<HostResolver>();
}

AwPacProcessor::~AwPacProcessor() {
  base::WaitableEvent event;
  // |proxy_resolver_| must be destroyed on the same thread it is created.
  GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AwPacProcessor::Destroy, base::Unretained(this),
                     base::Unretained(&event)));
  event.Wait();
}

void AwPacProcessor::Destroy(base::WaitableEvent* event) {
  // Cancel all unfinished jobs to unblock calling thread.
  for (Job* job : jobs_) {
    job->Cancel();
  }

  proxy_resolver_.reset();
  event->Signal();
}

void AwPacProcessor::DestroyNative(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  delete this;
}

void AwPacProcessor::SetProxyScriptNative(
    std::unique_ptr<net::ProxyResolverFactory::Request>* request,
    const std::string& script,
    net::CompletionOnceCallback complete) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  GetProxyResolverFactory()->CreateProxyResolverV8Tracing(
      net::PacFileData::FromUTF8(script),
      std::make_unique<Bindings>(host_resolver_.get()), &proxy_resolver_,
      std::move(complete), request);
}

void AwPacProcessor::MakeProxyRequestNative(
    std::unique_ptr<net::ProxyResolver::Request>* request,
    const std::string& url,
    net::ProxyInfo* proxy_info,
    net::CompletionOnceCallback complete) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  if (proxy_resolver_) {
    proxy_resolver_->GetProxyForURL(
        GURL(url), net::NetworkAnonymizationKey(), proxy_info,
        std::move(complete), request,
        std::make_unique<Bindings>(host_resolver_.get()));
  } else {
    std::move(complete).Run(net::ERR_FAILED);
  }
}

bool AwPacProcessor::SetProxyScript(std::string script) {
  SetProxyScriptJob job(this, script);
  return job.ExecSync();
}

jboolean AwPacProcessor::SetProxyScript(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        const JavaParamRef<jstring>& jscript) {
  std::string script = ConvertJavaStringToUTF8(env, jscript);
  return SetProxyScript(script);
}

bool AwPacProcessor::MakeProxyRequest(std::string url, std::string* result) {
  MakeProxyRequestJob job(this, url);
  if (job.ExecSync()) {
    if (job.proxy_info().ContainsMultiProxyChain()) {
      // Multi-proxy chains cannot be represented as a PAC string.
      return false;
    }
    *result = job.proxy_info().ToPacString();
    return true;
  } else {
    return false;
  }
}

ScopedJavaLocalRef<jstring> AwPacProcessor::MakeProxyRequest(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jurl) {
  std::string url = ConvertJavaStringToUTF8(env, jurl);
  std::string result;
  if (MakeProxyRequest(url, &result)) {
    return ConvertUTF8ToJavaString(env, result);
  } else {
    return nullptr;
  }
}

void AwPacProcessor::SetNetworkAndLinkAddresses(
    JNIEnv* env,
    net_handle_t net_handle,
    const base::android::JavaParamRef<jobjectArray>& jlink_addresses) {
  std::vector<std::string> string_link_addresses;
  base::android::AppendJavaStringArrayToStringVector(env, jlink_addresses,
                                                     &string_link_addresses);
  std::vector<net::IPAddress> link_addresses;
  for (const std::string& address : string_link_addresses) {
    net::IPAddress ip_address;
    if (ip_address.AssignFromIPLiteral(address)) {
      link_addresses.push_back(ip_address);
    } else {
      LOG(ERROR) << "Not a supported IP literal: " << address;
    }
  }

  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&HostResolver::SetNetworkAndLinkAddresses,
                                base::Unretained(host_resolver_.get()),
                                net_handle, std::move(link_addresses)));
}

static jlong JNI_AwPacProcessor_CreateNativePacProcessor(JNIEnv* env) {
  AwPacProcessor* processor = new AwPacProcessor();
  return reinterpret_cast<intptr_t>(processor);
}

static void JNI_AwPacProcessor_InitializeEnvironment(JNIEnv* env) {
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("AwPacProcessor");
}

}  // namespace android_webview

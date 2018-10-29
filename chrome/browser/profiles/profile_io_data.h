// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
#define CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/storage_partition_descriptor.h"
#include "chrome/common/buildflags.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/net_buildflags.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job_factory.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/url_request_context_owner.h"

class ChromeNetworkDelegate;
class ChromeURLRequestContextGetter;
class HostContentSettingsMap;
class ProtocolHandlerRegistry;

namespace chromeos {
class CertificateProvider;
}

namespace content_settings {
class CookieSettings;
}

namespace data_reduction_proxy {
class DataReductionProxyIOData;
}

namespace domain_reliability {
class DomainReliabilityMonitor;
}

namespace extensions {
class InfoMap;
}

namespace net {
class CertVerifier;
class ChannelIDService;
class ClientCertStore;
class CookieStore;
class HttpTransactionFactory;
class URLRequestContextBuilder;
class URLRequestJobFactoryImpl;

#if BUILDFLAG(ENABLE_REPORTING)
class NetworkErrorLoggingService;
class ReportingService;
#endif  // BUILDFLAG(ENABLE_REPORTING)
}  // namespace net

namespace network {
class CertVerifierWithTrustAnchors;
}  // namespace network

// Conceptually speaking, the ProfileIOData represents data that lives on the IO
// thread that is owned by a Profile, such as, but not limited to, network
// objects like CookieMonster, HttpTransactionFactory, etc.  Profile owns
// ProfileIOData, but will make sure to delete it on the IO thread (except
// possibly in unit tests where there is no IO thread).
class ProfileIOData {
 public:
  typedef std::vector<scoped_refptr<ChromeURLRequestContextGetter>>
      ChromeURLRequestContextGetterVector;

  virtual ~ProfileIOData();

  static ProfileIOData* FromResourceContext(content::ResourceContext* rc);

  // Returns true if |scheme| is handled in Chrome, or by default handlers in
  // net::URLRequest.
  static bool IsHandledProtocol(const std::string& scheme);

  // Returns true if |url| is handled in Chrome, or by default handlers in
  // net::URLRequest.
  static bool IsHandledURL(const GURL& url);

  // Utility to install additional WebUI handlers into the |job_factory|.
  // Ownership of the handlers is transfered from |protocol_handlers|
  // to the |job_factory|.
  // TODO(mmenke): Remove this, once only AddProtocolHandlersToBuilder is used.
  static void InstallProtocolHandlers(
      net::URLRequestJobFactoryImpl* job_factory,
      content::ProtocolHandlerMap* protocol_handlers);

  // Utility to install additional WebUI handlers into |builder|. Ownership of
  // the handlers is transfered from |protocol_handlers| to |builder|.
  static void AddProtocolHandlersToBuilder(
      net::URLRequestContextBuilder* builder,
      content::ProtocolHandlerMap* protocol_handlers);

  // Sets a global CertVerifier to use when initializing all profiles.
  static void SetCertVerifierForTesting(net::CertVerifier* cert_verifier);

  // Called by Profile.
  content::ResourceContext* GetResourceContext() const;

  // Initializes the ProfileIOData object and primes the RequestContext
  // generation. Must be called prior to any of the Get*() methods other than
  // GetResouceContext or GetMetricsEnabledStateOnIOThread.
  void Init(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) const;

  net::URLRequestContext* GetMainRequestContext() const;
  net::URLRequestContext* GetMediaRequestContext() const;
  virtual net::CookieStore* GetExtensionsCookieStore() const = 0;
  net::URLRequestContext* GetIsolatedAppRequestContext(
      IOThread* io_thread,
      net::URLRequestContext* main_context,
      const StoragePartitionDescriptor& partition_descriptor,
      std::unique_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
          protocol_handler_interceptor,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors,
      network::mojom::NetworkContextRequest network_context_request,
      network::mojom::NetworkContextParamsPtr network_context_params) const;
  net::URLRequestContext* GetIsolatedMediaRequestContext(
      net::URLRequestContext* app_context,
      const StoragePartitionDescriptor& partition_descriptor) const;

  // These are useful when the Chrome layer is called from the content layer
  // with a content::ResourceContext, and they want access to Chrome data for
  // that profile.
  extensions::InfoMap* GetExtensionInfoMap() const;
  content_settings::CookieSettings* GetCookieSettings() const;
  HostContentSettingsMap* GetHostContentSettingsMap() const;

  StringPrefMember* google_services_account_id() const {
    return &google_services_user_account_id_;
  }

  // Gets Sync state, for Dice account consistency.
  bool IsSyncEnabled() const;
  bool SyncHasAuthError() const;

  BooleanPrefMember* safe_browsing_enabled() const {
    return &safe_browsing_enabled_;
  }

  StringListPrefMember* safe_browsing_whitelist_domains() const {
    return &safe_browsing_whitelist_domains_;
  }

  IntegerPrefMember* network_prediction_options() const {
    return &network_prediction_options_;
  }

  signin::AccountConsistencyMethod account_consistency() const {
    return account_consistency_;
  }

#if !defined(OS_CHROMEOS)
  std::string GetSigninScopedDeviceId() const;
#endif

#if defined(OS_CHROMEOS)
  std::string username_hash() const {
    return username_hash_;
  }
#endif

  Profile::ProfileType profile_type() const {
    return profile_type_;
  }

  bool IsOffTheRecord() const;

  BooleanPrefMember* force_google_safesearch() const {
    return &force_google_safesearch_;
  }

  IntegerPrefMember* force_youtube_restrict() const {
    return &force_youtube_restrict_;
  }

  StringPrefMember* allowed_domains_for_apps() const {
    return &allowed_domains_for_apps_;
  }

  IntegerPrefMember* incognito_availibility() const {
    return &incognito_availibility_pref_;
  }

#if BUILDFLAG(ENABLE_PLUGINS)
  BooleanPrefMember* always_open_pdf_externally() const {
    return &always_open_pdf_externally_;
  }
#endif

#if defined(OS_CHROMEOS)
  BooleanPrefMember* account_consistency_mirror_required() const {
    return &account_consistency_mirror_required_pref_;
  }
#endif

  // Initialize the member needed to track the metrics enabled state. This is
  // only to be called on the UI thread.
  void InitializeMetricsEnabledStateOnUIThread();

  // Returns whether or not metrics reporting is enabled in the browser instance
  // on which this profile resides. This is safe for use from the IO thread, and
  // should only be called from there.
  bool GetMetricsEnabledStateOnIOThread() const;

  void set_client_cert_store_factory_for_testing(
      const base::Callback<std::unique_ptr<net::ClientCertStore>()>& factory) {
    client_cert_store_factory_ = factory;
  }

  data_reduction_proxy::DataReductionProxyIOData*
  data_reduction_proxy_io_data() const {
    return data_reduction_proxy_io_data_.get();
  }

  // Returns the predictor service for this Profile. Returns nullptr if there is
  // no Predictor, as is the case with OffTheRecord profiles.
  virtual chrome_browser_net::Predictor* GetPredictor();

  // Get platform ClientCertStore. May return nullptr.
  std::unique_ptr<net::ClientCertStore> CreateClientCertStore();

 protected:
#if defined(OS_CHROMEOS)
  // Defines possible ways in which a profile may use the Chrome OS system
  // token.
  enum class SystemKeySlotUseType {
    // This profile does not use the system key slot.
    kNone,
    // This profile only uses the system key slot for client certiticates.
    kUseForClientAuth,
    // This profile uses the system key slot for client certificates and for
    // certificate management.
    kUseForClientAuthAndCertManagement
  };
#endif

  // A URLRequestContext for media that owns its HTTP factory, to ensure
  // it is deleted.
  class MediaRequestContext : public net::URLRequestContext {
   public:
    // |name| is used to describe this context. Currently there are two kinds of
    // media request context -- main media request context ("main_meda") and
    // isolated app media request context ("isolated_media").
    explicit MediaRequestContext(const char* name);

    void SetHttpTransactionFactory(
        std::unique_ptr<net::HttpTransactionFactory> http_factory);

   private:
    ~MediaRequestContext() override;

    std::unique_ptr<net::HttpTransactionFactory> http_factory_;
  };

  // A URLRequestContext for apps that owns its cookie store and HTTP factory,
  // to ensure they are deleted.
  class AppRequestContext : public net::URLRequestContext {
   public:
    AppRequestContext();

    void SetCookieStore(std::unique_ptr<net::CookieStore> cookie_store);
    void SetChannelIDService(
        std::unique_ptr<net::ChannelIDService> channel_id_service);
    void SetHttpNetworkSession(
        std::unique_ptr<net::HttpNetworkSession> http_network_session);
    void SetHttpTransactionFactory(
        std::unique_ptr<net::HttpTransactionFactory> http_factory);
    void SetJobFactory(std::unique_ptr<net::URLRequestJobFactory> job_factory);
#if BUILDFLAG(ENABLE_REPORTING)
    void SetReportingService(
        std::unique_ptr<net::ReportingService> reporting_service);
    void SetNetworkErrorLoggingService(
        std::unique_ptr<net::NetworkErrorLoggingService>
            network_error_logging_service);
#endif  // BUILDFLAG(ENABLE_REPORTING)

   private:
    ~AppRequestContext() override;

    std::unique_ptr<net::CookieStore> cookie_store_;
    std::unique_ptr<net::ChannelIDService> channel_id_service_;
    std::unique_ptr<net::HttpNetworkSession> http_network_session_;
    std::unique_ptr<net::HttpTransactionFactory> http_factory_;
    std::unique_ptr<net::URLRequestJobFactory> job_factory_;
#if BUILDFLAG(ENABLE_REPORTING)
    std::unique_ptr<net::ReportingService> reporting_service_;
    std::unique_ptr<net::NetworkErrorLoggingService>
        network_error_logging_service_;
#endif  // BUILDFLAG(ENABLE_REPORTING)
  };

  // Created on the UI thread, read on the IO thread during ProfileIOData lazy
  // initialization.
  struct ProfileParams {
    ProfileParams();
    ~ProfileParams();

    base::FilePath path;
    IOThread* io_thread = nullptr;

    // Used to configure the main URLRequestContext through the IOThread's
    // in-process network service.
    network::mojom::NetworkContextRequest main_network_context_request;
    network::mojom::NetworkContextParamsPtr main_network_context_params;

    scoped_refptr<content_settings::CookieSettings> cookie_settings;
    scoped_refptr<HostContentSettingsMap> host_content_settings_map;
#if BUILDFLAG(ENABLE_EXTENSIONS)
    scoped_refptr<extensions::InfoMap> extension_info_map;
#endif
    signin::AccountConsistencyMethod account_consistency =
        signin::AccountConsistencyMethod::kDisabled;

    // This pointer exists only as a means of conveying a url job factory
    // pointer from the protocol handler registry on the UI thread to the
    // the URLRequestContext on the IO thread. The consumer MUST take
    // ownership of the object by calling release() on this pointer.
    std::unique_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
        protocol_handler_interceptor;

    // Holds the URLRequestInterceptor pointer that is created on the UI thread
    // and then passed to the list of request_interceptors on the IO thread.
    std::unique_ptr<net::URLRequestInterceptor> new_tab_page_interceptor;

#if defined(OS_CHROMEOS)
    std::unique_ptr<network::CertVerifierWithTrustAnchors> policy_cert_verifier;
    std::string username_hash;
    SystemKeySlotUseType system_key_slot_use_type = SystemKeySlotUseType::kNone;
    std::unique_ptr<chromeos::CertificateProvider> certificate_provider;
#endif

    // The profile this struct was populated from. It's passed as a void* to
    // ensure it's not accidently used on the IO thread. Before using it on the
    // UI thread, call ProfileManager::IsValidProfile to ensure it's alive.
    void* profile = nullptr;
  };

  explicit ProfileIOData(Profile::ProfileType profile_type);

  void InitializeOnUIThread(Profile* profile);

  // Does common setup of the URLRequestJobFactories. Adds default
  // ProtocolHandlers to |job_factory|, adds URLRequestInterceptors in front of
  // it as needed, and returns the result.
  //
  // |protocol_handler_interceptor| is configured to intercept URLRequests
  //     before all other URLRequestInterceptors, if non-null.
  // |host_resolver| is needed to set up the FtpProtocolHandler.
  //
  // TODO(mmenke): Remove this once all URLRequestContexts are set up using
  // URLRequestContextBuilders.
  std::unique_ptr<net::URLRequestJobFactory> SetUpJobFactoryDefaults(
      std::unique_ptr<net::URLRequestJobFactoryImpl> job_factory,
      content::URLRequestInterceptorScopedVector request_interceptors,
      std::unique_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
          protocol_handler_interceptor,
      net::NetworkDelegate* network_delegate,
      net::HostResolver* host_resolver) const;

  // Does common setup of the URLRequestJobFactories. Adds
  // |request_interceptors| and some default ProtocolHandlers to |builder|, adds
  // URLRequestInterceptors in front of them as needed.
  //
  // Unlike SetUpJobFactoryDefaults, leaves configuring data, file, and ftp
  // support to the ProfileNetworkContextService.
  //
  // |protocol_handler_interceptor| is configured to intercept URLRequests
  //     before all other URLRequestInterceptors, if non-null.
  void SetUpJobFactoryDefaultsForBuilder(
      net::URLRequestContextBuilder* builder,
      content::URLRequestInterceptorScopedVector request_interceptors,
      std::unique_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
          protocol_handler_interceptor) const;

  // Called when the Profile is destroyed. |context_getters| must include all
  // URLRequestContextGetters that refer to the ProfileIOData's
  // URLRequestContexts. Triggers destruction of the ProfileIOData and shuts
  // down |context_getters| safely on the IO thread.
  // TODO(mmenke):  Passing all those URLRequestContextGetters around like this
  //     is really silly.  Can we do something cleaner?
  void ShutdownOnUIThread(
      std::unique_ptr<ChromeURLRequestContextGetterVector> context_getters);

  void set_data_reduction_proxy_io_data(
      std::unique_ptr<data_reduction_proxy::DataReductionProxyIOData>
          data_reduction_proxy_io_data) const;

  net::URLRequestContext* main_request_context() const {
    return main_request_context_;
  }

  bool initialized() const {
    return initialized_;
  }

  // Destroys the ResourceContext first, to cancel any URLRequests that are
  // using it still, before we destroy the member variables that those
  // URLRequests may be accessing.
  void DestroyResourceContext();

  // Creates network transaction factory. The created factory will share
  // HttpNetworkSession with |main_http_factory|.
  std::unique_ptr<net::HttpCache> CreateHttpFactory(
      net::HttpTransactionFactory* main_http_factory,
      std::unique_ptr<net::HttpCache::BackendFactory> backend) const;

  // Deletes the media cache at the specified path if the media cache is
  // disabled.
  static void MaybeDeleteMediaCache(const base::FilePath& media_cache_path);

 private:
  class ResourceContext : public content::ResourceContext {
   public:
    explicit ResourceContext(ProfileIOData* io_data);
    ~ResourceContext() override;

    // ResourceContext implementation:
    net::URLRequestContext* GetRequestContext() override;

   private:
    friend class ProfileIOData;

    ProfileIOData* const io_data_;

    net::URLRequestContext* request_context_;
  };

  typedef std::map<StoragePartitionDescriptor,
                   net::URLRequestContext*,
                   StoragePartitionDescriptorLess>
      URLRequestContextMap;

  // --------------------------------------------
  // Virtual interface for subtypes to implement:
  // --------------------------------------------

  // Does any necessary additional configuration of the network delegate,
  // including composing it with other NetworkDelegates, if needed. By default,
  // just returns the input NetworkDelegate.
  virtual std::unique_ptr<net::NetworkDelegate> ConfigureNetworkDelegate(
      IOThread* io_thread,
      std::unique_ptr<ChromeNetworkDelegate> chrome_network_delegate) const;

  // Does the initialization of the URLRequestContextBuilder for a ProfileIOData
  // subclass. Subclasseses should use the static helper functions above to
  // implement this.
  virtual void InitializeInternal(
      net::URLRequestContextBuilder* builder,
      ProfileParams* profile_params,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors)
      const = 0;

  // Called after the main URLRequestContext has been initialized, just after
  // InitializeInternal().
  virtual void OnMainRequestContextCreated(
      ProfileParams* profile_params) const = 0;

  // Initializes the cookie store for extensions.
  virtual void InitializeExtensionsCookieStore(
      ProfileParams* profile_params) const = 0;

  // Does an on-demand initialization of a media RequestContext for the given
  // isolated app.
  virtual net::URLRequestContext* InitializeMediaRequestContext(
      net::URLRequestContext* original_context,
      const StoragePartitionDescriptor& details,
      const char* name) const = 0;

  // These functions are used to transfer ownership of the lazily initialized
  // context from ProfileIOData to the URLRequestContextGetter.
  virtual net::URLRequestContext*
      AcquireMediaRequestContext() const = 0;
  virtual net::URLRequestContext*
      AcquireIsolatedMediaRequestContext(
          net::URLRequestContext* app_context,
          const StoragePartitionDescriptor& partition_descriptor) const = 0;

  // The order *DOES* matter for the majority of these member variables, so
  // don't move them around unless you know what you're doing!
  // General rules:
  //   * ResourceContext references the URLRequestContexts, so
  //   URLRequestContexts must outlive ResourceContext, hence ResourceContext
  //   should be destroyed first.
  //   * URLRequestContexts reference a whole bunch of members, so
  //   URLRequestContext needs to be destroyed before them.
  //   * Therefore, ResourceContext should be listed last, and then the
  //   URLRequestContexts, and then the URLRequestContext members.
  //   * Note that URLRequestContext members have a directed dependency graph
  //   too, so they must themselves be ordered correctly.

  // Tracks whether or not we've been lazily initialized.
  mutable bool initialized_;

  // Data from the UI thread from the Profile, used to initialize ProfileIOData.
  // Deleted after lazy initialization.
  mutable std::unique_ptr<ProfileParams> profile_params_;

  // Used for testing.
  mutable base::Callback<std::unique_ptr<net::ClientCertStore>()>
      client_cert_store_factory_;

  mutable StringPrefMember google_services_user_account_id_;
  mutable BooleanPrefMember sync_has_auth_error_;
  mutable BooleanPrefMember sync_suppress_start_;
  mutable BooleanPrefMember sync_first_setup_complete_;
  mutable signin::AccountConsistencyMethod account_consistency_;

#if !defined(OS_CHROMEOS)
  mutable StringPrefMember signin_scoped_device_id_;
#endif

  // Member variables which are pointed to by the various context objects.
  mutable BooleanPrefMember force_google_safesearch_;
  mutable IntegerPrefMember force_youtube_restrict_;
  mutable BooleanPrefMember safe_browsing_enabled_;
  mutable StringListPrefMember safe_browsing_whitelist_domains_;
  mutable StringPrefMember allowed_domains_for_apps_;
  mutable IntegerPrefMember network_prediction_options_;
  mutable IntegerPrefMember incognito_availibility_pref_;
#if BUILDFLAG(ENABLE_PLUGINS)
  mutable BooleanPrefMember always_open_pdf_externally_;
#endif
#if defined(OS_CHROMEOS)
  mutable BooleanPrefMember account_consistency_mirror_required_pref_;
#endif

  BooleanPrefMember enable_metrics_;

  // Pointed to by URLRequestContext.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  mutable scoped_refptr<extensions::InfoMap> extension_info_map_;
#endif

  mutable std::unique_ptr<data_reduction_proxy::DataReductionProxyIOData>
      data_reduction_proxy_io_data_;

#if defined(OS_CHROMEOS)
  mutable std::string username_hash_;
  mutable SystemKeySlotUseType system_key_slot_use_type_;
  mutable std::unique_ptr<chromeos::CertificateProvider> certificate_provider_;
#endif

  // When the network service is disabled, this holds on to a
  // content::NetworkContext class that owns |main_request_context_|.
  mutable std::unique_ptr<network::mojom::NetworkContext> main_network_context_;
  // When the network service is disabled, this holds onto all the
  // NetworkContexts pointed at by the |app_request_context_map_|.
  mutable std::list<std::unique_ptr<network::mojom::NetworkContext>>
      app_network_contexts_;
  // When the network service is disabled, this owns |system_request_context|.
  mutable network::URLRequestContextOwner main_request_context_owner_;
  mutable net::URLRequestContext* main_request_context_;

  // One URLRequestContext per isolated app for main and media requests.
  mutable URLRequestContextMap app_request_context_map_;
  mutable URLRequestContextMap isolated_media_request_context_map_;

  mutable std::unique_ptr<ResourceContext> resource_context_;

  mutable scoped_refptr<content_settings::CookieSettings> cookie_settings_;

  mutable scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  // Owned (possibly with one or more layers of LayeredNetworkDelegate) by the
  // URLRequestContext, which is owned by the |main_network_context_|.
  mutable ChromeNetworkDelegate* chrome_network_delegate_unowned_;
  // Owned by |chrome_network_delegate_unowned_|.
  mutable domain_reliability::DomainReliabilityMonitor*
      domain_reliability_monitor_unowned_;

  const Profile::ProfileType profile_type_;

  DISALLOW_COPY_AND_ASSIGN(ProfileIOData);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_

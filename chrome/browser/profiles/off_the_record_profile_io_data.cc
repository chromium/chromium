// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/off_the_record_profile_io_data.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/chrome_url_request_context_getter.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/net_log/chrome_net_log.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/browser/resource_context.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/net_buildflags.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "storage/browser/database/database_tracker.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension.h"
#endif

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/network_error_logging/network_error_logging_delegate.h"
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_service.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

using content::BrowserThread;

OffTheRecordProfileIOData::Handle::Handle(Profile* profile)
    : io_data_(new OffTheRecordProfileIOData(profile->GetProfileType())),
      profile_(profile),
      initialized_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile);
}

OffTheRecordProfileIOData::Handle::~Handle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  io_data_->ShutdownOnUIThread(GetAllContextGetters());
}

content::ResourceContext*
OffTheRecordProfileIOData::Handle::GetResourceContext() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize();
  return GetResourceContextNoInit();
}

content::ResourceContext*
OffTheRecordProfileIOData::Handle::GetResourceContextNoInit() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Don't call LazyInitialize here, since the resource context is created at
  // the beginning of initalization and is used by some members while they're
  // being initialized (i.e. AppCacheService).
  return io_data_->GetResourceContext();
}

scoped_refptr<ChromeURLRequestContextGetter>
OffTheRecordProfileIOData::Handle::CreateMainRequestContextGetter(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) const {
  // TODO(oshima): Re-enable when ChromeOS only accesses the profile on the UI
  // thread.
#if !defined(OS_CHROMEOS)
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#endif  // defined(OS_CHROMEOS)
  LazyInitialize();
  DCHECK(!main_request_context_getter_.get());
  main_request_context_getter_ = ChromeURLRequestContextGetter::Create(
      profile_, io_data_, protocol_handlers, std::move(request_interceptors));
  return main_request_context_getter_;
}

scoped_refptr<ChromeURLRequestContextGetter>
OffTheRecordProfileIOData::Handle::GetIsolatedAppRequestContextGetter(
    const base::FilePath& partition_path,
    bool in_memory) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!partition_path.empty());
  LazyInitialize();

  // Keep a map of request context getters, one per requested app ID.
  StoragePartitionDescriptor descriptor(partition_path, in_memory);
  auto iter = app_request_context_getter_map_.find(descriptor);
  CHECK(iter != app_request_context_getter_map_.end());
  return iter->second;
}

scoped_refptr<ChromeURLRequestContextGetter>
OffTheRecordProfileIOData::Handle::CreateIsolatedAppRequestContextGetter(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!partition_path.empty());
  LazyInitialize();

  // Keep a map of request context getters, one per requested app ID.
  StoragePartitionDescriptor descriptor(partition_path, in_memory);
  DCHECK_EQ(app_request_context_getter_map_.count(descriptor), 0u);

  std::unique_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
      protocol_handler_interceptor(
          ProtocolHandlerRegistryFactory::GetForBrowserContext(profile_)
              ->CreateJobInterceptorFactory());
  base::FilePath relative_partition_path;
  // This method is passed the absolute partition path, but
  // ProfileNetworkContext works in terms of relative partition paths.
  bool result = profile_->GetPath().AppendRelativePath(
      partition_path, &relative_partition_path);
  DCHECK(result);
  network::mojom::NetworkContextRequest network_context_request;
  network::mojom::NetworkContextParamsPtr network_context_params;
  ProfileNetworkContextServiceFactory::GetForContext(profile_)
      ->SetUpProfileIODataNetworkContext(in_memory, relative_partition_path,
                                         &network_context_request,
                                         &network_context_params);
  scoped_refptr<ChromeURLRequestContextGetter> context =
      ChromeURLRequestContextGetter::CreateForIsolatedApp(
          profile_, io_data_, descriptor,
          std::move(protocol_handler_interceptor), protocol_handlers,
          std::move(request_interceptors), std::move(network_context_request),
          std::move(network_context_params));
  app_request_context_getter_map_[descriptor] = context;

  return context;
}

void OffTheRecordProfileIOData::Handle::LazyInitialize() const {
  if (initialized_)
    return;

  // Set initialized_ to true at the beginning in case any of the objects
  // below try to get the ResourceContext pointer.
  initialized_ = true;
  io_data_->safe_browsing_enabled()->Init(prefs::kSafeBrowsingEnabled,
      profile_->GetPrefs());
  io_data_->safe_browsing_enabled()->MoveToThread(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
  io_data_->safe_browsing_whitelist_domains()->Init(
      prefs::kSafeBrowsingWhitelistDomains, profile_->GetPrefs());
  io_data_->safe_browsing_whitelist_domains()->MoveToThread(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
#if BUILDFLAG(ENABLE_PLUGINS)
  io_data_->always_open_pdf_externally()->Init(
      prefs::kPluginsAlwaysOpenPdfExternally, profile_->GetPrefs());
  io_data_->always_open_pdf_externally()->MoveToThread(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
#endif
  io_data_->InitializeOnUIThread(profile_);
}

std::unique_ptr<ProfileIOData::ChromeURLRequestContextGetterVector>
OffTheRecordProfileIOData::Handle::GetAllContextGetters() {
  std::unique_ptr<ChromeURLRequestContextGetterVector> context_getters(
      new ChromeURLRequestContextGetterVector());
  auto iter = app_request_context_getter_map_.begin();
  for (; iter != app_request_context_getter_map_.end(); ++iter)
    context_getters->push_back(iter->second);

  if (main_request_context_getter_.get())
    context_getters->push_back(main_request_context_getter_);

  return context_getters;
}

OffTheRecordProfileIOData::OffTheRecordProfileIOData(
    Profile::ProfileType profile_type)
    : ProfileIOData(profile_type) {}

OffTheRecordProfileIOData::~OffTheRecordProfileIOData() {
  DestroyResourceContext();
}

void OffTheRecordProfileIOData::InitializeInternal(
    net::URLRequestContextBuilder* builder,
    ProfileParams* profile_params,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) const {
  // For incognito, we use a non-persistent channel ID store.
  std::unique_ptr<net::ChannelIDService> channel_id_service(
      std::make_unique<net::ChannelIDService>(
          new net::DefaultChannelIDStore(nullptr)));

  AddProtocolHandlersToBuilder(builder, protocol_handlers);
  SetUpJobFactoryDefaultsForBuilder(
      builder, std::move(request_interceptors),
      std::move(profile_params->protocol_handler_interceptor));
}

void OffTheRecordProfileIOData::OnMainRequestContextCreated(
    ProfileParams* profile_params) const {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  InitializeExtensionsCookieStore(profile_params);
#endif
}

void OffTheRecordProfileIOData::InitializeExtensionsCookieStore(
    ProfileParams* profile_params) const {
  content::CookieStoreConfig cookie_config;
  // Enable cookies for chrome-extension URLs.
  cookie_config.cookieable_schemes.push_back(extensions::kExtensionScheme);
  extensions_cookie_store_ = content::CreateCookieStore(
      cookie_config, profile_params->io_thread->net_log());
}

net::URLRequestContext*
OffTheRecordProfileIOData::InitializeMediaRequestContext(
    net::URLRequestContext* original_context,
    const StoragePartitionDescriptor& partition_descriptor,
    const char* name) const {
  NOTREACHED();
  return NULL;
}

net::URLRequestContext*
OffTheRecordProfileIOData::AcquireMediaRequestContext() const {
  NOTREACHED();
  return NULL;
}

net::URLRequestContext*
OffTheRecordProfileIOData::AcquireIsolatedMediaRequestContext(
    net::URLRequestContext* app_context,
    const StoragePartitionDescriptor& partition_descriptor) const {
  NOTREACHED();
  return NULL;
}

net::CookieStore* OffTheRecordProfileIOData::GetExtensionsCookieStore() const {
  return extensions_cookie_store_.get();
}

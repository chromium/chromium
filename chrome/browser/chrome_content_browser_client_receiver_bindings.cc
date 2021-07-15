// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exposes services from the browser to child processes.

#include "chrome/browser/chrome_content_browser_client.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cache_stats_recorder.h"
#include "chrome/browser/chrome_browser_interface_binders.h"
#include "chrome/browser/chrome_content_browser_client_parts.h"
#include "chrome/browser/content_settings/content_settings_manager_delegate.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/net_benchmarking.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/content_capture/browser/onscreen_content_provider.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/metrics/call_stack_profile_collector.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/mojo_safe_browsing_impl.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/service_worker_version_base_info.h"
#include "media/mojo/buildflags.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/widevine/cdm/buildflags.h"

#if defined(OS_ANDROID)
#include "chrome/browser/download/android/available_offline_content_provider.h"
#elif defined(OS_WIN)
#include "chrome/browser/win/conflicts/module_database.h"
#include "chrome/browser/win/conflicts/module_event_sink_impl.h"
#elif BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/performance_manager/mechanisms/userspace_swap_chromeos.h"
#include "chromeos/components/cdm_factory_daemon/cdm_factory_daemon_proxy.h"
#include "components/performance_manager/public/performance_manager.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/chrome_extensions_browser_client.h"
#include "extensions/browser/extensions_browser_client.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS) || defined(OS_WIN)
#include "chrome/browser/media/cdm_document_service_impl.h"
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS) || defined(OS_WIN)

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "chrome/browser/media/output_protection_impl.h"
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(ENABLE_MOJO_CDM) && defined(OS_ANDROID)
#include "chrome/browser/media/android/cdm/media_drm_storage_factory.h"
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "chrome/browser/spellchecker/spell_check_host_chrome_impl.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#if BUILDFLAG(HAS_SPELLCHECK_PANEL)
#include "chrome/browser/spellchecker/spell_check_panel_host_impl.h"
#endif
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/badging/badge_manager.h"
#endif

namespace {

// Helper method for ExposeInterfacesToRenderer() that checks the latest
// SafeBrowsing pref value on the UI thread before hopping over to the IO
// thread.
void MaybeCreateSafeBrowsingForRenderer(
    int process_id,
    content::ResourceContext* resource_context,
    base::RepeatingCallback<scoped_refptr<safe_browsing::UrlCheckerDelegate>(
        bool safe_browsing_enabled,
        bool should_check_on_sb_disabled,
        const std::vector<std::string>& allowlist_domains)>
        get_checker_delegate,
    mojo::PendingReceiver<safe_browsing::mojom::SafeBrowsing> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(process_id);
  if (!render_process_host)
    return;

  PrefService* pref_service =
      Profile::FromBrowserContext(render_process_host->GetBrowserContext())
          ->GetPrefs();

  std::vector<std::string> allowlist_domains =
      safe_browsing::GetURLAllowlistByPolicy(pref_service);

  // Log the size of the domains to make sure copying them is
  // not too expensive.
  if (allowlist_domains.size() > 0) {
    int total_size = 0;
    for (const auto& domains : allowlist_domains) {
      total_size += domains.size();
    }
    base::UmaHistogramCounts10000(
        "SafeBrowsing.Policy.AllowlistDomainsTotalSize", total_size);
  }

  bool safe_browsing_enabled =
      safe_browsing::IsSafeBrowsingEnabled(*pref_service);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &safe_browsing::MojoSafeBrowsingImpl::MaybeCreate, process_id,
          resource_context,
          base::BindRepeating(get_checker_delegate, safe_browsing_enabled,
                              // Navigation initiated from renderer should never
                              // check when safe browsing is disabled, because
                              // enterprise check only supports mainframe URL.
                              /*should_check_on_sb_disabled=*/false,
                              allowlist_domains),
          std::move(receiver)));
}

// BadgeManager is not used for Android.
#if !defined(OS_ANDROID)
void BindBadgeServiceForServiceWorker(
    const content::ServiceWorkerVersionBaseInfo& info,
    mojo::PendingReceiver<blink::mojom::BadgeService> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(info.process_id);
  if (!render_process_host)
    return;

  badging::BadgeManager::BindServiceWorkerReceiver(
      render_process_host, info.scope, std::move(receiver));
}
#endif

}  // namespace

void ChromeContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* render_process_host) {
  // The CacheStatsRecorder is an associated binding, instead of a
  // non-associated one, because the sender (in the renderer process) posts the
  // message after a time delay, in order to rate limit. The association
  // protects against the render process host ID being recycled in that time
  // gap between the preparation and the execution of that IPC.
  associated_registry->AddInterface(base::BindRepeating(
      &CacheStatsRecorder::Create, render_process_host->GetID()));

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner =
      content::GetUIThreadTaskRunner({});
  registry->AddInterface(
      base::BindRepeating(&metrics::CallStackProfileCollector::Create));

  if (NetBenchmarking::CheckBenchmarkingEnabled()) {
    Profile* profile =
        Profile::FromBrowserContext(render_process_host->GetBrowserContext());
    auto* loading_predictor =
        predictors::LoadingPredictorFactory::GetForProfile(profile);
    registry->AddInterface(
        base::BindRepeating(
            &NetBenchmarking::Create,
            loading_predictor ? loading_predictor->GetWeakPtr() : nullptr,
            render_process_host->GetID()),
        ui_task_runner);
  }

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL) || BUILDFLAG(SAFE_BROWSING_DB_REMOTE)
  if (safe_browsing_service_) {
    content::ResourceContext* resource_context =
        render_process_host->GetBrowserContext()->GetResourceContext();
    registry->AddInterface(
        base::BindRepeating(
            &MaybeCreateSafeBrowsingForRenderer, render_process_host->GetID(),
            resource_context,
            base::BindRepeating(
                &ChromeContentBrowserClient::GetSafeBrowsingUrlCheckerDelegate,
                base::Unretained(this))),
        ui_task_runner);
  }
#endif

#if defined(OS_WIN)
  // Add the ModuleEventSink interface. This is the interface used by renderer
  // processes to notify the browser of modules in their address space. The
  // process handle is not yet available at this point so pass in a callback
  // to allow to retrieve a duplicate at the time the interface is actually
  // created.
  auto get_process = base::BindRepeating(
      [](int id) -> base::Process {
        auto* host = content::RenderProcessHost::FromID(id);
        if (host)
          return host->GetProcess().Duplicate();
        return base::Process();
      },
      render_process_host->GetID());
  registry->AddInterface(
      base::BindRepeating(
          &ModuleEventSinkImpl::Create, std::move(get_process),
          content::PROCESS_TYPE_RENDERER,
          base::BindRepeating(&ModuleDatabase::HandleModuleLoadEvent)),
      ui_task_runner);
#endif
#if defined(OS_ANDROID)
  Profile* profile =
      Profile::FromBrowserContext(render_process_host->GetBrowserContext());
  registry->AddInterface(
      base::BindRepeating(&android::AvailableOfflineContentProvider::Create,
                          profile),
      content::GetUIThreadTaskRunner({}));
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (performance_manager::mechanism::userspace_swap::
          UserspaceSwapInitializationImpl::UserspaceSwapSupportedAndEnabled()) {
    registry->AddInterface(
        base::BindRepeating(&performance_manager::mechanism::userspace_swap::
                                UserspaceSwapInitializationImpl::Create,
                            render_process_host->GetID()),
        performance_manager::PerformanceManager::GetTaskRunner());
  }
#endif

  for (auto* ep : extra_parts_) {
    ep->ExposeInterfacesToRenderer(registry, associated_registry,
                                   render_process_host);
  }
}

void ChromeContentBrowserClient::BindMediaServiceReceiver(
    content::RenderFrameHost* render_frame_host,
    mojo::GenericPendingReceiver receiver) {
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  if (auto r = receiver.As<media::mojom::OutputProtection>()) {
    OutputProtectionImpl::Create(render_frame_host, std::move(r));
    return;
  }
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(ENABLE_LIBRARY_CDMS) || defined(OS_WIN)
  if (auto r = receiver.As<media::mojom::CdmDocumentService>()) {
    CdmDocumentServiceImpl::Create(render_frame_host, std::move(r));
    return;
  }
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS) || defined(OS_WIN)

#if BUILDFLAG(ENABLE_MOJO_CDM) && defined(OS_ANDROID)
  if (auto r = receiver.As<media::mojom::MediaDrmStorage>()) {
    CreateMediaDrmStorage(render_frame_host, std::move(r));
    return;
  }
#endif
}

void ChromeContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  chrome::internal::PopulateChromeFrameBinders(map, render_frame_host);
  chrome::internal::PopulateChromeWebUIFrameBinders(map);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  const GURL& site = render_frame_host->GetSiteInstance()->GetSiteURL();
  if (!site.SchemeIs(extensions::kExtensionScheme))
    return;

  content::BrowserContext* browser_context =
      render_frame_host->GetProcess()->GetBrowserContext();
  auto* extension = extensions::ExtensionRegistry::Get(browser_context)
                        ->enabled_extensions()
                        .GetByID(site.host());
  if (!extension)
    return;
  extensions::ExtensionsBrowserClient::Get()
      ->RegisterBrowserInterfaceBindersForFrame(map, render_frame_host,
                                                extension);
#endif
}

void ChromeContentBrowserClient::
    RegisterBrowserInterfaceBindersForServiceWorker(
        mojo::BinderMapWithContext<
            const content::ServiceWorkerVersionBaseInfo&>* map) {
#if !defined(OS_ANDROID)
  map->Add<blink::mojom::BadgeService>(
      base::BindRepeating(&BindBadgeServiceForServiceWorker));
#endif
}

bool ChromeContentBrowserClient::BindAssociatedReceiverFromFrame(
    content::RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle* handle) {
  if (interface_name == autofill::mojom::AutofillDriver::Name_) {
    autofill::ContentAutofillDriverFactory::BindAutofillDriver(
        mojo::PendingAssociatedReceiver<autofill::mojom::AutofillDriver>(
            std::move(*handle)),
        render_frame_host);
    return true;
  }
  if (interface_name == autofill::mojom::PasswordManagerDriver::Name_) {
    password_manager::ContentPasswordManagerDriverFactory::
        BindPasswordManagerDriver(
            mojo::PendingAssociatedReceiver<
                autofill::mojom::PasswordManagerDriver>(std::move(*handle)),
            render_frame_host);
    return true;
  }
  if (interface_name ==
      chrome::mojom::OpenSearchDescriptionDocumentHandler::Name_) {
    SearchEngineTabHelper::BindOpenSearchDescriptionDocumentHandler(
        mojo::PendingAssociatedReceiver<
            chrome::mojom::OpenSearchDescriptionDocumentHandler>(
            std::move(*handle)),
        render_frame_host);
    return true;
  }
  if (interface_name == content_capture::mojom::ContentCaptureReceiver::Name_) {
    content_capture::OnscreenContentProvider::BindContentCaptureReceiver(
        mojo::PendingAssociatedReceiver<
            content_capture::mojom::ContentCaptureReceiver>(std::move(*handle)),
        render_frame_host);
    return true;
  }

  if (interface_name ==
      subresource_filter::mojom::SubresourceFilterHost::Name_) {
    subresource_filter::ContentSubresourceFilterThrottleManager::BindReceiver(
        mojo::PendingAssociatedReceiver<
            subresource_filter::mojom::SubresourceFilterHost>(
            std::move(*handle)),
        render_frame_host);
    return true;
  }

  return false;
}

void ChromeContentBrowserClient::BindGpuHostReceiver(
    mojo::GenericPendingReceiver receiver) {
  if (auto r = receiver.As<metrics::mojom::CallStackProfileCollector>()) {
    metrics::CallStackProfileCollector::Create(std::move(r));
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (auto r = receiver.As<chromeos::cdm::mojom::CdmFactoryDaemon>())
    chromeos::CdmFactoryDaemonProxy::Create(std::move(r));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ChromeContentBrowserClient::BindUtilityHostReceiver(
    mojo::GenericPendingReceiver receiver) {
  if (auto r = receiver.As<metrics::mojom::CallStackProfileCollector>())
    metrics::CallStackProfileCollector::Create(std::move(r));
}

void ChromeContentBrowserClient::BindHostReceiverForRenderer(
    content::RenderProcessHost* render_process_host,
    mojo::GenericPendingReceiver receiver) {
  if (auto host_receiver =
          receiver.As<content_settings::mojom::ContentSettingsManager>()) {
    content_settings::ContentSettingsManagerImpl::Create(
        render_process_host, std::move(host_receiver),
        std::make_unique<chrome::ContentSettingsManagerDelegate>());
    return;
  }

#if BUILDFLAG(ENABLE_SPELLCHECK)
  if (auto host_receiver = receiver.As<spellcheck::mojom::SpellCheckHost>()) {
    SpellCheckHostChromeImpl::Create(render_process_host->GetID(),
                                     std::move(host_receiver));
    return;
  }

#if BUILDFLAG(HAS_SPELLCHECK_PANEL)
  if (auto host_receiver =
          receiver.As<spellcheck::mojom::SpellCheckPanelHost>()) {
    SpellCheckPanelHostImpl::Create(render_process_host->GetID(),
                                    std::move(host_receiver));
    return;
  }
#endif  // BUILDFLAG(HAS_SPELLCHECK_PANEL)
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#if BUILDFLAG(ENABLE_PLUGINS)
  if (auto host_receiver = receiver.As<chrome::mojom::MetricsService>()) {
    ChromeMetricsServiceAccessor::BindMetricsServiceReceiver(
        std::move(host_receiver));
  }
#endif  // BUILDFLAG(ENABLE_PLUGINS)
}

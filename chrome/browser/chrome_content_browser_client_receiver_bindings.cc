// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exposes services from the browser to child processes.

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_interface_binders.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_content_browser_client_parts.h"
#include "chrome/browser/content_settings/content_settings_manager_delegate.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/net_benchmarking.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_web_request_reporter_impl.h"
#include "chrome/browser/signin/google_accounts_private_api_host.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/trusted_vault/trusted_vault_encryption_keys_tab_helper.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/content_capture/browser/onscreen_content_provider.h"
#include "components/fingerprinting_protection_filter/browser/throttle_manager.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/fingerprinting_protection_filter/mojom/fingerprinting_protection_filter.mojom.h"
#include "components/metrics/call_stacks/call_stack_profile_collector.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/mojo_safe_browsing_impl.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_version_base_info.h"
#include "media/mojo/buildflags.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "pdf/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/widevine/cdm/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/download/android/available_offline_content_provider.h"
#include "chrome/browser/plugins/plugin_observer_android.h"
#elif BUILDFLAG(IS_WIN)
#include "chrome/browser/win/conflicts/module_database.h"
#include "chrome/browser/win/conflicts/module_event_sink_impl.h"
#elif BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/mojo_service_manager/utility_process_bridge.h"
#include "chromeos/components/cdm_factory_daemon/cdm_factory_daemon_proxy_ash.h"
#include "components/performance_manager/public/performance_manager.h"
#if defined(ARCH_CPU_X86_64)
#include "chrome/browser/performance_manager/mechanisms/userspace_swap_chromeos.h"
#endif  // defined(ARCH_CPU_X86_64)
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/components/cdm_factory_daemon/cdm_factory_daemon_proxy_lacros.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_cros.h"
#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_cros_basic.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/extensions_browser_client.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS) || BUILDFLAG(IS_WIN)
#include "chrome/browser/media/cdm_document_service_impl.h"
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "chrome/browser/media/output_protection_impl.h"
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(ENABLE_MOJO_CDM) && BUILDFLAG(IS_ANDROID)
#include "chrome/browser/media/android/cdm/media_drm_storage_factory.h"
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "chrome/browser/spellchecker/spell_check_host_chrome_impl.h"
#include "chrome/browser/spellchecker/spell_check_initialization_host_impl.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#if BUILDFLAG(HAS_SPELLCHECK_PANEL)
#include "chrome/browser/spellchecker/spell_check_panel_host_impl.h"
#endif
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/ui/pdf/chrome_pdf_document_helper_client.h"
#include "components/pdf/browser/pdf_document_helper.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/print_view_manager_basic.h"
#include "components/printing/browser/headless/headless_print_manager.h"
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_view_manager.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // BUILDFLAG(ENABLE_PRINTING)

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/guest_view/web_view/chrome_web_view_permission_helper_delegate.h"
#include "chrome/browser/plugins/plugin_observer.h"
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#endif

namespace {

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
// Helper method for ExposeInterfacesToRenderer() that checks the latest
// SafeBrowsing pref value on the UI thread before hopping over to the IO
// thread.
void MaybeCreateSafeBrowsingForRenderer(
    int process_id,
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

  bool safe_browsing_enabled =
      safe_browsing::IsSafeBrowsingEnabled(*pref_service);

  safe_browsing::MojoSafeBrowsingImpl::MaybeCreate(
      process_id,
      base::BindRepeating(get_checker_delegate, safe_browsing_enabled,
                          // Navigation initiated from renderer should never
                          // check when safe browsing is disabled, because
                          // enterprise check only supports mainframe URL.
                          /*should_check_on_sb_disabled=*/false,
                          allowlist_domains),
      std::move(receiver));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void MaybeCreateExtensionWebRequestReporterForRenderer(
    int process_id,
    mojo::PendingReceiver<safe_browsing::mojom::ExtensionWebRequestReporter>
        receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(process_id);
  if (!render_process_host) {
    return;
  }

  safe_browsing::ExtensionWebRequestReporterImpl::Create(render_process_host,
                                                         std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

// BadgeManager is not used for Android.
#if !BUILDFLAG(IS_ANDROID)
void BindBadgeServiceForServiceWorker(
    const content::ServiceWorkerVersionBaseInfo& info,
    mojo::PendingReceiver<blink::mojom::BadgeService> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(info.process_id);
  if (!render_process_host)
    return;

  badging::BadgeManager::BindServiceWorkerReceiverIfAllowed(
      render_process_host, info, std::move(receiver));
}
#endif

}  // namespace

void ChromeContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* render_process_host) {
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner =
      content::GetUIThreadTaskRunner({});
  registry->AddInterface<metrics::mojom::CallStackProfileCollector>(
      base::BindRepeating(&metrics::CallStackProfileCollector::Create));

  if (NetBenchmarking::CheckBenchmarkingEnabled()) {
    Profile* profile =
        Profile::FromBrowserContext(render_process_host->GetBrowserContext());
    auto* loading_predictor =
        predictors::LoadingPredictorFactory::GetForProfile(profile);
    registry->AddInterface<chrome::mojom::NetBenchmarking>(
        base::BindRepeating(
            &NetBenchmarking::Create,
            loading_predictor ? loading_predictor->GetWeakPtr() : nullptr,
            render_process_host->GetID()),
        ui_task_runner);
  }

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  if (safe_browsing_service_) {
    registry->AddInterface<safe_browsing::mojom::SafeBrowsing>(
        base::BindRepeating(
            &MaybeCreateSafeBrowsingForRenderer, render_process_host->GetID(),
            base::BindRepeating(
                &ChromeContentBrowserClient::GetSafeBrowsingUrlCheckerDelegate,
                base::Unretained(this))),
        ui_task_runner);
#if BUILDFLAG(ENABLE_EXTENSIONS)
    registry->AddInterface<safe_browsing::mojom::ExtensionWebRequestReporter>(
        base::BindRepeating(&MaybeCreateExtensionWebRequestReporterForRenderer,
                            render_process_host->GetID()),
        ui_task_runner);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  }
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

#if BUILDFLAG(IS_WIN)
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
  registry->AddInterface<mojom::ModuleEventSink>(
      base::BindRepeating(
          &ModuleEventSinkImpl::Create, std::move(get_process),
          content::PROCESS_TYPE_RENDERER,
          base::BindRepeating(&ModuleDatabase::HandleModuleLoadEvent)),
      ui_task_runner);
#endif
#if BUILDFLAG(IS_ANDROID)
  registry->AddInterface<chrome::mojom::AvailableOfflineContentProvider>(
      base::BindRepeating(&android::AvailableOfflineContentProvider::Create,
                          render_process_host->GetID()),
      content::GetUIThreadTaskRunner({}));
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(ARCH_CPU_X86_64)
  if (performance_manager::mechanism::userspace_swap::
          UserspaceSwapInitializationImpl::UserspaceSwapSupportedAndEnabled()) {
    registry
        ->AddInterface<::userspace_swap::mojom::UserspaceSwapInitialization>(
            base::BindRepeating(
                &performance_manager::mechanism::userspace_swap::
                    UserspaceSwapInitializationImpl::Create,
                render_process_host->GetID()),
            performance_manager::PerformanceManager::GetTaskRunner());
  }
#endif  // defined(ARCH_CPU_X86_64)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  for (auto& ep : extra_parts_) {
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

#if BUILDFLAG(ENABLE_LIBRARY_CDMS) || BUILDFLAG(IS_WIN)
  if (auto r = receiver.As<media::mojom::CdmDocumentService>()) {
    CdmDocumentServiceImpl::Create(render_frame_host, std::move(r));
    return;
  }
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_MOJO_CDM) && BUILDFLAG(IS_ANDROID)
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
  chrome::internal::PopulateChromeWebUIFrameBinders(map, render_frame_host);

#if BUILDFLAG(ENABLE_SPELLCHECK)
  map->Add<spellcheck::mojom::SpellCheckHost>(base::BindRepeating(
      [](content::RenderFrameHost* frame_host,
         mojo::PendingReceiver<spellcheck::mojom::SpellCheckHost> receiver) {
        SpellCheckHostChromeImpl::Create(frame_host->GetProcess()->GetID(),
                                         std::move(receiver));
      }));
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
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

void ChromeContentBrowserClient::RegisterWebUIInterfaceBrokers(
    content::WebUIBrowserInterfaceBrokerRegistry& registry) {
  chrome::internal::PopulateChromeWebUIFrameInterfaceBrokers(registry);
}

void ChromeContentBrowserClient::
    RegisterBrowserInterfaceBindersForServiceWorker(
        content::BrowserContext* browser_context,
        const content::ServiceWorkerVersionBaseInfo&
            service_worker_version_info,
        mojo::BinderMapWithContext<
            const content::ServiceWorkerVersionBaseInfo&>* map) {
#if !BUILDFLAG(IS_ANDROID)
  map->Add<blink::mojom::BadgeService>(
      base::BindRepeating(&BindBadgeServiceForServiceWorker));
#endif
}

void ChromeContentBrowserClient::
    RegisterAssociatedInterfaceBindersForServiceWorker(
        const content::ServiceWorkerVersionBaseInfo&
            service_worker_version_info,
        blink::AssociatedInterfaceRegistry& associated_registry) {
  for (auto& ep : extra_parts_) {
    ep->ExposeInterfacesToRendererForServiceWorker(service_worker_version_info,
                                                   associated_registry);
  }
}

void ChromeContentBrowserClient::
    RegisterAssociatedInterfaceBindersForRenderFrameHost(
        content::RenderFrameHost& render_frame_host,
        blink::AssociatedInterfaceRegistry& associated_registry) {
  for (auto& ep : extra_parts_) {
    ep->ExposeInterfacesToRendererForRenderFrameHost(render_frame_host,
                                                     associated_registry);
  }

  associated_registry.AddInterface<autofill::mojom::AutofillDriver>(
      base::BindRepeating(
          &autofill::ContentAutofillDriverFactory::BindAutofillDriver,
          &render_frame_host));
  associated_registry.AddInterface<autofill::mojom::PasswordGenerationDriver>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<
                 autofill::mojom::PasswordGenerationDriver> receiver) {
            ChromePasswordManagerClient::BindPasswordGenerationDriver(
                std::move(receiver), render_frame_host);
          },
          &render_frame_host));
  associated_registry.AddInterface<
      autofill::mojom::PasswordManagerDriver>(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<autofill::mojom::PasswordManagerDriver>
             receiver) {
        password_manager::ContentPasswordManagerDriverFactory::
            BindPasswordManagerDriver(std::move(receiver), render_frame_host);
      },
      &render_frame_host));
  associated_registry.AddInterface<chrome::mojom::NetworkDiagnostics>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<chrome::mojom::NetworkDiagnostics>
                 receiver) {
            chrome_browser_net::NetErrorTabHelper::BindNetworkDiagnostics(
                std::move(receiver), render_frame_host);
          },
          &render_frame_host));
  associated_registry.AddInterface<chrome::mojom::NetworkEasterEgg>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<chrome::mojom::NetworkEasterEgg>
                 receiver) {
            chrome_browser_net::NetErrorTabHelper::BindNetworkEasterEgg(
                std::move(receiver), render_frame_host);
          },
          &render_frame_host));
  associated_registry.AddInterface<chrome::mojom::NetErrorPageSupport>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<chrome::mojom::NetErrorPageSupport>
                 receiver) {
            chrome_browser_net::NetErrorTabHelper::BindNetErrorPageSupport(
                std::move(receiver), render_frame_host);
          },
          &render_frame_host));
#if BUILDFLAG(ENABLE_PLUGINS)
  associated_registry.AddInterface<
      chrome::mojom::PluginAuthHost>(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<chrome::mojom::PluginAuthHost>
             receiver) {
        extensions::ChromeWebViewPermissionHelperDelegate::BindPluginAuthHost(
            std::move(receiver), render_frame_host);
      },
      &render_frame_host));
#endif
#if BUILDFLAG(ENABLE_PLUGINS) || BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_ANDROID)
  using PluginObserverImpl = PluginObserverAndroid;
#else
    using PluginObserverImpl = PluginObserver;
#endif
  associated_registry.AddInterface<chrome::mojom::PluginHost>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<chrome::mojom::PluginHost>
                 receiver) {
            PluginObserverImpl::BindPluginHost(std::move(receiver),
                                               render_frame_host);
          },
          &render_frame_host));
#endif  // BUILDFLAG(ENABLE_PLUGINS) || BUILDFLAG(IS_ANDROID)
  associated_registry.AddInterface<
      chrome::mojom::TrustedVaultEncryptionKeysExtension>(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<
             chrome::mojom::TrustedVaultEncryptionKeysExtension> receiver) {
        TrustedVaultEncryptionKeysTabHelper::
            BindTrustedVaultEncryptionKeysExtension(std::move(receiver),
                                                    render_frame_host);
      },
      &render_frame_host));
  associated_registry.AddInterface<
      chrome::mojom::GoogleAccountsPrivateApiExtension>(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<
             chrome::mojom::GoogleAccountsPrivateApiExtension> receiver) {
        GoogleAccountsPrivateApiHost::BindHost(std::move(receiver),
                                               render_frame_host);
      },
      &render_frame_host));
  associated_registry.AddInterface<
      content_capture::mojom::ContentCaptureReceiver>(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<
             content_capture::mojom::ContentCaptureReceiver> receiver) {
        content_capture::OnscreenContentProvider::BindContentCaptureReceiver(
            std::move(receiver), render_frame_host);
      },
      &render_frame_host));
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  associated_registry.AddInterface<extensions::mojom::LocalFrameHost>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<extensions::mojom::LocalFrameHost>
                 receiver) {
            extensions::ExtensionWebContentsObserver::BindLocalFrameHost(
                std::move(receiver), render_frame_host);
          },
          &render_frame_host));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  associated_registry.AddInterface<offline_pages::mojom::MhtmlPageNotifier>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<
                 offline_pages::mojom::MhtmlPageNotifier> receiver) {
            offline_pages::OfflinePageTabHelper::BindHtmlPageNotifier(
                std::move(receiver), render_frame_host);
          },
          &render_frame_host));
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
  associated_registry.AddInterface<page_load_metrics::mojom::PageLoadMetrics>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<
                 page_load_metrics::mojom::PageLoadMetrics> receiver) {
            page_load_metrics::MetricsWebContentsObserver::BindPageLoadMetrics(
                std::move(receiver), render_frame_host);
          },
          &render_frame_host));
#if BUILDFLAG(ENABLE_PDF)
  associated_registry.AddInterface<pdf::mojom::PdfHost>(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<pdf::mojom::PdfHost> receiver) {
        pdf::PDFDocumentHelper::BindPdfHost(
            std::move(receiver), render_frame_host,
            std::make_unique<ChromePDFDocumentHelperClient>());
      },
      &render_frame_host));
#endif  // BUILDFLAG(ENABLE_PDF)
#if !BUILDFLAG(IS_ANDROID)
  associated_registry.AddInterface<search::mojom::EmbeddedSearchConnector>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<
                 search::mojom::EmbeddedSearchConnector> receiver) {
            SearchTabHelper::BindEmbeddedSearchConnecter(std::move(receiver),
                                                         render_frame_host);
          },
          &render_frame_host));
#endif  //  !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(ENABLE_PRINTING)
  associated_registry.AddInterface<printing::mojom::PrintManagerHost>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<printing::mojom::PrintManagerHost>
                 receiver) {
            if (headless::IsHeadlessMode()) {
              headless::HeadlessPrintManager::BindPrintManagerHost(
                  std::move(receiver), render_frame_host);
            } else {
#if BUILDFLAG(IS_CHROMEOS)
              if (base::FeatureList::IsEnabled(
                      ::features::kPrintPreviewCrosPrimary)) {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
                chromeos::PrintViewManagerCros::BindPrintManagerHost(
                    std::move(receiver), render_frame_host);
#else
                chromeos::PrintViewManagerCrosBasic::BindPrintManagerHost(
                    std::move(receiver), render_frame_host);
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
                return;
              }
#endif  // BUILDFLAG(CHROMEOS)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
              printing::PrintViewManager::BindPrintManagerHost(
                  std::move(receiver), render_frame_host);
#else
              printing::PrintViewManagerBasic::BindPrintManagerHost(
                  std::move(receiver), render_frame_host);
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
            }
          },
          &render_frame_host));
#endif  // BUILDFLAG(ENABLE_PRINTING)
  associated_registry.AddInterface<
      security_interstitials::mojom::InterstitialCommands>(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<
             security_interstitials::mojom::InterstitialCommands> receiver) {
        security_interstitials::SecurityInterstitialTabHelper::
            BindInterstitialCommands(std::move(receiver), render_frame_host);
      },
      &render_frame_host));
  associated_registry.AddInterface<
      subresource_filter::mojom::SubresourceFilterHost>(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<
             subresource_filter::mojom::SubresourceFilterHost> receiver) {
        subresource_filter::ContentSubresourceFilterThrottleManager::
            BindReceiver(std::move(receiver), render_frame_host);
      },
      &render_frame_host));
  if (fingerprinting_protection_filter::features::
          IsFingerprintingProtectionFeatureEnabled()) {
    associated_registry.AddInterface<
        fingerprinting_protection_filter::mojom::FingerprintingProtectionHost>(
        base::BindRepeating(
            [](content::RenderFrameHost* render_frame_host,
               mojo::PendingAssociatedReceiver<
                   fingerprinting_protection_filter::mojom::
                       FingerprintingProtectionHost> receiver) {
              fingerprinting_protection_filter::ThrottleManager::BindReceiver(
                  std::move(receiver), render_frame_host);
            },
            &render_frame_host));
  }
  associated_registry
      .AddInterface<supervised_user::mojom::SupervisedUserCommands>(
          base::BindRepeating(
              [](content::RenderFrameHost* render_frame_host,
                 mojo::PendingAssociatedReceiver<
                     supervised_user::mojom::SupervisedUserCommands> receiver) {
                SupervisedUserNavigationObserver::BindSupervisedUserCommands(
                    std::move(receiver), render_frame_host);
              },
              &render_frame_host));
}

void ChromeContentBrowserClient::BindGpuHostReceiver(
    mojo::GenericPendingReceiver receiver) {
  if (auto r = receiver.As<metrics::mojom::CallStackProfileCollector>()) {
    metrics::CallStackProfileCollector::Create(std::move(r));
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (auto r = receiver.As<chromeos::cdm::mojom::BrowserCdmFactory>())
    chromeos::CdmFactoryDaemonProxyAsh::Create(std::move(r));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  if (auto r = receiver.As<chromeos::cdm::mojom::BrowserCdmFactory>())
    chromeos::CdmFactoryDaemonProxyLacros::Create(std::move(r));
#endif
}

void ChromeContentBrowserClient::BindUtilityHostReceiver(
    mojo::GenericPendingReceiver receiver) {
  if (auto r = receiver.As<metrics::mojom::CallStackProfileCollector>()) {
    metrics::CallStackProfileCollector::Create(std::move(r));
    return;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (auto service_manager_receiver =
          receiver
              .As<chromeos::mojo_service_manager::mojom::ServiceManager>()) {
    ash::mojo_service_manager::EstablishUtilityProcessBridge(
        std::move(service_manager_receiver));
    return;
  }
#endif
}

void ChromeContentBrowserClient::BindHostReceiverForRenderer(
    content::RenderProcessHost* render_process_host,
    mojo::GenericPendingReceiver receiver) {
  if (auto host_receiver =
          receiver.As<content_settings::mojom::ContentSettingsManager>()) {
    content_settings::ContentSettingsManagerImpl::Create(
        render_process_host, std::move(host_receiver),
        std::make_unique<ContentSettingsManagerDelegate>());
    return;
  }

#if BUILDFLAG(ENABLE_SPELLCHECK)
  if (auto host_receiver =
          receiver.As<spellcheck::mojom::SpellCheckInitializationHost>()) {
    SpellCheckInitializationHostImpl::Create(render_process_host->GetID(),
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

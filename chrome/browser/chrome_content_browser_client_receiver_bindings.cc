// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exposes services from the browser to child processes.

#include "chrome/browser/chrome_content_browser_client.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cache_stats_recorder.h"
#include "chrome/browser/chrome_browser_interface_binders.h"
#include "chrome/browser/chrome_content_browser_client_parts.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/net_benchmarking.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/network_hints_handler_impl.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/content_capture/browser/content_capture_receiver_manager.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/metrics/call_stack_profile_collector.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/rappor/public/rappor_utils.h"
#include "components/rappor/rappor_recorder_impl.h"
#include "components/rappor/rappor_service_impl.h"
#include "components/safe_browsing/browser/mojo_safe_browsing_impl.h"
#include "components/safe_browsing/buildflags.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/startup_metric_utils/browser/startup_metric_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/render_process_host.h"
#include "media/mojo/buildflags.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/widevine/cdm/buildflags.h"

#if defined(OS_ANDROID)
#include "chrome/browser/download/android/available_offline_content_provider.h"
#elif defined(OS_CHROMEOS)
#include "chrome/browser/ash_service_registry.h"
#include "services/service_manager/public/cpp/service.h"
#elif defined(OS_WIN)
#include "chrome/browser/win/conflicts/module_database.h"
#include "chrome/browser/win/conflicts/module_event_sink_impl.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "chrome/browser/media/output_protection_impl.h"
#include "chrome/browser/media/platform_verification_impl.h"
#endif

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

namespace {

void AddDataReductionProxyReceiver(
    int render_process_id,
    mojo::PendingReceiver<data_reduction_proxy::mojom::DataReductionProxy>
        receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* rph = content::RenderProcessHost::FromID(render_process_id);
  if (!rph)
    return;

  auto* drp_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          rph->GetBrowserContext());
  if (!drp_settings)
    return;

  drp_settings->data_reduction_proxy_service()->Clone(std::move(receiver));
}

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
  associated_registry->AddInterface(
      base::Bind(&CacheStatsRecorder::Create, render_process_host->GetID()));

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner =
      base::CreateSingleThreadTaskRunner({content::BrowserThread::UI});
  registry->AddInterface(base::Bind(&rappor::RapporRecorderImpl::Create,
                                    g_browser_process->rappor_service()),
                         ui_task_runner);
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
        base::Bind(
            &safe_browsing::MojoSafeBrowsingImpl::MaybeCreate,
            render_process_host->GetID(), resource_context,
            base::Bind(
                &ChromeContentBrowserClient::GetSafeBrowsingUrlCheckerDelegate,
                base::Unretained(this), resource_context)),
        base::CreateSingleThreadTaskRunner({content::BrowserThread::IO}));
  }
#endif

  if (data_reduction_proxy::params::IsEnabledWithNetworkService()) {
    registry->AddInterface(base::BindRepeating(&AddDataReductionProxyReceiver,
                                               render_process_host->GetID()),
                           ui_task_runner);
  }

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
      base::CreateSingleThreadTaskRunner({content::BrowserThread::UI}));
#endif

  for (auto* ep : extra_parts_) {
    ep->ExposeInterfacesToRenderer(registry, associated_registry,
                                   render_process_host);
  }
}

void ChromeContentBrowserClient::ExposeInterfacesToMediaService(
    service_manager::BinderRegistry* registry,
    content::RenderFrameHost* render_frame_host) {
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  registry->AddInterface(
      base::Bind(&OutputProtectionImpl::Create, render_frame_host));
  registry->AddInterface(
      base::Bind(&PlatformVerificationImpl::Create, render_frame_host));
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(ENABLE_MOJO_CDM) && defined(OS_ANDROID)
  registry->AddInterface(base::Bind(&CreateMediaDrmStorage, render_frame_host));
#endif
}

void ChromeContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    service_manager::BinderMapWithContext<content::RenderFrameHost*>* map) {
  chrome::internal::PopulateChromeFrameBinders(map);
}

void ChromeContentBrowserClient::BindInterfaceRequestFromFrame(
    content::RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (!frame_interfaces_ && !frame_interfaces_parameterized_ &&
      !worker_interfaces_parameterized_) {
    InitWebContextInterfaces();
  }

  if (!frame_interfaces_parameterized_->TryBindInterface(
          interface_name, &interface_pipe, render_frame_host)) {
    frame_interfaces_->TryBindInterface(interface_name, &interface_pipe);
  }
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
  if (interface_name == content_capture::mojom::ContentCaptureReceiver::Name_) {
    content_capture::ContentCaptureReceiverManager::BindContentCaptureReceiver(
        mojo::PendingAssociatedReceiver<
            content_capture::mojom::ContentCaptureReceiver>(std::move(*handle)),
        render_frame_host);
    return true;
  }

  return false;
}

void ChromeContentBrowserClient::BindInterfaceRequestFromWorker(
    content::RenderProcessHost* render_process_host,
    const url::Origin& origin,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (!frame_interfaces_ && !frame_interfaces_parameterized_ &&
      !worker_interfaces_parameterized_) {
    InitWebContextInterfaces();
  }

  worker_interfaces_parameterized_->BindInterface(
      interface_name, std::move(interface_pipe), render_process_host, origin);
}

void ChromeContentBrowserClient::BindGpuHostReceiver(
    mojo::GenericPendingReceiver receiver) {
  if (auto r = receiver.As<metrics::mojom::CallStackProfileCollector>())
    metrics::CallStackProfileCollector::Create(std::move(r));
}

void ChromeContentBrowserClient::BindHostReceiverForRenderer(
    content::RenderProcessHost* render_process_host,
    mojo::GenericPendingReceiver receiver) {
  if (auto host_receiver =
          receiver.As<network_hints::mojom::NetworkHintsHandler>()) {
    predictors::NetworkHintsHandlerImpl::Create(render_process_host->GetID(),
                                                std::move(host_receiver));
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
}

void ChromeContentBrowserClient::BindHostReceiverForRendererOnIOThread(
    int render_process_id,
    mojo::GenericPendingReceiver* receiver) {
  if (auto host_receiver =
          receiver->As<startup_metric_utils::mojom::StartupMetricHost>()) {
    startup_metric_utils::StartupMetricHostImpl::Create(
        std::move(host_receiver));
    return;
  }
}

void ChromeContentBrowserClient::RunServiceInstance(
    const service_manager::Identity& identity,
    mojo::PendingReceiver<service_manager::mojom::Service>* receiver) {
  const std::string& service_name = identity.name();
  ALLOW_UNUSED_LOCAL(service_name);
#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
  if (service_name == media::mojom::kMediaServiceName) {
    service_manager::Service::RunAsyncUntilTermination(
        media::CreateMediaService(std::move(*receiver)));
    return;
  }
#endif

#if defined(OS_CHROMEOS)
  auto service = ash_service_registry::HandleServiceRequest(
      service_name, std::move(*receiver));
  if (service)
    service_manager::Service::RunAsyncUntilTermination(std::move(service));
#endif  // defined(OS_CHROMEOS)
}

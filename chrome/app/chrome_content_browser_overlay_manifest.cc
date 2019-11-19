// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_content_browser_overlay_manifest.h"

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/engagement/site_engagement_details.mojom.h"
#include "chrome/browser/media/media_engagement_score_details.mojom.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals.mojom.h"
#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals.mojom.h"
#include "chrome/browser/ui/webui/interventions_internals/interventions_internals.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/browser/ui/webui/omnibox/omnibox.mojom.h"
#include "chrome/browser/ui/webui/reset_password/reset_password.mojom.h"
#include "chrome/browser/ui/webui/snippets_internals/snippets_internals.mojom.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals.mojom.h"
#include "chrome/common/available_offline_content.mojom.h"
#include "chrome/common/cache_stats_recorder.mojom.h"
#include "chrome/common/media_router/mojom/media_router.mojom.h"
#include "chrome/common/net_benchmarking.mojom.h"
#include "chrome/common/offline_page_auto_fetcher.mojom.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/contextual_search/content/common/mojom/contextual_search_js_api_service.mojom.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy.mojom.h"
#include "components/metrics/public/mojom/call_stack_profile_collector.mojom.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/rappor/public/mojom/rappor_recorder.mojom.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "components/translate/content/common/translate.mojom.h"
#include "extensions/buildflags/buildflags.h"
#include "services/preferences/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "third_party/blink/public/mojom/badging/badging.mojom.h"
#include "third_party/blink/public/mojom/installedapp/installed_app_provider.mojom.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision.mojom.h"
#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer.mojom.h"
#include "chrome/browser/ui/webui/chromeos/machine_learning/machine_learning_internals_page_handler.mojom.h"
#include "chromeos/services/cellular_setup/public/mojom/cellular_setup.mojom.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/services/media_perception/public/mojom/media_perception.mojom.h"
#include "chromeos/services/multidevice_setup/public/cpp/manifest.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "chromeos/services/network_config/public/mojom/constants.mojom.h"  // nogncheck
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"  // nogncheck
#include "components/chromeos_camera/common/camera_app_helper.mojom.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#endif

#if defined(OS_WIN)
#include "chrome/common/conflicts/module_event_sink_win.mojom.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/ui/webui/explore_sites_internals/explore_sites_internals.mojom.h"
#else
#include "chrome/browser/ui/webui/app_management/app_management.mojom.h"
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/mojom/keep_alive.mojom.h"  // nogncheck
#endif

const service_manager::Manifest& GetChromeContentBrowserOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .ExposeCapability("gpu",
                          service_manager::Manifest::InterfaceList<
                              metrics::mojom::CallStackProfileCollector>())
        .ExposeCapability("renderer",
                          service_manager::Manifest::InterfaceList<
                              chrome::mojom::AvailableOfflineContentProvider,
                              chrome::mojom::CacheStatsRecorder,
                              chrome::mojom::NetBenchmarking,
                              data_reduction_proxy::mojom::DataReductionProxy,
                              metrics::mojom::CallStackProfileCollector,
#if defined(OS_WIN)
                              mojom::ModuleEventSink,
#endif
                              rappor::mojom::RapporRecorder,
                              safe_browsing::mojom::SafeBrowsing>())
        .RequireCapability("ash", "system_ui")
        .RequireCapability("ash", "test")
        .RequireCapability("ash", "display")
        .RequireCapability("assistant", "assistant")
        .RequireCapability("assistant_audio_decoder", "assistant:audio_decoder")
        // Only used in the classic Ash case
        .RequireCapability("chrome", "input_device_controller")
        .RequireCapability("chrome_printing", "converter")
        .RequireCapability("cups_ipp_parser", "ipp_parser")
        .RequireCapability("device", "device:fingerprint")
        .RequireCapability("device", "device:geolocation_config")
        .RequireCapability("device", "device:geolocation_control")
        .RequireCapability("device", "device:ip_geolocator")
        .RequireCapability("ime", "input_engine")
        .RequireCapability("mirroring", "mirroring")
        .RequireCapability("nacl_broker", "browser")
        .RequireCapability("nacl_loader", "browser")
        .RequireCapability("noop", "noop")
        .RequireCapability("patch", "patch_file")
        .RequireCapability("preferences", "pref_client")
        .RequireCapability("preferences", "pref_control")
        .RequireCapability("profile_import", "import")
        .RequireCapability("removable_storage_writer",
                           "removable_storage_writer")
        .RequireCapability("secure_channel", "secure_channel")
        .RequireCapability("ui", "ime_registrar")
        .RequireCapability("ui", "input_device_controller")
        .RequireCapability("ui", "window_manager")
        .RequireCapability("unzip", "unzip_file")
        .RequireCapability("util_win", "util_win")
        .RequireCapability("xr_device_service", "xr_device_provider")
        .RequireCapability("xr_device_service", "xr_device_test_hook")
#if defined(OS_CHROMEOS)
        .RequireCapability("multidevice_setup", "multidevice_setup")
#endif
        .ExposeInterfaceFilterCapability_Deprecated(
            "navigation:frame", "renderer",
            service_manager::Manifest::InterfaceList<
                autofill::mojom::AutofillDriver,
                autofill::mojom::PasswordManagerDriver,
                chrome::mojom::OfflinePageAutoFetcher,
#if defined(OS_CHROMEOS)
                chromeos_camera::mojom::CameraAppHelper,
                chromeos::cellular_setup::mojom::CellularSetup,
                chromeos::crostini_installer::mojom::PageHandlerFactory,
                chromeos::ime::mojom::InputEngineManager,
                chromeos::machine_learning::mojom::PageHandler,
                chromeos::media_perception::mojom::MediaPerception,
                chromeos::multidevice_setup::mojom::MultiDeviceSetup,
                chromeos::multidevice_setup::mojom::PrivilegedHostDeviceSetter,
                chromeos::network_config::mojom::CrosNetworkConfig,
                cros::mojom::CameraAppDeviceProvider,
#endif
                contextual_search::mojom::ContextualSearchJsApiService,
#if BUILDFLAG(ENABLE_EXTENSIONS)
                extensions::KeepAlive,
#endif
                media::mojom::MediaEngagementScoreDetailsProvider,
                media_router::mojom::MediaRouter,
                page_load_metrics::mojom::PageLoadMetrics,
                translate::mojom::ContentTranslateDriver,

                // WebUI-only interfaces go below this line. These should be
                // brokered through a dedicated interface, but they're here
                // for for now.
                downloads::mojom::PageHandlerFactory,
                feed_internals::mojom::PageHandler,
                new_tab_page::mojom::PageHandlerFactory,
#if defined(OS_ANDROID)
                explore_sites_internals::mojom::PageHandler,
#else
                app_management::mojom::PageHandlerFactory,
#endif
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
                discards::mojom::DetailsProvider, discards::mojom::GraphDump,
#endif
#if defined(OS_CHROMEOS)
                add_supervision::mojom::AddSupervisionHandler,
#endif
                mojom::BluetoothInternalsHandler,
                mojom::InterventionsInternalsPageHandler,
                mojom::OmniboxPageHandler, mojom::ResetPasswordHandler,
                mojom::SiteEngagementDetailsProvider,
                mojom::UsbInternalsPageHandler,
                snippets_internals::mojom::PageHandlerFactory>())
        .PackageService(prefs::GetManifest())
#if defined(OS_CHROMEOS)
        .PackageService(chromeos::multidevice_setup::GetManifest())
#endif  // defined(OS_CHROMEOS)
        .Build()
  };
  return *manifest;
}

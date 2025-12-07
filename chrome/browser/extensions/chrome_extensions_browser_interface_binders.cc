// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extensions_browser_interface_binders.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "components/guest_view/buildflags/buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_version_base_info.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "mojo/public/cpp/bindings/binder_map.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/api/mime_handler_private/mime_handler_private.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/common/api/mime_handler.mojom.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/webui/camera_app_ui/camera_app_ui.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos_factory.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/ash/components/enhanced_network_tts/enhanced_network_tts_impl.h"
#include "chromeos/ash/components/enhanced_network_tts/mojom/enhanced_network_tts.mojom.h"
#include "chromeos/ash/components/language_packs/language_packs_impl.h"
#include "chromeos/ash/components/language_packs/public/mojom/language_packs.mojom.h"
#include "chromeos/components/remote_apps/mojom/remote_apps.mojom.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/appid_util.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/xu_camera.mojom.h"
#include "chromeos/services/media_perception/public/mojom/media_perception.mojom.h"
#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/media_perception_private/media_perception_api_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"  // nogncheck
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/input_method_manager.h"
#endif

#if BUILDFLAG(PLATFORM_CFM)
#include "chrome/browser/ash/chromebox_for_meetings/meet_browser/meet_browser_service.h"
#include "chrome/browser/ash/chromebox_for_meetings/xu_camera/xu_camera_service.h"
#include "chromeos/ash/components/chromebox_for_meetings/features.h"
#endif
#endif  // BUILDFLAG(IS_CHROMEOS)

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {
#if BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Resolves InputEngineManager receiver in InputMethodManager.
void BindInputEngineManager(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<ash::ime::mojom::InputEngineManager> receiver) {
  ash::input_method::InputMethodManager::Get()->ConnectInputEngineManager(
      std::move(receiver));
}

void BindMachineLearningService(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<
        chromeos::machine_learning::mojom::MachineLearningService> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->BindMachineLearningService(std::move(receiver));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

void BindLanguagePacks(
    mojo::PendingReceiver<ash::language::mojom::LanguagePacks> receiver) {
  ash::language_packs::LanguagePacksImpl::GetInstance().BindReceiver(
      std::move(receiver));
}

void BindGoogleTtsStream(
    content::BrowserContext* browser_context,
    mojo::PendingReceiver<chromeos::tts::mojom::GoogleTtsStream> receiver) {
  TtsEngineExtensionObserverChromeOSFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context))
      ->BindGoogleTtsStream(std::move(receiver));
}

void BindEnhancedNetworkTts(
    content::BrowserContext* browser_context,
    mojo::PendingReceiver<ash::enhanced_network_tts::mojom::EnhancedNetworkTts>
        receiver) {
  ash::enhanced_network_tts::EnhancedNetworkTtsImpl::GetInstance()
      .BindReceiverAndURLFactory(
          std::move(receiver),
          Profile::FromBrowserContext(browser_context)->GetURLLoaderFactory());
}

void BindRemoteAppsFactory(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<chromeos::remote_apps::mojom::RemoteAppsFactory>
        pending_receiver) {
  // |remote_apps_manager| will be null for sessions that are not regular user
  // sessions or managed guest sessions. This is checked in
  // |RemoteAppsImpl::IsMojoPrivateApiAllowed()|.
  ash::RemoteAppsManager* remote_apps_manager =
      ash::RemoteAppsManagerFactory::GetForProfile(
          Profile::FromBrowserContext(render_frame_host->GetBrowserContext()));
  DCHECK(remote_apps_manager);
  remote_apps_manager->BindFactoryInterface(std::move(pending_receiver));
}

void BindCfmServiceContext(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<chromeos::cfm::mojom::CfmServiceContext> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  chromeos::cfm::ServiceConnection::GetInstance()->BindServiceContext(
      std::move(receiver));
#if BUILDFLAG(PLATFORM_CFM)
  ash::cfm::MeetBrowserService::Get()->SetMeetGlobalRenderFrameToken(
      render_frame_host->GetGlobalFrameToken());
#endif  // BUILDFLAG(PLATFORM_CFM)
}

#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_GUEST_VIEW)
void BindMimeHandlerService(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<mime_handler::MimeHandlerService> receiver) {
  auto* guest_view = MimeHandlerViewGuest::FromRenderFrameHost(frame_host);
  if (!guest_view) {
    return;
  }
  MimeHandlerServiceImpl::Create(guest_view->GetStreamWeakPtr(),
                                 std::move(receiver));
}

void BindBeforeUnloadControl(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<mime_handler::BeforeUnloadControl> receiver) {
  auto* guest_view = MimeHandlerViewGuest::FromRenderFrameHost(frame_host);
  if (!guest_view) {
    return;
  }
  guest_view->FuseBeforeUnloadControl(std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)

}  // namespace

void PopulateChromeFrameBindersForExtension(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) {
  DCHECK(extension);

#if BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Register InputEngineManager for official Google ChromeOS 1P Input only.
  if (extension->id() == ash::extension_ime_util::kXkbExtensionId) {
    binder_map->Add<ash::ime::mojom::InputEngineManager>(
        &BindInputEngineManager);
    binder_map->Add<ash::language::mojom::LanguagePacks>(
        [](content::RenderFrameHost* frame_host,
           mojo::PendingReceiver<ash::language::mojom::LanguagePacks>
               receiver) { BindLanguagePacks(std::move(receiver)); });
    binder_map->Add<chromeos::machine_learning::mojom::MachineLearningService>(
        &BindMachineLearningService);
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// The experimentation framework used to manage the
// `ash::cfm::features::kMojoServices` feature flag requires
// Chrome to restart before updates are applied. Meet Devices have
// a variable uptime ranging from a week or more and set by the
// admin. Additionally its kiosked process is not tied to a chromium
// release and can be dynamically updated during Chrome runtime.
// Unfortunately this makes it difficult to fully predict when the
// flag will be applied to all devices across the fleet.
// As such we proactively support the case for devices that may be
// in a different state than expected from the kiosked process.
// TODO(b/341493979): Deprecate after CfM LaCrOS migration is completed.
#if BUILDFLAG(PLATFORM_CFM)
  if (chromeos::cfm::IsChromeboxForMeetingsHashedAppId(
          extension->hashed_id().value())) {
    binder_map->Add<ash::cfm::mojom::XuCamera>(
        [](content::RenderFrameHost* frame_host,
           mojo::PendingReceiver<ash::cfm::mojom::XuCamera> receiver) {
          if (base::FeatureList::IsEnabled(ash::cfm::features::kXuControls)) {
            ash::cfm::XuCameraService::Get()->BindServiceContext(
                std::move(receiver), frame_host->GetGlobalId());
          } else {
            receiver.ResetWithReason(
                static_cast<uint32_t>(
                    chromeos::cfm::mojom::DisconnectReason::kFinchDisabledCode),
                chromeos::cfm::mojom::DisconnectReason::kFinchDisabledMessage);
          }
        });
  }
#endif  // BUILDFLAG(PLATFORM_CFM)

  if (extension->permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kMediaPerceptionPrivate)) {
    extensions::ExtensionsAPIClient* client =
        extensions::ExtensionsAPIClient::Get();
    extensions::MediaPerceptionAPIDelegate* delegate = nullptr;
    if (client) {
      delegate = client->GetMediaPerceptionAPIDelegate();
    }
    if (delegate) {
      // Note that it is safe to use base::Unretained here because |delegate| is
      // owned by the |client|, which is instantiated by the
      // ChromeExtensionsBrowserClient, which in turn is owned and lives as long
      // as the BrowserProcessImpl.
      binder_map->Add<chromeos::media_perception::mojom::MediaPerception>(
          base::BindRepeating(&extensions::MediaPerceptionAPIDelegate::
                                  ForwardMediaPerceptionReceiver,
                              base::Unretained(delegate)));
    }
  }

  if (extension->id() == extension_misc::kGoogleSpeechSynthesisExtensionId) {
    binder_map->Add<chromeos::tts::mojom::GoogleTtsStream>(base::BindRepeating(
        [](content::RenderFrameHost* frame_host,
           mojo::PendingReceiver<chromeos::tts::mojom::GoogleTtsStream>
               receiver) {
          BindGoogleTtsStream(frame_host->GetBrowserContext(),
                              std::move(receiver));
        }));
    binder_map->Add<ash::language::mojom::LanguagePacks>(base::BindRepeating(
        [](content::RenderFrameHost* frame_host,
           mojo::PendingReceiver<ash::language::mojom::LanguagePacks>
               receiver) { BindLanguagePacks(std::move(receiver)); }));
  }

  // Limit the binding to EnhancedNetworkTts Extension.
  if (extension->id() == extension_misc::kEnhancedNetworkTtsExtensionId) {
    binder_map->Add<ash::enhanced_network_tts::mojom::EnhancedNetworkTts>(
        [](content::RenderFrameHost* frame_host,
           mojo::PendingReceiver<
               ash::enhanced_network_tts::mojom::EnhancedNetworkTts> receiver) {
          BindEnhancedNetworkTts(frame_host->GetBrowserContext(),
                                 std::move(receiver));
        });
  }

  if (ash::RemoteAppsImpl::IsMojoPrivateApiAllowed(render_frame_host,
                                                   extension)) {
    binder_map->Add<chromeos::remote_apps::mojom::RemoteAppsFactory>(
        &BindRemoteAppsFactory);
  }

  // Only allow specific extensions to bind CfmServiceContext
  if (chromeos::cfm::IsChromeboxForMeetingsHashedAppId(
          extension->hashed_id().value())) {
    binder_map->Add<chromeos::cfm::mojom::CfmServiceContext>(
        &BindCfmServiceContext);

#if !BUILDFLAG(PLATFORM_CFM)
    // On first launch some older devices may be running on none-CfM
    // images. For those devices reject all requests until they are
    // rebooted to the CfM image variant for their device.
    // This applies to LaCrOS and none CfM Ash builds
    // TODO(crbug.com/341493979): Deprecate after CfM LaCrOS migration.
    binder_map->Add<ash::cfm::mojom::XuCamera>(
        [](content::RenderFrameHost* frame_host,
           mojo::PendingReceiver<ash::cfm::mojom::XuCamera> receiver) {
          receiver.ResetWithReason(
              static_cast<uint32_t>(chromeos::cfm::mojom::DisconnectReason::
                                        kServiceUnavailableCode),
              chromeos::cfm::mojom::DisconnectReason::
                  kServiceUnavailableMessage);
        });
#endif  // BUILDFLAG(PLATFORM_CFM)
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  binder_map->Add<mime_handler::MimeHandlerService>(&BindMimeHandlerService);
  binder_map->Add<mime_handler::BeforeUnloadControl>(&BindBeforeUnloadControl);
#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)
}

void PopulateChromeServiceWorkerBindersForExtension(
    mojo::BinderMapWithContext<const content::ServiceWorkerVersionBaseInfo&>*
        binder_map,
    content::BrowserContext* browser_context,
    const Extension* extension) {
#if BUILDFLAG(IS_CHROMEOS)
  if (extension->id() == extension_misc::kGoogleSpeechSynthesisExtensionId) {
    binder_map->Add<chromeos::tts::mojom::GoogleTtsStream>(base::BindRepeating(
        [](content::BrowserContext* browser_context,
           const content::ServiceWorkerVersionBaseInfo&,
           mojo::PendingReceiver<chromeos::tts::mojom::GoogleTtsStream>
               receiver) {
          BindGoogleTtsStream(browser_context, std::move(receiver));
        },
        browser_context));
    binder_map->Add<ash::language::mojom::LanguagePacks>(
        [](const content::ServiceWorkerVersionBaseInfo&,
           mojo::PendingReceiver<ash::language::mojom::LanguagePacks>
               receiver) { BindLanguagePacks(std::move(receiver)); });
  }

  if (extension->id() == extension_misc::kEnhancedNetworkTtsExtensionId) {
    binder_map->Add<ash::enhanced_network_tts::mojom::EnhancedNetworkTts>(
        base::BindRepeating(
            [](content::BrowserContext* browser_context,
               const content::ServiceWorkerVersionBaseInfo&,
               mojo::PendingReceiver<
                   ash::enhanced_network_tts::mojom::EnhancedNetworkTts>
                   receiver) {
              BindEnhancedNetworkTts(browser_context, std::move(receiver));
            },
            browser_context));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace extensions

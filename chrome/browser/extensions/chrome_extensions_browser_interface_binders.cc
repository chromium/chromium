// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extensions_browser_interface_binders.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "chromeos/components/remote_apps/mojom/remote_apps.mojom.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/appid_util.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/xu_camera.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/camera_app_ui/camera_app_ui.h"
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
#include "chrome/browser/ash/chromebox_for_meetings/xu_camera/xu_camera_service.h"
#include "chromeos/ash/components/chromebox_for_meetings/features.h"
#endif
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/remote_apps/remote_apps_proxy_lacros.h"
#include "chrome/browser/lacros/remote_apps/remote_apps_proxy_lacros_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace extensions {

namespace {
#if BUILDFLAG(IS_CHROMEOS_ASH)

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
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<ash::language::mojom::LanguagePacks> receiver) {
  ash::language_packs::LanguagePacksImpl::GetInstance().BindReceiver(
      std::move(receiver));
}

void BindGoogleTtsStream(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<chromeos::tts::mojom::GoogleTtsStream> receiver) {
  TtsEngineExtensionObserverChromeOSFactory::GetForProfile(
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext()))
      ->BindGoogleTtsStream(std::move(receiver));
}

void BindEnhancedNetworkTts(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<ash::enhanced_network_tts::mojom::EnhancedNetworkTts>
        receiver) {
  ash::enhanced_network_tts::EnhancedNetworkTtsImpl::GetInstance()
      .BindReceiverAndURLFactory(
          std::move(receiver),
          Profile::FromBrowserContext(render_frame_host->GetBrowserContext())
              ->GetURLLoaderFactory());
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
void BindRemoteAppsFactory(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<chromeos::remote_apps::mojom::RemoteAppsFactory>
        pending_receiver) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // |remote_apps_manager| will be null for sessions that are not regular user
  // sessions or managed guest sessions. This is checked in
  // |RemoteAppsImpl::IsMojoPrivateApiAllowed()|.
  ash::RemoteAppsManager* remote_apps_manager =
      ash::RemoteAppsManagerFactory::GetForProfile(
          Profile::FromBrowserContext(render_frame_host->GetBrowserContext()));
  DCHECK(remote_apps_manager);
  remote_apps_manager->BindFactoryInterface(std::move(pending_receiver));
#else   // implies BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::RemoteAppsProxyLacros* remote_apps_proxy_lacros =
      chromeos::RemoteAppsProxyLacrosFactory::GetForBrowserContext(
          Profile::FromBrowserContext(render_frame_host->GetBrowserContext()));
  DCHECK(remote_apps_proxy_lacros);
  remote_apps_proxy_lacros->BindFactoryInterface(std::move(pending_receiver));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void BindCfmServiceContext(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<chromeos::cfm::mojom::CfmServiceContext> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  chromeos::cfm::ServiceConnection::GetInstance()->BindServiceContext(
      std::move(receiver));
}

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

void PopulateChromeFrameBindersForExtension(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) {
  DCHECK(extension);

#if BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Register InputEngineManager for official Google ChromeOS 1P Input only.
  if (extension->id() == ash::extension_ime_util::kXkbExtensionId) {
    binder_map->Add<ash::ime::mojom::InputEngineManager>(
        base::BindRepeating(&BindInputEngineManager));
    binder_map->Add<ash::language::mojom::LanguagePacks>(
        base::BindRepeating(&BindLanguagePacks));
    binder_map->Add<chromeos::machine_learning::mojom::MachineLearningService>(
        base::BindRepeating(&BindMachineLearningService));
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
    binder_map->Add<ash::cfm::mojom::XuCamera>(base::BindRepeating(
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
        }));
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
    binder_map->Add<chromeos::tts::mojom::GoogleTtsStream>(
        base::BindRepeating(&BindGoogleTtsStream));
    binder_map->Add<ash::language::mojom::LanguagePacks>(
        base::BindRepeating(&BindLanguagePacks));
  }

  // Limit the binding to EnhancedNetworkTts Extension.
  if (extension->id() == extension_misc::kEnhancedNetworkTtsExtensionId) {
    binder_map->Add<ash::enhanced_network_tts::mojom::EnhancedNetworkTts>(
        base::BindRepeating(&BindEnhancedNetworkTts));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::RemoteAppsImpl::IsMojoPrivateApiAllowed(render_frame_host,
                                                   extension)) {
    binder_map->Add<chromeos::remote_apps::mojom::RemoteAppsFactory>(
        base::BindRepeating(&BindRemoteAppsFactory));
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  const extensions::Feature* feature =
      extensions::FeatureProvider::GetBehaviorFeature(
          extensions::behavior_feature::kImprivataInSessionExtension);
  if (extension && feature &&
      feature->IsAvailableToExtension(extension).is_available()) {
    binder_map->Add<chromeos::remote_apps::mojom::RemoteAppsFactory>(
        base::BindRepeating(&BindRemoteAppsFactory));
  }
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Only allow specific extensions to bind CfmServiceContext
  if (chromeos::cfm::IsChromeboxForMeetingsHashedAppId(
          extension->hashed_id().value())) {
    binder_map->Add<chromeos::cfm::mojom::CfmServiceContext>(
        base::BindRepeating(&BindCfmServiceContext));

#if !BUILDFLAG(PLATFORM_CFM)
// On first launch some older devices may be running on none-CfM
// images. For those devices reject all requests until they are
// rebooted to the CfM image variant for their device.
// This applies to LaCrOS and none CfM Ash builds
// TODO(b/341493979): Deprecate after CfM LaCrOS migration is completed.
    binder_map->Add<ash::cfm::mojom::XuCamera>(base::BindRepeating(
        [](content::RenderFrameHost* frame_host,
           mojo::PendingReceiver<ash::cfm::mojom::XuCamera> receiver) {
          receiver.ResetWithReason(
              static_cast<uint32_t>(chromeos::cfm::mojom::DisconnectReason::
                                        kServiceUnavailableCode),
              chromeos::cfm::mojom::DisconnectReason::
                  kServiceUnavailableMessage);
        }));
#endif  // BUILDFLAG(PLATFORM_CFM)
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace extensions

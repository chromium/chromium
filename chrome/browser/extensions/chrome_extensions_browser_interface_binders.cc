// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extensions_browser_interface_binders.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/enhanced_network_tts/mojom/enhanced_network_tts.mojom.h"
#include "chrome/browser/ash/enhanced_network_tts/enhanced_network_tts_impl.h"
#include "chrome/browser/ash/remote_apps/remote_apps_impl.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/components/camera_app_ui/camera_app_ui.h"
#include "chromeos/components/chromebox_for_meetings/buildflags/buildflags.h"
#include "chromeos/components/remote_apps/mojom/remote_apps.mojom.h"
#include "chromeos/services/media_perception/public/mojom/media_perception.mojom.h"
#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/media_perception_private/media_perception_api_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"  // nogncheck
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#endif

#if BUILDFLAG(PLATFORM_CFM)
#include "chromeos/components/chromebox_for_meetings/features/features.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/appid_util.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#endif
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

namespace {
#if BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Resolves InputEngineManager receiver in InputMethodManager.
void BindInputEngineManager(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<chromeos::ime::mojom::InputEngineManager> receiver) {
  chromeos::input_method::InputMethodManager::Get()->ConnectInputEngineManager(
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

void BindGoogleTtsStream(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<chromeos::tts::mojom::GoogleTtsStream> receiver) {
  TtsEngineExtensionObserverChromeOS::GetInstance(
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext()))
      ->BindGoogleTtsStream(std::move(receiver));
}

void BindRemoteAppsFactory(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<chromeos::remote_apps::mojom::RemoteAppsFactory>
        pending_receiver) {
  // |remote_apps_manager| will be null in non-managed guest sessions, but this
  // is already checked in |RemoteAppsImpl::IsAllowed()|.
  ash::RemoteAppsManager* remote_apps_manager =
      ash::RemoteAppsManagerFactory::GetForProfile(
          Profile::FromBrowserContext(render_frame_host->GetBrowserContext()));
  DCHECK(remote_apps_manager);
  remote_apps_manager->BindInterface(std::move(pending_receiver));
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
}  // namespace

void PopulateChromeFrameBindersForExtension(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) {
  DCHECK(extension);

#if BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Registry InputEngineManager for official Google XKB Input only.
  if (extension->id() == chromeos::extension_ime_util::kXkbExtensionId) {
    binder_map->Add<chromeos::ime::mojom::InputEngineManager>(
        base::BindRepeating(&BindInputEngineManager));
    binder_map->Add<chromeos::machine_learning::mojom::MachineLearningService>(
        base::BindRepeating(&BindMachineLearningService));
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(PLATFORM_CFM)
  if (base::FeatureList::IsEnabled(chromeos::cfm::features::kMojoServices) &&
      chromeos::cfm::IsChromeboxForMeetingsAppId(extension->id())) {
    binder_map->Add<chromeos::cfm::mojom::CfmServiceContext>(
        base::BindRepeating(
            [](content::RenderFrameHost* frame_host,
               mojo::PendingReceiver<chromeos::cfm::mojom::CfmServiceContext>
                   receiver) {
              chromeos::cfm::ServiceConnection::GetInstance()
                  ->BindServiceContext(std::move(receiver));
            }));
  }
#endif  // BUILDFLAG(PLATFORM_CFM)

  if (extension->permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kMediaPerceptionPrivate)) {
    extensions::ExtensionsAPIClient* client =
        extensions::ExtensionsAPIClient::Get();
    extensions::MediaPerceptionAPIDelegate* delegate = nullptr;
    if (client)
      delegate = client->GetMediaPerceptionAPIDelegate();
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
  }

  if (ash::RemoteAppsImpl::IsAllowed(render_frame_host, extension)) {
    binder_map->Add<chromeos::remote_apps::mojom::RemoteAppsFactory>(
        base::BindRepeating(&BindRemoteAppsFactory));
  }

  if (features::IsEnhancedNetworkVoicesEnabled()) {
    // TODO(crbug.com/1217301): Add a permission check for the binding.
    binder_map->Add<ash::enhanced_network_tts::mojom::EnhancedNetworkTts>(
        base::BindRepeating(&BindEnhancedNetworkTts));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace extensions

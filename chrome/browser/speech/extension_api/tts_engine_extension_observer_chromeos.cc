// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos.h"

#include <set>

#include "base/check.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/tts_controller.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/accessibility/accessibility_features.h"

namespace {

using ::ash::AccessibilityManager;
using ::ash::AccessibilityNotificationType;

}  // namespace

void TtsEngineExtensionObserverChromeOS::
    UpdateGoogleSpeechSynthesisKeepAliveCountManifestV2(
        content::BrowserContext* context,
        bool increment) {
  extensions::ProcessManager* pm = extensions::ProcessManager::Get(context);
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(context);
  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(
          extension_misc::kGoogleSpeechSynthesisExtensionId);
  if (!extension)
    return;

  if (increment) {
    pm->IncrementLazyKeepaliveCount(
        extension, extensions::Activity::ACCESSIBILITY, std::string());
  } else {
    pm->DecrementLazyKeepaliveCount(
        extension, extensions::Activity::ACCESSIBILITY, std::string());
  }
}

void TtsEngineExtensionObserverChromeOS::
    UpdateGoogleSpeechSynthesisKeepAliveCountManifestV3(
        content::BrowserContext* context,
        bool increment) {
  extensions::ProcessManager* pm = extensions::ProcessManager::Get(context);
  const auto& worker_ids = pm->GetServiceWorkersForExtension(
      extension_misc::kGoogleSpeechSynthesisExtensionId);
  if (worker_ids.empty()) {
    return;
  }

  // TODO(crbug.com/40936639): increment and decrement the lone service worker
  // once the bug is fixed. For now, we increment/decrement all active service
  // workers.
  if (increment) {
    for (const extensions::WorkerId& worker_id : worker_ids) {
      base::Uuid uuid = pm->IncrementServiceWorkerKeepaliveCount(
          /*worker_id=*/worker_id,
          /*timeout_type=*/
          content::ServiceWorkerExternalRequestTimeoutType::kDoesNotTimeout,
          /*activity_type=*/extensions::Activity::Type::ACCESSIBILITY,
          /*extra_data=*/std::string());
      keepalive_uuids_[worker_id].push_back(uuid);
    }
  } else {
    for (auto it = keepalive_uuids_.begin(); it != keepalive_uuids_.end();) {
      if (it->second.empty()) {
        // This case should ideally not happen if increments and decrements
        // are perfectly matched, but we guard against it to prevent a crash.
        it = keepalive_uuids_.erase(it);
        continue;
      }

      pm->DecrementServiceWorkerKeepaliveCount(
          /*worker_id=*/it->first,
          /*request_uuid=*/it->second.back(),
          /*activity_type=*/extensions::Activity::Type::ACCESSIBILITY,
          /*extra_data=*/std::string());

      it->second.pop_back();
      if (it->second.empty()) {
        it = keepalive_uuids_.erase(it);
      } else {
        ++it;
      }
    }
  }
}

void TtsEngineExtensionObserverChromeOS::
    UpdateGoogleSpeechSynthesisKeepAliveCountHelper(
        content::BrowserContext* context,
        bool increment) {
  if (::features::IsAccessibilityManifestV3EnabledForGoogleTts()) {
    UpdateGoogleSpeechSynthesisKeepAliveCountManifestV3(context, increment);
  } else {
    UpdateGoogleSpeechSynthesisKeepAliveCountManifestV2(context, increment);
  }
}

void TtsEngineExtensionObserverChromeOS::
    UpdateGoogleSpeechSynthesisKeepAliveCount(content::BrowserContext* context,
                                              bool increment) {
  // Deal with profiles that are non-off the record and otr. For a given
  // extension load/unload, we only ever get called for one of the two potential
  // profile types.
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile)
    return;

  UpdateGoogleSpeechSynthesisKeepAliveCountHelper(
      profile->HasPrimaryOTRProfile()
          ? profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)
          : profile,
      increment);
}

void TtsEngineExtensionObserverChromeOS::
    UpdateGoogleSpeechSynthesisKeepAliveCountOnReload(
        content::BrowserContext* browser_context) {
  if (AccessibilityManager::Get()->IsSpokenFeedbackEnabled()) {
    UpdateGoogleSpeechSynthesisKeepAliveCount(browser_context,
                                              true /* increment */);
  }

  if (AccessibilityManager::Get()->IsSelectToSpeakEnabled()) {
    UpdateGoogleSpeechSynthesisKeepAliveCount(browser_context,
                                              true /* increment */);
  }
}

TtsEngineExtensionObserverChromeOS::TtsEngineExtensionObserverChromeOS(
    Profile* profile)
    : profile_(profile) {
  extension_registry_observation_.Observe(
      extensions::ExtensionRegistry::Get(profile_));

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);
  DCHECK(event_router);
  event_router->RegisterObserver(this, tts_engine_events::kOnSpeak);
  event_router->RegisterObserver(this, tts_engine_events::kOnStop);

  accessibility_status_subscription_ =
      AccessibilityManager::Get()->RegisterCallback(base::BindRepeating(
          &TtsEngineExtensionObserverChromeOS::OnAccessibilityStatusChanged,
          base::Unretained(this)));
}

TtsEngineExtensionObserverChromeOS::~TtsEngineExtensionObserverChromeOS() =
    default;

void TtsEngineExtensionObserverChromeOS::BindGoogleTtsStream(
    mojo::PendingReceiver<chromeos::tts::mojom::GoogleTtsStream> receiver) {
  // At this point, the component extension has loaded, and the js has requested
  // a TtsStreamFactory be bound. It's safe now to update the keep alive count
  // for important accessibility features.
  UpdateGoogleSpeechSynthesisKeepAliveCountOnReload(profile_);

  CreateTtsServiceIfNeeded();

  // Always create a new audio stream for the tts stream. It is assumed once the
  // tts stream is reset by the service, the audio stream is appropriately
  // cleaned up by the audio service.
  mojo::PendingRemote<media::mojom::AudioStreamFactory> factory_remote;
  auto factory_receiver = factory_remote.InitWithNewPipeAndPassReceiver();
  content::GetAudioService().BindStreamFactory(std::move(factory_receiver));
  tts_service_->BindGoogleTtsStream(std::move(receiver),
                                    std::move(factory_remote));
}

void TtsEngineExtensionObserverChromeOS::BindPlaybackTtsStream(
    mojo::PendingReceiver<chromeos::tts::mojom::PlaybackTtsStream> receiver,
    chromeos::tts::mojom::AudioParametersPtr audio_parameters,
    chromeos::tts::mojom::TtsService::BindPlaybackTtsStreamCallback callback) {
  CreateTtsServiceIfNeeded();

  // Always create a new audio stream for the tts stream. It is assumed once the
  // tts stream is reset by the service, the audio stream is appropriately
  // cleaned up by the audio service.
  mojo::PendingRemote<media::mojom::AudioStreamFactory> factory_remote;
  auto factory_receiver = factory_remote.InitWithNewPipeAndPassReceiver();
  content::GetAudioService().BindStreamFactory(std::move(factory_receiver));
  tts_service_->BindPlaybackTtsStream(
      std::move(receiver), std::move(factory_remote),
      std::move(audio_parameters), std::move(callback));
}

void TtsEngineExtensionObserverChromeOS::Shutdown() {
  extensions::EventRouter::Get(profile_)->UnregisterObserver(this);
}

bool TtsEngineExtensionObserverChromeOS::IsLoadedTtsEngine(
    const std::string& extension_id) {
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);
  DCHECK(event_router);
  if ((event_router->ExtensionHasEventListener(extension_id,
                                               tts_engine_events::kOnSpeak) ||
       event_router->ExtensionHasEventListener(
           extension_id, tts_engine_events::kOnSpeakWithAudioStream)) &&
      event_router->ExtensionHasEventListener(extension_id,
                                              tts_engine_events::kOnStop)) {
    return true;
  }

  return false;
}

void TtsEngineExtensionObserverChromeOS::OnListenerAdded(
    const extensions::EventListenerInfo& details) {
  if (!IsLoadedTtsEngine(details.extension_id))
    return;

  content::TtsController::GetInstance()->VoicesChanged();
}

void TtsEngineExtensionObserverChromeOS::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (extension->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::kTtsEngine)) {
    engine_extension_ids_.insert(extension->id());

    if (extension->id() == extension_misc::kGoogleSpeechSynthesisExtensionId)
      UpdateGoogleSpeechSynthesisKeepAliveCountOnReload(browser_context);
  }
}

void TtsEngineExtensionObserverChromeOS::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  size_t erase_count = 0;
  erase_count += engine_extension_ids_.erase(extension->id());
  if (erase_count > 0)
    content::TtsController::GetInstance()->VoicesChanged();

  if (tts_service_ &&
      extension->id() == extension_misc::kGoogleSpeechSynthesisExtensionId)
    tts_service_.reset();
}

void TtsEngineExtensionObserverChromeOS::OnAccessibilityStatusChanged(
    const ash::AccessibilityStatusEventDetails& details) {
  if (details.notification_type !=
          AccessibilityNotificationType::kToggleSpokenFeedback &&
      details.notification_type !=
          AccessibilityNotificationType::kToggleSelectToSpeak) {
    return;
  }

  // Google speech synthesis might not be loaded yet. If it isn't, the call in
  // |OnExtensionLoaded| will do the increment. If it is, the call below will
  // increment. Decrements only occur when toggling off here.
  UpdateGoogleSpeechSynthesisKeepAliveCount(profile(), details.enabled);
}

void TtsEngineExtensionObserverChromeOS::CreateTtsServiceIfNeeded() {
  // Only launch a new TtsService if necessary. By assigning below, if
  // |tts_service_| held a remote, it will be killed and a new one created,
  // ensuring we only ever have one TtsService running.
  if (tts_service_)
    return;

  tts_service_ =
      content::ServiceProcessHost::Launch<chromeos::tts::mojom::TtsService>(
          content::ServiceProcessHost::Options()
              .WithDisplayName("TtsService")
              .Pass());

  tts_service_.set_disconnect_handler(base::BindOnce(
      [](mojo::Remote<chromeos::tts::mojom::TtsService>* tts_service) {
        tts_service->reset();
      },
      &tts_service_));
}

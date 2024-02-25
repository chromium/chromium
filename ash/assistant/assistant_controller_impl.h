// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_CONTROLLER_IMPL_H_
#define ASH_ASSISTANT_ASSISTANT_CONTROLLER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/assistant/assistant_alarm_timer_controller_impl.h"
#include "ash/assistant/assistant_interaction_controller_impl.h"
#include "ash/assistant/assistant_notification_controller_impl.h"
#include "ash/assistant/assistant_setup_controller.h"
#include "ash/assistant/assistant_state_controller.h"
#include "ash/assistant/assistant_suggestions_controller_impl.h"
#include "ash/assistant/assistant_ui_controller_impl.h"
#include "ash/assistant/assistant_view_delegate_impl.h"
#include "ash/assistant/assistant_web_ui_controller.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/public/cpp/assistant/assistant_interface_binder.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/style/color_mode_observer.h"
#include "ash/public/mojom/assistant_volume_control.mojom.h"
#include "ash/shell_observer.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class PrefRegistrySimple;

namespace ash {

class ASH_EXPORT AssistantControllerImpl
    : public AssistantController,
      public AssistantControllerObserver,
      public AssistantStateObserver,
      public mojom::AssistantVolumeControl,
      public CrasAudioHandler::AudioObserver,
      public AccessibilityObserver,
      public AssistantInterfaceBinder,
      public ColorModeObserver,
      public ShellObserver {
 public:
  AssistantControllerImpl();

  AssistantControllerImpl(const AssistantControllerImpl&) = delete;
  AssistantControllerImpl& operator=(const AssistantControllerImpl&) = delete;

  ~AssistantControllerImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  void BindReceiver(
      mojo::PendingReceiver<mojom::AssistantVolumeControl> receiver);

  // Downloads the image found at the specified |url|. On completion, the
  // supplied |callback| will be run with the downloaded image. If the download
  // attempt is unsuccessful, a NULL image is returned.
  void DownloadImage(const GURL& url,
                     ImageDownloader::DownloadCallback callback);

  // AssistantController:
  void AddObserver(AssistantControllerObserver* observer) override;
  void RemoveObserver(AssistantControllerObserver* observer) override;
  void OpenAssistantSettings() override;
  void OpenUrl(const GURL& url, bool in_background, bool from_server) override;
  base::WeakPtr<ash::AssistantController> GetWeakPtr() override;
  void SetAssistant(assistant::Assistant* assistant) override;
  void StartSpeakerIdEnrollmentFlow() override;
  void SendAssistantFeedback(bool assistant_debug_info_allowed,
                             const std::string& feedback_description,
                             const std::string& screenshot_png) override;

  // AssistantControllerObserver:
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;

  // mojom::VolumeControl:
  void SetVolume(int volume, bool user_initiated) override;
  void SetMuted(bool muted) override;
  void AddVolumeObserver(
      mojo::PendingRemote<mojom::VolumeObserver> observer) override;

  // CrasAudioHandler::AudioObserver:
  void OnOutputMuteChanged(bool mute_on) override;
  void OnOutputNodeVolumeChanged(uint64_t node, int volume) override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // ColorModeObserver:
  void OnColorModeChanged(bool dark_mode_enabled) override;

  // ShellObserver:
  void OnShellDestroying() override;

  AssistantAlarmTimerControllerImpl* alarm_timer_controller() {
    return &assistant_alarm_timer_controller_;
  }

  AssistantNotificationControllerImpl* notification_controller() {
    return &assistant_notification_controller_;
  }

  AssistantSetupController* setup_controller() {
    return &assistant_setup_controller_;
  }

  AssistantWebUiController* web_ui_controller() {
    return &assistant_web_ui_controller_;
  }

  AssistantViewDelegate* view_delegate() { return &view_delegate_; }

  bool IsAssistantReady() const;

 private:
  void NotifyConstructed();
  void NotifyDestroying();
  void NotifyDeepLinkReceived(const GURL& deep_link);
  void NotifyOpeningUrl(const GURL& url, bool in_background, bool from_server);
  void NotifyUrlOpened(const GURL& url, bool from_server);

  // AssistantStateObserver:
  void OnAssistantStatusChanged(assistant::AssistantStatus status) override;
  void OnLockedFullScreenStateChanged(bool enabled) override;

  // AssistantInterfaceBinder implementation:
  void BindVolumeControl(
      mojo::PendingReceiver<mojom::AssistantVolumeControl> receiver) override;

  // The observer list should be initialized early so that sub-controllers may
  // register as observers during their construction.
  base::ObserverList<AssistantControllerObserver> observers_;

  mojo::Receiver<mojom::AssistantVolumeControl>
      assistant_volume_control_receiver_{this};
  mojo::RemoteSet<mojom::VolumeObserver> volume_observers_;

  // |assistant_| can be nullptr if libassistant creation is not yet completed,
  // i.e. it cannot take a request.
  raw_ptr<assistant::Assistant> assistant_ = nullptr;

  // Assistant sub-controllers.
  AssistantAlarmTimerControllerImpl assistant_alarm_timer_controller_{this};
  AssistantInteractionControllerImpl assistant_interaction_controller_{this};
  AssistantNotificationControllerImpl assistant_notification_controller_;
  AssistantStateController assistant_state_controller_;
  AssistantSetupController assistant_setup_controller_{this};
  AssistantSuggestionsControllerImpl assistant_suggestions_controller_;
  AssistantUiControllerImpl assistant_ui_controller_{this};
  AssistantWebUiController assistant_web_ui_controller_;

  AssistantViewDelegateImpl view_delegate_{this};

  base::ScopedObservation<DarkLightModeControllerImpl, ColorModeObserver>
      color_mode_observer_{this};

  base::WeakPtrFactory<AssistantControllerImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_CONTROLLER_IMPL_H_

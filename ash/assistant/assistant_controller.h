// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/assistant/assistant_alarm_timer_controller.h"
#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/assistant_notification_controller.h"
#include "ash/assistant/assistant_screen_context_controller.h"
#include "ash/assistant/assistant_setup_controller.h"
#include "ash/assistant/assistant_state_controller.h"
#include "ash/assistant/assistant_suggestions_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/assistant_view_delegate_impl.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/public/cpp/assistant/assistant_image_downloader.h"
#include "ash/public/cpp/assistant/assistant_interface_binder.h"
#include "ash/public/mojom/assistant_volume_control.mojom.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom-forward.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/content/public/mojom/navigable_contents_factory.mojom-forward.h"

class PrefRegistrySimple;

namespace ash {

class AssistantAlarmTimerController;
class AssistantInteractionController;
class AssistantNotificationController;
class AssistantScreenContextController;
class AssistantSetupController;
class AssistantStateController;
class AssistantSuggestionsController;
class AssistantUiController;
class AssistantWebUiController;

class ASH_EXPORT AssistantController
    : public chromeos::assistant::mojom::AssistantController,
      public AssistantControllerObserver,
      public AssistantStateObserver,
      public mojom::AssistantVolumeControl,
      public chromeos::CrasAudioHandler::AudioObserver,
      public AccessibilityObserver,
      public AssistantInterfaceBinder {
 public:
  AssistantController();
  ~AssistantController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  void BindReceiver(
      mojo::PendingReceiver<chromeos::assistant::mojom::AssistantController>
          receiver);
  void BindReceiver(
      mojo::PendingReceiver<mojom::AssistantVolumeControl> receiver);

  // Adds/removes the specified |observer|.
  void AddObserver(AssistantControllerObserver* observer);
  void RemoveObserver(AssistantControllerObserver* observer);

  // Downloads the image found at the specified |url|. On completion, the
  // supplied |callback| will be run with the downloaded image. If the download
  // attempt is unsuccessful, a NULL image is returned.
  void DownloadImage(const GURL& url,
                     AssistantImageDownloader::DownloadCallback callback);

  // chromeos::assistant::mojom::AssistantController:
  // TODO(updowndota): Refactor Set() calls to use a factory pattern.
  void SetAssistant(mojo::PendingRemote<chromeos::assistant::mojom::Assistant>
                        assistant) override;
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

  // chromeos::CrasAudioHandler::AudioObserver:
  void OnOutputMuteChanged(bool mute_on) override;
  void OnOutputNodeVolumeChanged(uint64_t node, int volume) override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // Opens the specified |url| in a new browser tab. Special handling is applied
  // to deep links which may cause deviation from this behavior.
  void OpenUrl(const GURL& url,
               bool in_background = false,
               bool from_server = false);

  // Acquires a NavigableContentsFactory from the Content Service to allow
  // Assistant to display embedded web contents.
  void GetNavigableContentsFactory(
      mojo::PendingReceiver<content::mojom::NavigableContentsFactory> receiver);

  AssistantAlarmTimerController* alarm_timer_controller() {
    return &assistant_alarm_timer_controller_;
  }

  AssistantInteractionController* interaction_controller() {
    return &assistant_interaction_controller_;
  }

  AssistantNotificationController* notification_controller() {
    return &assistant_notification_controller_;
  }

  AssistantScreenContextController* screen_context_controller() {
    return &assistant_screen_context_controller_;
  }

  AssistantSetupController* setup_controller() {
    return &assistant_setup_controller_;
  }

  AssistantSuggestionsController* suggestions_controller() {
    return &assistant_suggestions_controller_;
  }

  AssistantUiController* ui_controller() { return &assistant_ui_controller_; }

  AssistantWebUiController* web_ui_controller() {
    return assistant_web_ui_controller_.get();
  }

  AssistantViewDelegate* view_delegate() { return &view_delegate_; }

  bool IsAssistantReady() const;

  base::WeakPtr<AssistantController> GetWeakPtr();

 private:
  void NotifyConstructed();
  void NotifyDestroying();
  void NotifyDeepLinkReceived(const GURL& deep_link);
  void NotifyOpeningUrl(const GURL& url, bool in_background, bool from_server);
  void NotifyUrlOpened(const GURL& url, bool from_server);

  // AssistantStateObserver:
  void OnAssistantStatusChanged(mojom::AssistantState state) override;
  void OnLockedFullScreenStateChanged(bool enabled) override;

  // AssistantInterfaceBinder implementation:
  void BindController(
      mojo::PendingReceiver<chromeos::assistant::mojom::AssistantController>
          receiver) override;
  void BindAlarmTimerController(
      mojo::PendingReceiver<mojom::AssistantAlarmTimerController> receiver)
      override;
  void BindNotificationController(
      mojo::PendingReceiver<mojom::AssistantNotificationController> receiver)
      override;
  void BindScreenContextController(
      mojo::PendingReceiver<mojom::AssistantScreenContextController> receiver)
      override;
  void BindStateController(
      mojo::PendingReceiver<mojom::AssistantStateController> receiver) override;
  void BindVolumeControl(
      mojo::PendingReceiver<mojom::AssistantVolumeControl> receiver) override;

  // The observer list should be initialized early so that sub-controllers may
  // register as observers during their construction.
  base::ObserverList<AssistantControllerObserver> observers_;

  mojo::ReceiverSet<chromeos::assistant::mojom::AssistantController>
      assistant_controller_receivers_;

  mojo::Receiver<mojom::AssistantVolumeControl>
      assistant_volume_control_receiver_{this};
  mojo::RemoteSet<mojom::VolumeObserver> volume_observers_;

  mojo::Remote<chromeos::assistant::mojom::Assistant> assistant_;

  // Assistant sub-controllers.
  AssistantAlarmTimerController assistant_alarm_timer_controller_{this};
  AssistantInteractionController assistant_interaction_controller_{this};
  AssistantNotificationController assistant_notification_controller_{this};
  AssistantStateController assistant_state_controller_;
  AssistantScreenContextController assistant_screen_context_controller_{this};
  AssistantSetupController assistant_setup_controller_{this};
  AssistantSuggestionsController assistant_suggestions_controller_{this};
  AssistantUiController assistant_ui_controller_{this};
  std::unique_ptr<AssistantWebUiController> assistant_web_ui_controller_;

  AssistantViewDelegateImpl view_delegate_{this};

  base::WeakPtrFactory<AssistantController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_CONTROLLER_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_controller.h"

#include <algorithm>
#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/assistant/assistant_web_ui_controller.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/cpp/android_intent_helper.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/mojom/assistant_volume_control.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/utility/screenshot_controller.h"
#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/features.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "services/content/public/mojom/constants.mojom.h"
#include "services/content/public/mojom/navigable_contents_factory.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace ash {

AssistantController::AssistantController() {
  if (chromeos::assistant::features::IsAssistantWebContainerEnabled()) {
    assistant_web_ui_controller_ =
        std::make_unique<AssistantWebUiController>(this);
  }

  assistant_state_controller_.AddObserver(this);
  chromeos::CrasAudioHandler::Get()->AddAudioObserver(this);
  AddObserver(this);

  NotifyConstructed();
}

AssistantController::~AssistantController() {
  NotifyDestroying();

  chromeos::CrasAudioHandler::Get()->RemoveAudioObserver(this);
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
  assistant_state_controller_.RemoveObserver(this);
  RemoveObserver(this);
}

// static
void AssistantController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kAssistantNumWarmerWelcomeTriggered, 0);
}

void AssistantController::BindReceiver(
    mojo::PendingReceiver<chromeos::assistant::mojom::AssistantController>
        receiver) {
  assistant_controller_receivers_.Add(this, std::move(receiver));
}

void AssistantController::BindReceiver(
    mojo::PendingReceiver<mojom::AssistantVolumeControl> receiver) {
  assistant_volume_control_receiver_.Bind(std::move(receiver));
}

void AssistantController::AddObserver(AssistantControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void AssistantController::RemoveObserver(
    AssistantControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantController::SetAssistant(
    mojo::PendingRemote<chromeos::assistant::mojom::Assistant> assistant) {
  assistant_.Bind(std::move(assistant));

  // Provide reference to sub-controllers.
  assistant_alarm_timer_controller_.SetAssistant(assistant_.get());
  assistant_interaction_controller_.SetAssistant(assistant_.get());
  assistant_notification_controller_.SetAssistant(assistant_.get());
  assistant_screen_context_controller_.SetAssistant(assistant_.get());
  assistant_ui_controller_.SetAssistant(assistant_.get());

  // The Assistant service needs to have accessibility state synced with ash
  // and be notified of any accessibility status changes in the future to
  // provide an opportunity to turn on/off A11Y features.
  Shell::Get()->accessibility_controller()->AddObserver(this);
  OnAccessibilityStatusChanged();

  for (AssistantControllerObserver& observer : observers_)
    observer.OnAssistantReady();
}

void AssistantController::SendAssistantFeedback(
    bool assistant_debug_info_allowed,
    const std::string& feedback_description,
    const std::string& screenshot_png) {
  chromeos::assistant::mojom::AssistantFeedbackPtr assistant_feedback =
      chromeos::assistant::mojom::AssistantFeedback::New();
  assistant_feedback->assistant_debug_info_allowed =
      assistant_debug_info_allowed;
  assistant_feedback->description = feedback_description;
  assistant_feedback->screenshot_png = screenshot_png;
  assistant_->SendAssistantFeedback(std::move(assistant_feedback));
}

void AssistantController::StartSpeakerIdEnrollmentFlow() {
  if (assistant_state_controller_.consent_status().value_or(
          chromeos::assistant::prefs::ConsentStatus::kUnknown) ==
      chromeos::assistant::prefs::ConsentStatus::kActivityControlAccepted) {
    // If activity control has been accepted, launch the enrollment flow.
    setup_controller()->StartOnboarding(false /* relaunch */,
                                        FlowType::kSpeakerIdEnrollment);
  } else {
    // If activity control has not been accepted, launch the opt-in flow.
    setup_controller()->StartOnboarding(false /* relaunch */,
                                        FlowType::kConsentFlow);
  }
}

void AssistantController::DownloadImage(
    const GURL& url,
    AssistantImageDownloader::DownloadCallback callback) {
  const UserSession* user_session =
      Shell::Get()->session_controller()->GetUserSession(0);

  if (!user_session) {
    LOG(WARNING) << "Unable to retrieve active user session.";
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  AccountId account_id = user_session->user_info.account_id;
  AssistantImageDownloader::GetInstance()->Download(account_id, url,
                                                    std::move(callback));
}

void AssistantController::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  using assistant::util::DeepLinkParam;
  using assistant::util::DeepLinkType;

  switch (type) {
    case DeepLinkType::kChromeSettings: {
      // Chrome Settings deep links are opened in a new browser tab.
      OpenUrl(assistant::util::GetChromeSettingsUrl(
          assistant::util::GetDeepLinkParam(params, DeepLinkParam::kPage)));
      break;
    }
    case DeepLinkType::kFeedback:
      NewWindowDelegate::GetInstance()->OpenFeedbackPage(
          /*from_assistant=*/true);
      break;
    case DeepLinkType::kScreenshot:
      // We close the UI before taking the screenshot as it's probably not the
      // user's intention to include the Assistant in the picture.
      assistant_ui_controller_.CloseUi(AssistantExitPoint::kScreenshot);
      Shell::Get()->screenshot_controller()->TakeScreenshotForAllRootWindows();
      break;
    case DeepLinkType::kTaskManager:
      // Open task manager window.
      NewWindowDelegate::GetInstance()->ShowTaskManager();
      break;
    case DeepLinkType::kUnsupported:
    case DeepLinkType::kAlarmTimer:
    case DeepLinkType::kLists:
    case DeepLinkType::kNotes:
    case DeepLinkType::kOnboarding:
    case DeepLinkType::kQuery:
    case DeepLinkType::kReminders:
    case DeepLinkType::kSettings:
    case DeepLinkType::kWhatsOnMyScreen:
      // No action needed.
      break;
  }
}

void AssistantController::SetVolume(int volume, bool user_initiated) {
  volume = std::min(100, volume);
  volume = std::max(volume, 0);
  chromeos::CrasAudioHandler::Get()->SetOutputVolumePercent(volume);
}

void AssistantController::SetMuted(bool muted) {
  chromeos::CrasAudioHandler::Get()->SetOutputMute(muted);
}

void AssistantController::AddVolumeObserver(
    mojo::PendingRemote<mojom::VolumeObserver> observer) {
  volume_observers_.Add(std::move(observer));

  int output_volume =
      chromeos::CrasAudioHandler::Get()->GetOutputVolumePercent();
  bool mute = chromeos::CrasAudioHandler::Get()->IsOutputMuted();
  OnOutputMuteChanged(mute);
  OnOutputNodeVolumeChanged(0 /* node */, output_volume);
}

void AssistantController::OnOutputMuteChanged(bool mute_on) {
  for (auto& observer : volume_observers_)
    observer->OnMuteStateChanged(mute_on);
}

void AssistantController::OnOutputNodeVolumeChanged(uint64_t node, int volume) {
  // |node| refers to the active volume device, which we don't care here.
  for (auto& observer : volume_observers_)
    observer->OnVolumeChanged(volume);
}

void AssistantController::OnAccessibilityStatusChanged() {
  // The Assistant service needs to be informed of changes to accessibility
  // state so that it can turn on/off A11Y features appropriately.
  assistant_->OnAccessibilityStatusChanged(
      Shell::Get()->accessibility_controller()->spoken_feedback_enabled());
}

void AssistantController::OpenUrl(const GURL& url,
                                  bool in_background,
                                  bool from_server) {
  if (assistant::util::IsDeepLinkUrl(url)) {
    NotifyDeepLinkReceived(url);
    return;
  }

  auto* android_helper = AndroidIntentHelper::GetInstance();
  if (IsAndroidIntent(url) && !android_helper) {
    NOTREACHED();
    return;
  }

  // Give observers an opportunity to perform any necessary handling before we
  // open the specified |url| in a new browser tab.
  NotifyOpeningUrl(url, in_background, from_server);

  if (IsAndroidIntent(url)) {
    android_helper->LaunchAndroidIntent(url.spec());
  } else {
    // The new tab should be opened with a user activation since the user
    // interacted with the Assistant to open the url. |in_background| describes
    // the relationship between |url| and Assistant UI, not the browser. As
    // such, the browser will always be instructed to open |url| in a new
    // browser tab and Assistant UI state will be updated downstream to respect
    // |in_background|.
    NewWindowDelegate::GetInstance()->NewTabWithUrl(
        url, /*from_user_interaction=*/true);
  }
  NotifyUrlOpened(url, from_server);
}

void AssistantController::GetNavigableContentsFactory(
    mojo::PendingReceiver<content::mojom::NavigableContentsFactory> receiver) {
  const UserSession* user_session =
      Shell::Get()->session_controller()->GetUserSession(0);

  if (!user_session) {
    LOG(WARNING) << "Unable to retrieve active user session.";
    return;
  }

  const base::Optional<base::Token>& service_instance_group =
      user_session->user_info.service_instance_group;
  if (!service_instance_group) {
    LOG(ERROR) << "Unable to retrieve service instance group.";
    return;
  }

  Shell::Get()->connector()->Connect(
      service_manager::ServiceFilter::ByNameInGroup(
          content::mojom::kServiceName, *service_instance_group),
      std::move(receiver));
}

bool AssistantController::IsAssistantReady() const {
  return !!assistant_;
}

void AssistantController::NotifyConstructed() {
  for (AssistantControllerObserver& observer : observers_)
    observer.OnAssistantControllerConstructed();
}

void AssistantController::NotifyDestroying() {
  for (AssistantControllerObserver& observer : observers_)
    observer.OnAssistantControllerDestroying();
}

void AssistantController::NotifyDeepLinkReceived(const GURL& deep_link) {
  using assistant::util::DeepLinkType;

  // Retrieve deep link type and parsed parameters.
  DeepLinkType type = assistant::util::GetDeepLinkType(deep_link);
  const std::map<std::string, std::string> params =
      assistant::util::GetDeepLinkParams(deep_link);

  // TODO(wutao): Remove AssistantControllerObserver::OnDeepLinkReceived.
  for (AssistantControllerObserver& observer : observers_)
    observer.OnDeepLinkReceived(type, params);

  view_delegate_.NotifyDeepLinkReceived(type, params);
}

void AssistantController::NotifyOpeningUrl(const GURL& url,
                                           bool in_background,
                                           bool from_server) {
  for (AssistantControllerObserver& observer : observers_)
    observer.OnOpeningUrl(url, in_background, from_server);
}

void AssistantController::NotifyUrlOpened(const GURL& url, bool from_server) {
  for (AssistantControllerObserver& observer : observers_)
    observer.OnUrlOpened(url, from_server);
}

void AssistantController::OnAssistantStatusChanged(
    mojom::AssistantState state) {
  if (state == mojom::AssistantState::NOT_READY)
    assistant_ui_controller_.CloseUi(AssistantExitPoint::kUnspecified);
}

void AssistantController::OnLockedFullScreenStateChanged(bool enabled) {
  if (enabled)
    assistant_ui_controller_.CloseUi(AssistantExitPoint::kUnspecified);
}

void AssistantController::BindController(
    mojo::PendingReceiver<chromeos::assistant::mojom::AssistantController>
        receiver) {
  BindReceiver(std::move(receiver));
}

void AssistantController::BindAlarmTimerController(
    mojo::PendingReceiver<mojom::AssistantAlarmTimerController> receiver) {
  Shell::Get()->assistant_controller()->alarm_timer_controller()->BindReceiver(
      std::move(receiver));
}

void AssistantController::BindNotificationController(
    mojo::PendingReceiver<mojom::AssistantNotificationController> receiver) {
  Shell::Get()->assistant_controller()->notification_controller()->BindReceiver(
      std::move(receiver));
}

void AssistantController::BindScreenContextController(
    mojo::PendingReceiver<mojom::AssistantScreenContextController> receiver) {
  Shell::Get()
      ->assistant_controller()
      ->screen_context_controller()
      ->BindReceiver(std::move(receiver));
}

void AssistantController::BindStateController(
    mojo::PendingReceiver<mojom::AssistantStateController> receiver) {
  assistant_state_controller_.BindReceiver(std::move(receiver));
}

void AssistantController::BindVolumeControl(
    mojo::PendingReceiver<mojom::AssistantVolumeControl> receiver) {
  Shell::Get()->assistant_controller()->BindReceiver(std::move(receiver));
}

base::WeakPtr<AssistantController> AssistantController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace ash

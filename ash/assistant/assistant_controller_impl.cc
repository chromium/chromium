// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_controller_impl.h"

#include <algorithm>
#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/cpp/android_intent_helper.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/mojom/assistant_volume_control.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/utility/screenshot_controller.h"
#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "components/prefs/pref_registry_simple.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace ash {

AssistantControllerImpl::AssistantControllerImpl() {
  assistant_state_controller_.AddObserver(this);
  chromeos::CrasAudioHandler::Get()->AddAudioObserver(this);
  AddObserver(this);

  // The Assistant service needs to have accessibility state synced with ash
  // and be notified of any accessibility status changes in the future to
  // provide an opportunity to turn on/off A11Y features.
  Shell::Get()->accessibility_controller()->AddObserver(this);

  NotifyConstructed();
}

AssistantControllerImpl::~AssistantControllerImpl() {
  NotifyDestroying();

  chromeos::CrasAudioHandler::Get()->RemoveAudioObserver(this);
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
  assistant_state_controller_.RemoveObserver(this);
  RemoveObserver(this);
}

// static
void AssistantControllerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  AssistantInteractionControllerImpl::RegisterProfilePrefs(registry);
  AssistantUiControllerImpl::RegisterProfilePrefs(registry);
}

void AssistantControllerImpl::BindReceiver(
    mojo::PendingReceiver<mojom::AssistantVolumeControl> receiver) {
  assistant_volume_control_receiver_.Bind(std::move(receiver));
}

void AssistantControllerImpl::SetAssistant(
    chromeos::assistant::Assistant* assistant) {
  assistant_ = assistant;

  // Provide reference to sub-controllers.
  assistant_alarm_timer_controller_.SetAssistant(assistant);
  assistant_interaction_controller_.SetAssistant(assistant);
  assistant_notification_controller_.SetAssistant(assistant);
  assistant_screen_context_controller_.SetAssistant(assistant);
  assistant_ui_controller_.SetAssistant(assistant);

  OnAccessibilityStatusChanged();

  if (assistant) {
    for (AssistantControllerObserver& observer : observers_)
      observer.OnAssistantReady();
  }
}

void AssistantControllerImpl::SendAssistantFeedback(
    bool assistant_debug_info_allowed,
    const std::string& feedback_description,
    const std::string& screenshot_png) {
  chromeos::assistant::AssistantFeedback assistant_feedback;
  assistant_feedback.assistant_debug_info_allowed =
      assistant_debug_info_allowed;
  assistant_feedback.description = feedback_description;
  assistant_feedback.screenshot_png = screenshot_png;
  assistant_->SendAssistantFeedback(std::move(assistant_feedback));
}

void AssistantControllerImpl::StartSpeakerIdEnrollmentFlow() {
  setup_controller()->StartOnboarding(false /* relaunch */,
                                      FlowType::kSpeakerIdEnrollment);
}

void AssistantControllerImpl::DownloadImage(
    const GURL& url,
    ImageDownloader::DownloadCallback callback) {
  constexpr net::NetworkTrafficAnnotationTag kNetworkTrafficAnnotationTag =
      net::DefineNetworkTrafficAnnotation("image_downloader", R"(
            "semantics: {
              sender: "Google Assistant"
              description:
                "The Google Assistant requires dynamic loading of images to "
                "provide a media rich user experience. Images are downloaded "
                "on an as needed basis."
              trigger:
                "Generally triggered in direct response to a user issued "
                "query. A single query may necessitate the downloading of "
                "multiple images."
              destination: GOOGLE_OWNED_SERVICE
            }
            "policy": {
              cookies_allowed: NO
              setting:
                "The Google Assistant can be enabled/disabled in Chrome "
                "Settings and is subject to eligibility requirements."
            })");

  ImageDownloader::Get()->Download(url, kNetworkTrafficAnnotationTag,
                                   std::move(callback));
}

void AssistantControllerImpl::AddObserver(
    AssistantControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void AssistantControllerImpl::RemoveObserver(
    AssistantControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantControllerImpl::OpenUrl(const GURL& url,
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

void AssistantControllerImpl::OpenAssistantSettings() {
  // Launch Assistant settings via deeplink.
  OpenUrl(assistant::util::CreateAssistantSettingsDeepLink(),
          /*in_background=*/false, /*from_server=*/false);
}

base::WeakPtr<ash::AssistantController> AssistantControllerImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AssistantControllerImpl::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  using assistant::util::DeepLinkParam;
  using assistant::util::DeepLinkType;

  switch (type) {
    case DeepLinkType::kChromeSettings: {
      // Chrome Settings deep links are opened in a new browser tab.
      OpenUrl(
          assistant::util::GetChromeSettingsUrl(
              assistant::util::GetDeepLinkParam(params, DeepLinkParam::kPage)),
          /*in_background=*/false, /*from_server=*/false);
      break;
    }
    case DeepLinkType::kFeedback:
      NewWindowDelegate::GetInstance()->OpenFeedbackPage(
          /*from_assistant=*/true);

      // Close the assistant UI so that the feedback page is visible.
      assistant_ui_controller_.CloseUi(
          chromeos::assistant::AssistantExitPoint::kUnspecified);
      break;
    case DeepLinkType::kScreenshot:
      // We close the UI before taking the screenshot as it's probably not the
      // user's intention to include the Assistant in the picture.
      assistant_ui_controller_.CloseUi(
          chromeos::assistant::AssistantExitPoint::kScreenshot);
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
    case DeepLinkType::kProactiveSuggestions:
    case DeepLinkType::kQuery:
    case DeepLinkType::kReminders:
    case DeepLinkType::kSettings:
    case DeepLinkType::kWhatsOnMyScreen:
      // No action needed.
      break;
  }
}

void AssistantControllerImpl::SetVolume(int volume, bool user_initiated) {
  volume = std::min(100, volume);
  volume = std::max(volume, 0);
  chromeos::CrasAudioHandler::Get()->SetOutputVolumePercent(volume);
}

void AssistantControllerImpl::SetMuted(bool muted) {
  chromeos::CrasAudioHandler::Get()->SetOutputMute(muted);
}

void AssistantControllerImpl::AddVolumeObserver(
    mojo::PendingRemote<mojom::VolumeObserver> observer) {
  volume_observers_.Add(std::move(observer));

  int output_volume =
      chromeos::CrasAudioHandler::Get()->GetOutputVolumePercent();
  bool mute = chromeos::CrasAudioHandler::Get()->IsOutputMuted();
  OnOutputMuteChanged(mute);
  OnOutputNodeVolumeChanged(0 /* node */, output_volume);
}

void AssistantControllerImpl::OnOutputMuteChanged(bool mute_on) {
  for (auto& observer : volume_observers_)
    observer->OnMuteStateChanged(mute_on);
}

void AssistantControllerImpl::OnOutputNodeVolumeChanged(uint64_t node,
                                                        int volume) {
  // |node| refers to the active volume device, which we don't care here.
  for (auto& observer : volume_observers_)
    observer->OnVolumeChanged(volume);
}

void AssistantControllerImpl::OnAccessibilityStatusChanged() {
  if (!assistant_)
    return;

  // The Assistant service needs to be informed of changes to accessibility
  // state so that it can turn on/off A11Y features appropriately.
  assistant_->OnAccessibilityStatusChanged(
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled());
}

bool AssistantControllerImpl::IsAssistantReady() const {
  return !!assistant_;
}

void AssistantControllerImpl::NotifyConstructed() {
  for (AssistantControllerObserver& observer : observers_)
    observer.OnAssistantControllerConstructed();
}

void AssistantControllerImpl::NotifyDestroying() {
  for (AssistantControllerObserver& observer : observers_)
    observer.OnAssistantControllerDestroying();
}

void AssistantControllerImpl::NotifyDeepLinkReceived(const GURL& deep_link) {
  using assistant::util::DeepLinkType;

  // Retrieve deep link type and parsed parameters.
  DeepLinkType type = assistant::util::GetDeepLinkType(deep_link);
  const std::map<std::string, std::string> params =
      assistant::util::GetDeepLinkParams(deep_link);

  for (AssistantControllerObserver& observer : observers_)
    observer.OnDeepLinkReceived(type, params);
}

void AssistantControllerImpl::NotifyOpeningUrl(const GURL& url,
                                               bool in_background,
                                               bool from_server) {
  for (AssistantControllerObserver& observer : observers_)
    observer.OnOpeningUrl(url, in_background, from_server);
}

void AssistantControllerImpl::NotifyUrlOpened(const GURL& url,
                                              bool from_server) {
  for (AssistantControllerObserver& observer : observers_)
    observer.OnUrlOpened(url, from_server);
}

void AssistantControllerImpl::OnAssistantStatusChanged(
    chromeos::assistant::AssistantStatus status) {
  if (status == chromeos::assistant::AssistantStatus::NOT_READY)
    assistant_ui_controller_.CloseUi(
        chromeos::assistant::AssistantExitPoint::kUnspecified);
}

void AssistantControllerImpl::OnLockedFullScreenStateChanged(bool enabled) {
  if (enabled)
    assistant_ui_controller_.CloseUi(
        chromeos::assistant::AssistantExitPoint::kUnspecified);
}

void AssistantControllerImpl::BindVolumeControl(
    mojo::PendingReceiver<mojom::AssistantVolumeControl> receiver) {
  Shell::Get()->assistant_controller()->BindReceiver(std::move(receiver));
}

}  // namespace ash

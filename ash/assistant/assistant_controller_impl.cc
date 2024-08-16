// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_controller_impl.h"

#include <algorithm>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/android_intent_helper.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/mojom/assistant_volume_control.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_browser_delegate.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_feedback.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace ash {

namespace {

const AccountId& GetActiveUserAccountId() {
  const UserSession* active_user_session =
      Shell::Get()->session_controller()->GetUserSession(0);
  DCHECK(active_user_session);
  return active_user_session->user_info.account_id;
}

}  // namespace

AssistantControllerImpl::AssistantControllerImpl() {
  Shell::Get()->AddShellObserver(this);
  assistant_state_controller_.AddObserver(this);
  CrasAudioHandler::Get()->AddAudioObserver(this);
  AddObserver(this);

  // The Assistant service needs to have accessibility state synced with ash
  // and be notified of any accessibility status changes in the future to
  // provide an opportunity to turn on/off A11Y features.
  Shell::Get()->accessibility_controller()->AddObserver(this);

  color_mode_observer_.Observe(DarkLightModeControllerImpl::Get());

  NotifyConstructed();
}

AssistantControllerImpl::~AssistantControllerImpl() = default;

// static
void AssistantControllerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  AssistantInteractionControllerImpl::RegisterProfilePrefs(registry);
  AssistantUiControllerImpl::RegisterProfilePrefs(registry);
}

void AssistantControllerImpl::BindReceiver(
    mojo::PendingReceiver<mojom::AssistantVolumeControl> receiver) {
  if (assistant_volume_control_receiver_.is_bound()) {
    assistant_volume_control_receiver_.reset();
  }
  assistant_volume_control_receiver_.Bind(std::move(receiver));
}

void AssistantControllerImpl::SetAssistant(assistant::Assistant* assistant) {
  assistant_ = assistant;

  // Provide reference to sub-controllers.
  assistant_alarm_timer_controller_.SetAssistant(assistant);
  assistant_interaction_controller_.SetAssistant(assistant);
  assistant_notification_controller_.SetAssistant(assistant);
  assistant_ui_controller_.SetAssistant(assistant);

  if (assistant) {
    for (AssistantControllerObserver& observer : observers_)
      observer.OnAssistantReady();
  }
}

void AssistantControllerImpl::SendAssistantFeedback(
    bool assistant_debug_info_allowed,
    const std::string& feedback_description,
    const std::string& screenshot_png) {
  if (!IsAssistantReady()) {
    return;
  }

  assistant::AssistantFeedback assistant_feedback;
  assistant_feedback.assistant_debug_info_allowed =
      assistant_debug_info_allowed;
  assistant_feedback.description = feedback_description;
  assistant_feedback.screenshot_png =
      std::vector<uint8_t>(screenshot_png.begin(), screenshot_png.end());
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
            semantics {
              sender: "Google Assistant"
              description:
                "The Google Assistant requires dynamic loading of images to "
                "provide a media rich user experience. Images are downloaded "
                "on an as needed basis."
              trigger:
                "Generally triggered in direct response to a user issued "
                "query. A single query may necessitate the downloading of "
                "multiple images."
              data: "None."
              destination: GOOGLE_OWNED_SERVICE
            }
            policy {
              cookies_allowed: NO
              setting:
                "The Google Assistant can be enabled/disabled in Chrome "
                "Settings and is subject to eligibility requirements."
              policy_exception_justification:
                "The users can disable this feature. This does not send/store "
                "user data."
            })");

  ImageDownloader::Get()->Download(url, kNetworkTrafficAnnotationTag,
                                   GetActiveUserAccountId(),
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
  // app_list search result will be opened by `OpenUrl()`. However, the
  // `assistant_` may not be ready. Show a toast to indicate it.
  if (!IsAssistantReady()) {
    assistant_ui_controller_.ShowUnboundErrorToast();
    return;
  }

  if (assistant::util::IsDeepLinkUrl(url)) {
    NotifyDeepLinkReceived(url);
    return;
  }

  auto* android_helper = AndroidIntentHelper::GetInstance();
  if (IsAndroidIntent(url) && !android_helper) {
    NOTREACHED();
  }

  // Give observers an opportunity to perform any necessary handling before we
  // open the specified |url| in a new browser tab.
  NotifyOpeningUrl(url, in_background, from_server);

  if (IsAndroidIntent(url)) {
    android_helper->LaunchAndroidIntent(url.spec());
  } else {
    assistant::AssistantBrowserDelegate::Get()->OpenUrl(url);
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
          NewWindowDelegate::FeedbackSource::kFeedbackSourceAssistant);

      // Close the assistant UI so that the feedback page is visible.
      assistant_ui_controller_.CloseUi(
          assistant::AssistantExitPoint::kUnspecified);
      break;
    case DeepLinkType::kScreenshot:
      // We close the UI before taking the screenshot as it's probably not the
      // user's intention to include the Assistant in the picture.
      assistant_ui_controller_.CloseUi(
          assistant::AssistantExitPoint::kScreenshot);
      CaptureModeController::Get()->CaptureScreenshotsOfAllDisplays();
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
      // No action needed.
      break;
  }
}

void AssistantControllerImpl::SetVolume(int volume, bool user_initiated) {
  volume = std::min(100, volume);
  volume = std::max(volume, 0);
  CrasAudioHandler::Get()->SetOutputVolumePercent(volume);
}

void AssistantControllerImpl::SetMuted(bool muted) {
  CrasAudioHandler::Get()->SetOutputMute(muted);
}

void AssistantControllerImpl::AddVolumeObserver(
    mojo::PendingRemote<mojom::VolumeObserver> observer) {
  volume_observers_.Add(std::move(observer));

  int output_volume = CrasAudioHandler::Get()->GetOutputVolumePercent();
  bool mute = CrasAudioHandler::Get()->IsOutputMuted();
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
  if (!IsAssistantReady()) {
    return;
  }

  // The Assistant service needs to be informed of changes to accessibility
  // state so that it can turn on/off A11Y features appropriately.
  assistant_->OnAccessibilityStatusChanged(
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled());
}

void AssistantControllerImpl::OnColorModeChanged(bool dark_mode_enabled) {
  if (!IsAssistantReady()) {
    return;
  }

  assistant_->OnColorModeChanged(dark_mode_enabled);
}

void AssistantControllerImpl::OnShellDestroying() {
  NotifyDestroying();
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
  assistant_state_controller_.RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  RemoveObserver(this);
}

bool AssistantControllerImpl::IsAssistantReady() const {
  if (!assistant_) {
    return false;
  }

  if (AssistantState::Get()->assistant_status() ==
      assistant::AssistantStatus::NOT_READY) {
    return false;
  }

  return true;
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
    assistant::AssistantStatus status) {
  switch (status) {
    case assistant::AssistantStatus::NOT_READY:
      assistant_volume_control_receiver_.reset();
      assistant_ui_controller_.CloseUi(
          assistant::AssistantExitPoint::kUnspecified);
      break;
    case assistant::AssistantStatus::READY:
      OnAccessibilityStatusChanged();
      OnColorModeChanged(
          DarkLightModeControllerImpl::Get()->IsDarkModeEnabled());
      break;
  }
}

void AssistantControllerImpl::OnLockedFullScreenStateChanged(bool enabled) {
  if (enabled)
    assistant_ui_controller_.CloseUi(
        assistant::AssistantExitPoint::kUnspecified);
}

void AssistantControllerImpl::BindVolumeControl(
    mojo::PendingReceiver<mojom::AssistantVolumeControl> receiver) {
  Shell::Get()->assistant_controller()->BindReceiver(std::move(receiver));
}

}  // namespace ash

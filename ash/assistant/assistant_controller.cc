// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_controller.h"

#include <algorithm>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/assistant/assistant_cache_controller.h"
#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/assistant_notification_controller.h"
#include "ash/assistant/assistant_screen_context_controller.h"
#include "ash/assistant/assistant_setup_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/new_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/utility/screenshot_controller.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"

namespace ash {

AssistantController::AssistantController()
    : assistant_volume_control_binding_(this),
      assistant_cache_controller_(
          std::make_unique<AssistantCacheController>(this)),
      assistant_interaction_controller_(
          std::make_unique<AssistantInteractionController>(this)),
      assistant_notification_controller_(
          std::make_unique<AssistantNotificationController>(this)),
      assistant_screen_context_controller_(
          std::make_unique<AssistantScreenContextController>(this)),
      assistant_setup_controller_(
          std::make_unique<AssistantSetupController>(this)),
      assistant_ui_controller_(std::make_unique<AssistantUiController>(this)),
      voice_interaction_binding_(this),
      weak_factory_(this) {
  mojom::VoiceInteractionObserverPtr ptr;
  voice_interaction_binding_.Bind(mojo::MakeRequest(&ptr));
  Shell::Get()->voice_interaction_controller()->AddObserver(std::move(ptr));
  chromeos::CrasAudioHandler::Get()->AddAudioObserver(this);
  AddObserver(this);

  NotifyConstructed();
}

AssistantController::~AssistantController() {
  NotifyDestroying();

  chromeos::CrasAudioHandler::Get()->RemoveAudioObserver(this);
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
  RemoveObserver(this);
}

void AssistantController::BindRequest(
    mojom::AssistantControllerRequest request) {
  assistant_controller_bindings_.AddBinding(this, std::move(request));
}

void AssistantController::BindRequest(
    mojom::AssistantVolumeControlRequest request) {
  assistant_volume_control_binding_.Bind(std::move(request));
}

void AssistantController::AddObserver(AssistantControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void AssistantController::RemoveObserver(
    AssistantControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantController::SetAssistant(
    chromeos::assistant::mojom::AssistantPtr assistant) {
  assistant_ = std::move(assistant);

  // Provide reference to sub-controllers.
  assistant_interaction_controller_->SetAssistant(assistant_.get());
  assistant_notification_controller_->SetAssistant(assistant_.get());
  assistant_screen_context_controller_->SetAssistant(assistant_.get());
  assistant_ui_controller_->SetAssistant(assistant_.get());

  // The Assistant service needs to have accessibility state synced with ash
  // and be notified of any accessibility status changes in the future to
  // provide an opportunity to turn on/off A11Y features.
  Shell::Get()->accessibility_controller()->AddObserver(this);
  OnAccessibilityStatusChanged();
}

void AssistantController::SetAssistantImageDownloader(
    mojom::AssistantImageDownloaderPtr assistant_image_downloader) {
  assistant_image_downloader_ = std::move(assistant_image_downloader);
}

// TODO(dmblack): Call SetAssistantSetup directly on AssistantSetupController
// instead of going through AssistantController.
void AssistantController::SetAssistantSetup(
    mojom::AssistantSetupPtr assistant_setup) {
  assistant_setup_ = std::move(assistant_setup);
  assistant_setup_controller_->SetAssistantSetup(assistant_setup_.get());
}

void AssistantController::SetWebContentsManager(
    mojom::WebContentsManagerPtr web_contents_manager) {
  web_contents_manager_ = std::move(web_contents_manager);
}

// TODO(dmblack): Expose AssistantScreenContextController over mojo rather
// than implementing RequestScreenshot here in AssistantController.
void AssistantController::RequestScreenshot(
    const gfx::Rect& rect,
    RequestScreenshotCallback callback) {
  assistant_screen_context_controller_->RequestScreenshot(rect,
                                                          std::move(callback));
}

void AssistantController::OpenAssistantSettings() {
  // Launch Assistant settings via deeplink.
  OpenUrl(assistant::util::CreateAssistantSettingsDeepLink());
}

void AssistantController::ManageWebContents(
    const base::UnguessableToken& id_token,
    mojom::ManagedWebContentsParamsPtr params,
    mojom::WebContentsManager::ManageWebContentsCallback callback) {
  DCHECK(web_contents_manager_);

  const mojom::UserSession* user_session =
      Shell::Get()->session_controller()->GetUserSession(0);

  if (!user_session) {
    LOG(WARNING) << "Unable to retrieve active user session.";
    std::move(callback).Run(base::nullopt);
    return;
  }

  // Supply account ID.
  params->account_id = user_session->user_info->account_id;

  // Specify that we will handle top level browser requests.
  ash::mojom::ManagedWebContentsOpenUrlDelegatePtr ptr;
  web_contents_open_url_delegate_bindings_.AddBinding(this,
                                                      mojo::MakeRequest(&ptr));
  params->open_url_delegate_ptr_info = ptr.PassInterface();

  web_contents_manager_->ManageWebContents(id_token, std::move(params),
                                           std::move(callback));
}

void AssistantController::ReleaseWebContents(
    const base::UnguessableToken& id_token) {
  web_contents_manager_->ReleaseWebContents(id_token);
}

void AssistantController::ReleaseWebContents(
    const std::vector<base::UnguessableToken>& id_tokens) {
  web_contents_manager_->ReleaseAllWebContents(id_tokens);
}

void AssistantController::NavigateWebContentsBack(
    const base::UnguessableToken& id_token,
    mojom::WebContentsManager::NavigateWebContentsBackCallback callback) {
  web_contents_manager_->NavigateWebContentsBack(id_token, std::move(callback));
}

void AssistantController::DownloadImage(
    const GURL& url,
    mojom::AssistantImageDownloader::DownloadCallback callback) {
  DCHECK(assistant_image_downloader_);

  const mojom::UserSession* user_session =
      Shell::Get()->session_controller()->GetUserSession(0);

  if (!user_session) {
    LOG(WARNING) << "Unable to retrieve active user session.";
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  AccountId account_id = user_session->user_info->account_id;
  assistant_image_downloader_->Download(account_id, url, std::move(callback));
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
      // TODO(dmblack): Possibly use a new FeedbackSource (this method defaults
      // to kFeedbackSourceAsh). This may be useful for differentiating feedback
      // UI and behavior for Assistant.
      Shell::Get()->new_window_controller()->OpenFeedbackPage();
      break;
    case DeepLinkType::kScreenshot:
      // We close the UI before taking the screenshot as it's probably not the
      // user's intention to include the Assistant in the picture.
      assistant_ui_controller_->CloseUi(AssistantSource::kUnspecified);
      Shell::Get()->screenshot_controller()->TakeScreenshotForAllRootWindows();
      break;
    case DeepLinkType::kTaskManager:
      // Open task manager window.
      Shell::Get()->new_window_controller()->ShowTaskManager();
      break;
    case DeepLinkType::kUnsupported:
    case DeepLinkType::kOnboarding:
    case DeepLinkType::kQuery:
    case DeepLinkType::kReminders:
    case DeepLinkType::kSettings:
    case DeepLinkType::kWhatsOnMyScreen:
      // No action needed.
      break;
  }
}

void AssistantController::ShouldOpenUrlFromTab(
    const GURL& url,
    WindowOpenDisposition disposition,
    ash::mojom::ManagedWebContentsOpenUrlDelegate::ShouldOpenUrlFromTabCallback
        callback) {
  // We always handle deep links ourselves.
  if (assistant::util::IsDeepLinkUrl(url)) {
    std::move(callback).Run(/*should_open=*/false);
    NotifyDeepLinkReceived(url);
    return;
  }

  AssistantUiMode ui_mode = assistant_ui_controller_->model()->ui_mode();

  // When in main UI mode, WebContents should not navigate as they are hosting
  // Assistant cards. Instead, we route navigation attempts to the browser. We
  // also respect open |disposition| to launch in the browser if appropriate.
  if (ui_mode == AssistantUiMode::kMainUi ||
      disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB) {
    std::move(callback).Run(/*should_open=*/false);
    OpenUrl(url);
    return;
  }

  // In all other cases WebContents should be able to navigate freely.
  std::move(callback).Run(/*should_open=*/true);
}

void AssistantController::SetVolume(int volume, bool user_initiated) {
  volume = std::min(100, volume);
  volume = std::max(volume, 0);
  chromeos::CrasAudioHandler::Get()->SetOutputVolumePercent(volume);
}

void AssistantController::SetMuted(bool muted) {
  chromeos::CrasAudioHandler::Get()->SetOutputMute(muted);
}

void AssistantController::AddVolumeObserver(mojom::VolumeObserverPtr observer) {
  volume_observer_.AddPtr(std::move(observer));

  int output_volume =
      chromeos::CrasAudioHandler::Get()->GetOutputVolumePercent();
  bool mute = chromeos::CrasAudioHandler::Get()->IsOutputMuted();
  OnOutputMuteChanged(mute, false /* system_adjust */);
  OnOutputNodeVolumeChanged(0 /* node */, output_volume);
}

void AssistantController::OnOutputMuteChanged(bool mute_on,
                                              bool system_adjust) {
  volume_observer_.ForAllPtrs([mute_on](mojom::VolumeObserver* observer) {
    observer->OnMuteStateChanged(mute_on);
  });
}

void AssistantController::OnOutputNodeVolumeChanged(uint64_t node, int volume) {
  // |node| refers to the active volume device, which we don't care here.
  volume_observer_.ForAllPtrs([volume](mojom::VolumeObserver* observer) {
    observer->OnVolumeChanged(volume);
  });
}

void AssistantController::OnAccessibilityStatusChanged() {
  // The Assistant service needs to be informed of changes to accessibility
  // state so that it can turn on/off A11Y features appropriately.
  assistant_->OnAccessibilityStatusChanged(
      Shell::Get()->accessibility_controller()->IsSpokenFeedbackEnabled());
}

void AssistantController::OpenUrl(const GURL& url, bool from_server) {
  if (assistant::util::IsDeepLinkUrl(url)) {
    NotifyDeepLinkReceived(url);
    return;
  }

  // The new tab should be opened with a user activation since the user
  // interacted with the Assistant to open the url.
  Shell::Get()->new_window_controller()->NewTabWithUrl(
      url, /*from_user_interaction=*/true);
  NotifyUrlOpened(url, from_server);
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

  for (AssistantControllerObserver& observer : observers_)
    observer.OnDeepLinkReceived(type, params);
}

void AssistantController::NotifyUrlOpened(const GURL& url, bool from_server) {
  for (AssistantControllerObserver& observer : observers_)
    observer.OnUrlOpened(url, from_server);
}

void AssistantController::OnVoiceInteractionStatusChanged(
    mojom::VoiceInteractionState state) {
  if (state == mojom::VoiceInteractionState::NOT_READY)
    assistant_ui_controller_->HideUi(AssistantSource::kUnspecified);
}

base::WeakPtr<AssistantController> AssistantController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace ash

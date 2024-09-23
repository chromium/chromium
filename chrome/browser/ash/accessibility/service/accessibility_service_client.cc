// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/service/accessibility_service_client.h"

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/uuid.h"
#include "chrome/browser/accessibility/service/accessibility_service_router.h"
#include "chrome/browser/accessibility/service/accessibility_service_router_factory.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/service/accessibility_service_devtools_delegate.h"
#include "chrome/browser/ash/accessibility/service/autoclick_client_impl.h"
#include "chrome/browser/ash/accessibility/service/automation_client_impl.h"
#include "chrome/browser/ash/accessibility/service/speech_recognition_impl.h"
#include "chrome/browser/ash/accessibility/service/tts_client_impl.h"
#include "chrome/browser/ash/accessibility/service/user_input_impl.h"
#include "chrome/browser/ash/accessibility/service/user_interface_impl.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"

namespace ash {
namespace {

const char kAccessibilityCommonFilesPath[] = "chromeos/accessibility";

base::File LoadFile(base::FilePath path) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::FilePath resources_path;
  if (!base::PathService::Get(chrome::DIR_RESOURCES, &resources_path)) {
    NOTREACHED_IN_MIGRATION();
  }

  base::FilePath accessibility_file_path =
      resources_path.Append(kAccessibilityCommonFilesPath).Append(path);
  base::File file(accessibility_file_path,
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  return file;
}

}  // namespace

AccessibilityServiceClient::AccessibilityServiceClient() = default;

AccessibilityServiceClient::~AccessibilityServiceClient() {
  Reset();
}

void AccessibilityServiceClient::BindAutomation(
    mojo::PendingAssociatedRemote<ax::mojom::Automation> automation) {
  automation_client_->BindAutomation(std::move(automation));
}

void AccessibilityServiceClient::BindAutomationClient(
    mojo::PendingReceiver<ax::mojom::AutomationClient> automation_client) {
  automation_client_->BindAutomationClient(std::move(automation_client));
}

void AccessibilityServiceClient::BindAutoclickClient(
    mojo::PendingReceiver<ax::mojom::AutoclickClient> autoclick_receiver) {
  autoclick_client_->Bind(std::move(autoclick_receiver));
}

void AccessibilityServiceClient::BindSpeechRecognition(
    mojo::PendingReceiver<ax::mojom::SpeechRecognition> sr_receiver) {
  speech_recognition_impl_->Bind(std::move(sr_receiver));
}

void AccessibilityServiceClient::BindTts(
    mojo::PendingReceiver<ax::mojom::Tts> tts_receiver) {
  tts_client_->Bind(std::move(tts_receiver));
}

void AccessibilityServiceClient::BindUserInput(
    mojo::PendingReceiver<ax::mojom::UserInput> ui_receiver) {
  user_input_client_->Bind(std::move(ui_receiver));
}

void AccessibilityServiceClient::BindUserInterface(
    mojo::PendingReceiver<ax::mojom::UserInterface> ui_receiver) {
  user_interface_client_->Bind(std::move(ui_receiver));
}

void AccessibilityServiceClient::BindAccessibilityFileLoader(
    mojo::PendingReceiver<ax::mojom::AccessibilityFileLoader>
        file_loader_receiver) {
  CHECK(!file_loader_.is_bound());
  file_loader_.Bind(std::move(file_loader_receiver));
}

void AccessibilityServiceClient::Load(const base::FilePath& path,
                                      LoadCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&LoadFile, path),
      base::BindOnce(&AccessibilityServiceClient::OnFileLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AccessibilityServiceClient::SetProfile(content::BrowserContext* profile) {
  // If the profile has changed we will need to disconnect from the previous
  // service, get the service keyed to this profile, and if any features were
  // enabled, re-establish the service connection with those features. Note that
  // this matches behavior in AccessibilityExtensionLoader::SetProfile, which
  // does the parallel logic with the extension system.
  if (profile_ == profile)
    return;

  Reset();
  profile_ = profile;
  if (profile_ && enabled_features_.size())
    LaunchAccessibilityServiceAndBind();
}

void AccessibilityServiceClient::SetChromeVoxEnabled(bool enabled) {
  EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType::kChromeVox,
                            enabled);
}

void AccessibilityServiceClient::SetSelectToSpeakEnabled(bool enabled) {
  EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType::kSelectToSpeak,
                            enabled);
}

void AccessibilityServiceClient::SetSwitchAccessEnabled(bool enabled) {
  EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType::kSwitchAccess,
                            enabled);
}

void AccessibilityServiceClient::SetAutoclickEnabled(bool enabled) {
  EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType::kAutoClick,
                            enabled);
}

void AccessibilityServiceClient::SetMagnifierEnabled(bool enabled) {
  EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType::kMagnifier,
                            enabled);
}

void AccessibilityServiceClient::SetDictationEnabled(bool enabled) {
  EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType::kDictation,
                            enabled);
}

void AccessibilityServiceClient::RequestScrollableBoundsForPoint(
    const gfx::Point& point) {
  autoclick_client_->RequestScrollableBoundsForPoint(point);
}

void AccessibilityServiceClient::Reset() {
  at_controller_.reset();
  autoclick_client_.reset();
  file_loader_.reset();
  automation_client_.reset();
  devtools_agent_hosts_.clear();
  speech_recognition_impl_.reset();
  tts_client_.reset();
  user_input_client_.reset();
  user_interface_client_.reset();
}

void AccessibilityServiceClient::EnableAssistiveTechnology(
    ax::mojom::AssistiveTechnologyType type,
    bool enabled) {
  // Update the list of enabled features.
  auto iter =
      std::find(enabled_features_.begin(), enabled_features_.end(), type);
  // If a feature's state isn't being changed, do nothing.
  if ((enabled && iter != enabled_features_.end()) ||
      (!enabled && iter == enabled_features_.end())) {
    return;
  } else if (enabled && iter == enabled_features_.end()) {
    enabled_features_.push_back(type);
  } else if (!enabled && iter != enabled_features_.end()) {
    enabled_features_.erase(iter);
    AccessibilityManager::Get()->RemoveFocusRings(type);
  }

  // If nothing at all is enabled, ensure that automation gets disabled,
  // which will keep the system from collecting and passing a11y trees.
  // Note it is safe to call Disable multiple times in a row.
  if (enabled_features_.empty()) {
    automation_client_->Disable();
  }

  if (!enabled && !at_controller_.is_bound()) {
    // No need to launch the service, nothing is enabled.
    return;
  }

  if (at_controller_.is_bound()) {
    at_controller_->EnableAssistiveTechnology(enabled_features_);
    // Create or destroy devtools agent.
    if (enabled) {
      CreateDevToolsAgentHost(type);
    } else {
      auto it = devtools_agent_hosts_.find(type);
      if (it != devtools_agent_hosts_.end()) {
        // Detach all sessions before destroying.
        it->second->ForceDetachAllSessions();
        devtools_agent_hosts_.erase(it);
      }
    }
    return;
  }

  // A new feature is enabled but the service isn't running yet.
  LaunchAccessibilityServiceAndBind();
}

void AccessibilityServiceClient::LaunchAccessibilityServiceAndBind() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!profile_)
    return;

  ax::AccessibilityServiceRouter* router =
      ax::AccessibilityServiceRouterFactory::GetForBrowserContext(
          static_cast<content::BrowserContext*>(profile_));

  if (!router) {
    return;
  }

  autoclick_client_ = std::make_unique<AutoclickClientImpl>();
  automation_client_ = std::make_unique<AutomationClientImpl>();
  speech_recognition_impl_ = std::make_unique<SpeechRecognitionImpl>(profile_);
  tts_client_ = std::make_unique<TtsClientImpl>(profile_);
  user_input_client_ = std::make_unique<UserInputImpl>();
  user_interface_client_ = std::make_unique<UserInterfaceImpl>();

  // Bind the AXServiceClient before enabling features.
  router->BindAccessibilityServiceClient(
      service_client_.BindNewPipeAndPassRemote());
  router->BindAssistiveTechnologyController(
      at_controller_.BindNewPipeAndPassReceiver(), enabled_features_);
  // Create agent host for all enabled features.
  for (auto& type : enabled_features_) {
    CreateDevToolsAgentHost(type);
  }
}

void AccessibilityServiceClient::CreateDevToolsAgentHost(
    ax::mojom::AssistiveTechnologyType type) {
  auto host = content::DevToolsAgentHost::CreateForMojomDelegate(
      base::Uuid::GenerateRandomV4().AsLowercaseString(),
      // base::Unretained is safe because all agent hosts and
      // their delegates are deleted in the destructor of this class when
      // |hosts_| is cleared.
      std::make_unique<AccessibilityServiceDevToolsDelegate>(
          type,
          base::BindRepeating(&AccessibilityServiceClient::ConnectDevToolsAgent,
                              base::Unretained(this))));
  devtools_agent_hosts_.emplace(type, host);
}

void AccessibilityServiceClient::ConnectDevToolsAgent(
    ::mojo::PendingAssociatedReceiver<::blink::mojom::DevToolsAgent> agent,
    ax::mojom::AssistiveTechnologyType type) {
  if (!profile_) {
    return;
  }

  ax::AccessibilityServiceRouter* router =
      ax::AccessibilityServiceRouterFactory::GetForBrowserContext(
          static_cast<content::BrowserContext*>(profile_));
  if (router) {
    router->ConnectDevToolsAgent(std::move(agent), type);
  }
}

void AccessibilityServiceClient::OnFileLoaded(LoadCallback callback,
                                              base::File file) {
  std::move(callback).Run(std::move(file));
}

}  // namespace ash

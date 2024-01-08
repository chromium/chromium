// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_ACCESSIBILITY_SERVICE_CLIENT_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_ACCESSIBILITY_SERVICE_CLIENT_H_

#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "services/accessibility/public/mojom/file_loader.mojom.h"
#include "services/accessibility/public/mojom/speech_recognition.mojom-forward.h"
#include "services/accessibility/public/mojom/tts.mojom-forward.h"
#include "services/accessibility/public/mojom/user_interface.mojom-forward.h"
#include "ui/gfx/geometry/point.h"

namespace content {
class BrowserContext;
class DevToolsAgentHost;
}

namespace ash {
class AutoclickClientImpl;
class AutomationClientImpl;
class SpeechRecognitionImpl;
class TtsClientImpl;
class UserInputImpl;
class UserInterfaceImpl;

// The AccessibilityServiceClient in the Browser process interacts with the
// AccessibilityService process over mojom. It is responsible for communicating
// to the service which features are running and binding helper classes for the
// service.
// TODO(crbug.com/1355633): Move to ash/accessibility/service.
class AccessibilityServiceClient : public ax::mojom::AccessibilityServiceClient,
                                   public ax::mojom::AccessibilityFileLoader {
 public:
  AccessibilityServiceClient();
  AccessibilityServiceClient(const AccessibilityServiceClient&) = delete;
  AccessibilityServiceClient& operator=(const AccessibilityServiceClient&) =
      delete;
  ~AccessibilityServiceClient() override;

  // ax::mojom::AccessibilityServiceClient:
  void BindAutomation(
      mojo::PendingAssociatedRemote<ax::mojom::Automation> automation) override;
  void BindAutomationClient(mojo::PendingReceiver<ax::mojom::AutomationClient>
                                automation_client) override;
  void BindAutoclickClient(mojo::PendingReceiver<ax::mojom::AutoclickClient>
                               autoclick_receiver) override;
  void BindSpeechRecognition(
      mojo::PendingReceiver<ax::mojom::SpeechRecognition> sr_receiver) override;
  void BindTts(mojo::PendingReceiver<ax::mojom::Tts> tts_receiver) override;
  void BindUserInput(
      mojo::PendingReceiver<ax::mojom::UserInput> ui_receiver) override;
  void BindUserInterface(
      mojo::PendingReceiver<ax::mojom::UserInterface> ui_receiver) override;
  void BindAccessibilityFileLoader(
      mojo::PendingReceiver<ax::mojom::AccessibilityFileLoader>
          file_loader_receiver) override;

  // ax::mojom::AccessibilityFileLoader:
  void Load(const base::FilePath& path, LoadCallback callback) override;

  void SetProfile(content::BrowserContext* profile);

  // Enables or disables accessibility features in the service.
  void SetChromeVoxEnabled(bool enabled);
  void SetSelectToSpeakEnabled(bool enabled);
  void SetSwitchAccessEnabled(bool enabled);
  void SetAutoclickEnabled(bool enabled);
  void SetMagnifierEnabled(bool enabled);
  void SetDictationEnabled(bool enabled);

  // Sends information into the accessibility service.
  void RequestScrollableBoundsForPoint(const gfx::Point& point);

 private:
  friend class AccessibilityServiceClientTest;
  friend class AccessibilityManagerWithAccessibilityServiceTest;

  // Called when the profile changes or on destruction. Disconnects all mojom
  // endpoints.
  void Reset();

  void EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType type,
                                 bool enabled);

  void LaunchAccessibilityServiceAndBind();

  void CreateDevToolsAgentHost(ax::mojom::AssistiveTechnologyType type);

  // Function is used to create a callback that is passed into a
  // AccessibilityServiceDevToolsDelegate. It should not be called directly.
  void ConnectDevToolsAgent(
      ::mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent,
      ax::mojom::AssistiveTechnologyType type);

  // Helper method to return the opened file from the UI thread once it has been
  // opened.
  void OnFileLoaded(LoadCallback callback, base::File file);

  std::unique_ptr<AutoclickClientImpl> autoclick_client_;
  std::unique_ptr<AutomationClientImpl> automation_client_;
  std::unique_ptr<SpeechRecognitionImpl> speech_recognition_impl_;
  std::unique_ptr<TtsClientImpl> tts_client_;
  std::unique_ptr<UserInputImpl> user_input_client_;
  std::unique_ptr<UserInterfaceImpl> user_interface_client_;

  // Track the currently enabled features in case we disconnect from the service
  // and need to reconnect, for example when the profile changes.
  std::vector<ax::mojom::AssistiveTechnologyType> enabled_features_;

  raw_ptr<content::BrowserContext> profile_ = nullptr;

  // Here is the remote to the AT Controller, used to toggle features.
  mojo::Remote<ax::mojom::AssistiveTechnologyController> at_controller_;

  // This class receives mojom requests from the service via the interface
  // AccessibilityServiceClient.
  mojo::Receiver<ax::mojom::AccessibilityServiceClient> service_client_{this};

  // Loads files on demand by the accessibility service.
  mojo::Receiver<ax::mojom::AccessibilityFileLoader> file_loader_{this};

  // Container mapping AT type and devtools host.
  std::map<ax::mojom::AssistiveTechnologyType,
           scoped_refptr<content::DevToolsAgentHost>>
      devtools_agent_hosts_;

  base::WeakPtrFactory<AccessibilityServiceClient> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_ACCESSIBILITY_SERVICE_CLIENT_H_

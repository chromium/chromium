// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/voice_interaction/arc_voice_interaction_framework_service.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/voice_interaction/fake_voice_interaction_controller.h"
#include "chrome/browser/chromeos/arc/voice_interaction/highlighter_controller_client.h"
#include "chrome/browser/chromeos/arc/voice_interaction/voice_interaction_controller_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/test_browser_window_aura.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cras_audio_client.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/arc/test/fake_voice_interaction_framework_instance.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"

namespace arc {

namespace {

class TestHighlighterController : public ash::mojom::HighlighterController,
                                  public service_manager::Service {
 public:
  TestHighlighterController() : binding_(this) {}
  ~TestHighlighterController() override = default;

  void CallHandleSelection(const gfx::Rect& rect) {
    client_->HandleSelection(rect);
  }

  void CallHandleEnabledStateChange(bool enabled) {
    is_enabled_ = enabled;
    client_->HandleEnabledStateChange(enabled);
  }

  bool client_attached() const { return static_cast<bool>(client_); }

  // ash::mojom::HighlighterController:
  void SetClient(ash::mojom::HighlighterControllerClientPtr client) override {
    DCHECK(!client_);
    client_ = std::move(client);
    // Okay to use base::Unretained(this), as |client_| will be destroyed before
    // |this|.
    client_.set_connection_error_handler(
        base::BindOnce(&TestHighlighterController::OnClientConnectionLost,
                       base::Unretained(this)));
  }

  void ExitHighlighterMode() override {
    // simulate exiting current session.
    CallHandleEnabledStateChange(false);
  }

  void FlushMojo() { client_.FlushForTesting(); }

  bool is_enabled() { return is_enabled_; }

  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    DCHECK(interface_name == ash::mojom::HighlighterController::Name_);
    binding_.Bind(
        ash::mojom::HighlighterControllerRequest(std::move(interface_pipe)));
  }

 private:
  void OnClientConnectionLost() {
    client_.reset();
    binding_.Close();
  }

  mojo::Binding<ash::mojom::HighlighterController> binding_;
  ash::mojom::HighlighterControllerClientPtr client_;
  bool is_enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestHighlighterController);
};

std::unique_ptr<TestBrowserWindow> CreateTestBrowserWindow(
    aura::Window* parent) {
  auto window =
      std::make_unique<aura::Window>(nullptr, aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  window->SetBounds(gfx::Rect(0, 0, 200, 200));
  parent->AddChild(window.get());
  return std::make_unique<TestBrowserWindowAura>(std::move(window));
}

ui::Layer* FindLayer(ui::Layer* root, ui::Layer* target) {
  if (root == target)
    return target;
  for (auto* child : root->children()) {
    auto* result = FindLayer(child, target);
    if (result)
      return result;
  }
  return nullptr;
}

}  // namespace

class ArcVoiceInteractionFrameworkServiceTest : public ash::AshTestBase {
 public:
  ArcVoiceInteractionFrameworkServiceTest() = default;

  void SetUp() override {
    AshTestBase::SetUp();
    SetRunningOutsideAsh();
    // Setup test profile.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("user@gmail.com");
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestArcProfile"));
    profile_ = profile_builder.Build();

    // Setup dependencies for voice interaction framework service.
    session_manager_ = std::make_unique<session_manager::SessionManager>();
    arc_session_manager_ = std::make_unique<ArcSessionManager>(
        std::make_unique<ArcSessionRunner>(base::Bind(FakeArcSession::Create)));
    arc_bridge_service_ = std::make_unique<ArcBridgeService>();

    auto highlighter_controller_ptr =
        std::make_unique<TestHighlighterController>();
    highlighter_controller_ = highlighter_controller_ptr.get();
    voice_interaction_controller_ =
        std::make_unique<FakeVoiceInteractionController>();
    voice_interaction_controller_client_ =
        std::make_unique<VoiceInteractionControllerClient>();
    connector_factory_ =
        service_manager::TestConnectorFactory::CreateForUniqueService(
            std::move(highlighter_controller_ptr));
    connector_ = connector_factory_->CreateConnector();
    framework_service_ = std::make_unique<ArcVoiceInteractionFrameworkService>(
        profile_.get(), arc_bridge_service_.get());
    framework_service_->GetHighlighterClientForTesting()
        ->SetConnectorForTesting(connector_.get());
    voice_interaction_controller_client()->SetControllerForTesting(
        voice_interaction_controller_->CreateInterfacePtrAndBind());
    framework_instance_ =
        std::make_unique<FakeVoiceInteractionFrameworkInstance>();
    arc_bridge_service_->voice_interaction_framework()->SetInstance(
        framework_instance_.get());
    WaitForInstanceReady(arc_bridge_service_->voice_interaction_framework());
    // Flushing is required for the AttachClient call to get through to the
    // highligther controller.
    FlushHighlighterControllerMojo();

    framework_service()->SetVoiceInteractionSetupCompleted();
    // Flushing is required for the notify mojo call to get through to the voice
    // interaction controller.
    FlushVoiceInteractionControllerMojo();
  }

  void TearDown() override {
    arc_bridge_service_->voice_interaction_framework()->CloseInstance(
        framework_instance_.get());
    voice_interaction_controller_.reset();
    voice_interaction_controller_client_.reset();
    framework_instance_.reset();
    framework_service_.reset();
    arc_bridge_service_.reset();
    arc_session_manager_.reset();
    session_manager_.reset();
    profile_.reset();
    AshTestBase::TearDown();
  }

 protected:
  ArcBridgeService* arc_bridge_service() const {
    return arc_bridge_service_.get();
  }

  ArcVoiceInteractionFrameworkService* framework_service() const {
    return framework_service_.get();
  }

  FakeVoiceInteractionFrameworkInstance* framework_instance() const {
    return framework_instance_.get();
  }

  TestHighlighterController* highlighter_controller() const {
    return highlighter_controller_;
  }

  FakeVoiceInteractionController* voice_interaction_controller() {
    return voice_interaction_controller_.get();
  }

  VoiceInteractionControllerClient* voice_interaction_controller_client() {
    return voice_interaction_controller_client_.get();
  }

  void FlushHighlighterControllerMojo() {
    framework_service_->GetHighlighterClientForTesting()->FlushMojoForTesting();
  }

  void FlushVoiceInteractionControllerMojo() {
    voice_interaction_controller_client()->FlushMojoForTesting();
  }

  TestingProfile* profile() const { return profile_.get(); }

 private:
  std::unique_ptr<TestingProfile> profile_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<ArcBridgeService> arc_bridge_service_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<service_manager::TestConnectorFactory> connector_factory_;
  std::unique_ptr<service_manager::Connector> connector_;
  // |highlighter_controller_| is valid until |connector_factory_| is deleted.
  TestHighlighterController* highlighter_controller_;
  std::unique_ptr<FakeVoiceInteractionController> voice_interaction_controller_;
  std::unique_ptr<ArcVoiceInteractionFrameworkService> framework_service_;
  std::unique_ptr<FakeVoiceInteractionFrameworkInstance> framework_instance_;
  std::unique_ptr<VoiceInteractionControllerClient>
      voice_interaction_controller_client_;

  DISALLOW_COPY_AND_ASSIGN(ArcVoiceInteractionFrameworkServiceTest);
};

TEST_F(ArcVoiceInteractionFrameworkServiceTest, StartSetupWizard) {
  framework_service()->StartVoiceInteractionSetupWizard();
  // The signal to start setup wizard should be sent.
  EXPECT_EQ(1u, framework_instance()->setup_wizard_count());
}

TEST_F(ArcVoiceInteractionFrameworkServiceTest, ShowSettings) {
  framework_service()->ShowVoiceInteractionSettings();
  // The signal to show voice interaction settings should be sent.
  EXPECT_EQ(1u, framework_instance()->show_settings_count());
}

TEST_F(ArcVoiceInteractionFrameworkServiceTest, StartSession) {
  framework_service()->StartSessionFromUserInteraction(gfx::Rect());
  FlushVoiceInteractionControllerMojo();
  // The signal to start voice interaction session should be sent.
  EXPECT_EQ(1u, framework_instance()->start_session_count());
}

TEST_F(ArcVoiceInteractionFrameworkServiceTest, StartSessionWithoutFlag) {
  // Remove the voice interaction enabled flag.
  framework_service()->SetVoiceInteractionEnabled(false,
                                                  base::BindOnce([](bool) {}));

  framework_service()->StartSessionFromUserInteraction(gfx::Rect());
  // The signal should not be sent when voice interaction disabled.
  EXPECT_EQ(0u, framework_instance()->start_session_count());
}

TEST_F(ArcVoiceInteractionFrameworkServiceTest, StartSessionWithoutInstance) {
  // Reset the framework instance.
  arc_bridge_service()->voice_interaction_framework()->CloseInstance(
      framework_instance());

  framework_service()->StartSessionFromUserInteraction(gfx::Rect());
  // A notification should be sent if the container is not ready yet.
  FlushVoiceInteractionControllerMojo();
  EXPECT_EQ(ash::mojom::VoiceInteractionState::NOT_READY,
            voice_interaction_controller()->voice_interaction_state());
  // The signal should not be sent when framework instance not ready.
  EXPECT_EQ(0u, framework_instance()->start_session_count());
}

TEST_F(ArcVoiceInteractionFrameworkServiceTest, ToggleSession) {
  framework_service()->ToggleSessionFromUserInteraction();
  FlushVoiceInteractionControllerMojo();
  // The signal to toggle voice interaction session should be sent.
  EXPECT_EQ(1u, framework_instance()->toggle_session_count());
}

TEST_F(ArcVoiceInteractionFrameworkServiceTest, HotwordTriggered) {
  auto* audio_client = static_cast<chromeos::FakeCrasAudioClient*>(
      chromeos::DBusThreadManager::Get()->GetCrasAudioClient());
  audio_client->NotifyHotwordTriggeredForTesting(0, 0);
  EXPECT_TRUE(framework_service()->ValidateTimeSinceUserInteraction());
}

TEST_F(ArcVoiceInteractionFrameworkServiceTest, HighlighterControllerClient) {
  EXPECT_TRUE(highlighter_controller()->client_attached());

  // Enabled state should propagate to the framework instance.
  highlighter_controller()->CallHandleEnabledStateChange(true);
  highlighter_controller()->FlushMojo();
  EXPECT_EQ(1u, framework_instance()->set_metalayer_visibility_count());
  EXPECT_TRUE(framework_instance()->metalayer_visible());

  // Disabled state should propagate to the framework instance.
  framework_instance()->ResetCounters();
  highlighter_controller()->CallHandleEnabledStateChange(false);
  highlighter_controller()->FlushMojo();
  EXPECT_EQ(1u, framework_instance()->set_metalayer_visibility_count());
  EXPECT_FALSE(framework_instance()->metalayer_visible());

  // Enable the state again.
  framework_instance()->ResetCounters();
  highlighter_controller()->CallHandleEnabledStateChange(true);
  highlighter_controller()->FlushMojo();
  EXPECT_EQ(1u, framework_instance()->set_metalayer_visibility_count());
  EXPECT_TRUE(framework_instance()->metalayer_visible());

  // Simulate a valid selection.
  framework_instance()->ResetCounters();
  const gfx::Rect selection(100, 200, 300, 400);
  highlighter_controller()->CallHandleSelection(selection);
  highlighter_controller()->CallHandleEnabledStateChange(false);
  highlighter_controller()->FlushMojo();
  // Neither the selected region nor the state update should reach the
  // framework instance yet.
  EXPECT_EQ(0u, framework_instance()->start_session_for_region_count());
  EXPECT_EQ(0u, framework_instance()->set_metalayer_visibility_count());
  EXPECT_TRUE(framework_instance()->metalayer_visible());
  framework_service()
      ->GetHighlighterClientForTesting()
      ->SimulateSelectionTimeoutForTesting();
  // After a timeout, the selected region should reach the framework instance.
  EXPECT_EQ(1u, framework_instance()->start_session_for_region_count());
  EXPECT_EQ(selection.ToString(),
            framework_instance()->selected_region().ToString());
  // However, the state update should not be explicitly sent to the framework
  // instance, since the state change is implied with a valid selection.
  EXPECT_EQ(0u, framework_instance()->set_metalayer_visibility_count());

  // Clear the framework instance to simulate the container crash.
  // The client should become detached.
  arc_bridge_service()->voice_interaction_framework()->CloseInstance(
      framework_instance());
  FlushHighlighterControllerMojo();
  EXPECT_FALSE(highlighter_controller()->client_attached());

  // Set the framework instance again to simulate the container restart.
  // The client should become attached again.
  arc_bridge_service()->voice_interaction_framework()->SetInstance(
      framework_instance());
  WaitForInstanceReady(arc_bridge_service()->voice_interaction_framework());
  FlushHighlighterControllerMojo();
  EXPECT_TRUE(highlighter_controller()->client_attached());

  // State update should reach the client normally.
  framework_instance()->ResetCounters();
  highlighter_controller()->CallHandleEnabledStateChange(true);
  highlighter_controller()->FlushMojo();
  EXPECT_EQ(1u, framework_instance()->set_metalayer_visibility_count());
  EXPECT_TRUE(framework_instance()->metalayer_visible());
}

TEST_F(ArcVoiceInteractionFrameworkServiceTest,
       ExitVoiceInteractionAlsoExitHighlighter) {
  highlighter_controller()->CallHandleEnabledStateChange(true);

  framework_service()->ToggleSessionFromUserInteraction();
  framework_instance()->FlushMojoForTesting();
  FlushHighlighterControllerMojo();
  EXPECT_EQ(ash::mojom::VoiceInteractionState::RUNNING,
            framework_service()->GetStateForTesting());

  framework_service()->ToggleSessionFromUserInteraction();
  framework_instance()->FlushMojoForTesting();
  FlushHighlighterControllerMojo();
  EXPECT_EQ(ash::mojom::VoiceInteractionState::STOPPED,
            framework_service()->GetStateForTesting());

  EXPECT_FALSE(highlighter_controller()->is_enabled());
}

TEST_F(ArcVoiceInteractionFrameworkServiceTest,
       VoiceInteractionControllerClient) {
  FakeVoiceInteractionController* controller = voice_interaction_controller();
  VoiceInteractionControllerClient* controller_client =
      voice_interaction_controller_client();
  // The voice interaction flags should be set after the initial setup.
  EXPECT_EQ(controller->voice_interaction_state(),
            ash::mojom::VoiceInteractionState::STOPPED);

  // Send the signal to set the voice interaction state.
  controller_client->NotifyStatusChanged(
      ash::mojom::VoiceInteractionState::RUNNING);
  FlushVoiceInteractionControllerMojo();
  EXPECT_EQ(controller->voice_interaction_state(),
            ash::mojom::VoiceInteractionState::RUNNING);
}

TEST_F(ArcVoiceInteractionFrameworkServiceTest,
       CapturingScreenshotBlocksIncognitoWindows) {
  auto browser_window =
      CreateTestBrowserWindow(ash::Shell::GetPrimaryRootWindow());
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_TABBED;
  params.window = browser_window.get();
  Browser browser(params);
  browser_window->GetNativeWindow()->Show();

  profile()->ForceIncognito(true);
  // Layer::RecreateLayer() will replace the window's layer with the newly
  // created layer. Thus, we need to save the |old_layer| for comparison.
  auto* old_layer = browser_window->GetNativeWindow()->layer();
  auto layer_owner = framework_service()->CreateLayerTreeForSnapshotForTesting(
      ash::Shell::GetPrimaryRootWindow());
  EXPECT_FALSE(FindLayer(layer_owner->root(), old_layer));

  profile()->ForceIncognito(false);
  old_layer = browser_window->GetNativeWindow()->layer();
  layer_owner = framework_service()->CreateLayerTreeForSnapshotForTesting(
      ash::Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(FindLayer(layer_owner->root(), old_layer));

  ash::Shell::GetPrimaryRootWindow()->RemoveChild(
      browser_window->GetNativeWindow());
}

}  // namespace arc

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_helper.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/display/display_configuration_controller_test_api.h"
#include "ash/display/screen_ash.h"
#include "ash/mojo_test_interface_factory.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/test/ash_test_environment.h"
#include "ash/test/ash_test_views_delegate.h"
#include "ash/test_shell_delegate.h"
#include "ash/ws/window_service_owner.h"
#include "base/guid.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/discardable_memory/public/interfaces/discardable_shared_memory_manager.mojom.h"
#include "components/prefs/testing_pref_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/ws/public/cpp/host/gpu_interface_provider.h"
#include "services/ws/public/mojom/gpu.mojom.h"
#include "services/ws/window_service.h"
#include "ui/aura/env.h"
#include "ui/aura/input_state_lookup.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/platform_window_defaults.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/wm_state.h"

namespace ash {

// An implementation of GpuInterfaceProvider that queues up requests for
// interfaces. The requests are never actually bound, but are kept alive to
// ensure the requestor doesn't detect a close and try to exit.
class TestGpuInterfaceProvider : public ws::GpuInterfaceProvider {
 public:
  TestGpuInterfaceProvider() = default;
  ~TestGpuInterfaceProvider() override = default;

  // ws::GpuInterfaceProvider:
  void RegisterGpuInterfaces(
      service_manager::BinderRegistry* registry) override {
    registry->AddInterface(base::BindRepeating(
        &TestGpuInterfaceProvider::BindDiscardableSharedMemoryManager,
        base::Unretained(this)));
    registry->AddInterface(base::BindRepeating(
        &TestGpuInterfaceProvider::BindGpuRequest, base::Unretained(this)));
  }
  void RegisterOzoneGpuInterfaces(
      service_manager::BinderRegistry* registry) override {}

 private:
  void BindDiscardableSharedMemoryManager(
      discardable_memory::mojom::DiscardableSharedMemoryManagerRequest
          request) {
    request_handles_.push_back(request.PassMessagePipe());
  }
  void BindGpuRequest(ws::mojom::GpuRequest request) {
    request_handles_.push_back(request.PassMessagePipe());
  }

  std::vector<mojo::ScopedMessagePipeHandle> request_handles_;

  DISALLOW_COPY_AND_ASSIGN(TestGpuInterfaceProvider);
};

// TODO(sky): refactor and move to services.
class TestConnector : public service_manager::mojom::Connector {
 public:
  TestConnector() : test_user_id_(base::GenerateGUID()) {}

  ~TestConnector() override = default;

  service_manager::mojom::ServiceRequest GenerateServiceRequest() {
    return mojo::MakeRequest(&service_ptr_);
  }

  void Start() {
    service_ptr_->OnStart(
        service_manager::Identity("TestConnectorFactory", test_user_id_),
        base::BindOnce(&TestConnector::OnStartCallback,
                       base::Unretained(this)));
  }

  std::unique_ptr<service_manager::Connector> CreateConnector() {
    service_manager::mojom::ConnectorPtr proxy;
    Clone(mojo::MakeRequest(&proxy));
    return std::make_unique<service_manager::Connector>(std::move(proxy));
  }

 private:
  void OnStartCallback(
      service_manager::mojom::ConnectorRequest request,
      service_manager::mojom::ServiceControlAssociatedRequest control_request) {
  }

  // mojom::Connector implementation:
  void BindInterface(const service_manager::Identity& target,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe,
                     BindInterfaceCallback callback) override {
    service_manager::mojom::ServicePtr* service_ptr = &service_ptr_;
    // If you hit the DCHECK below, you need to add a call to AddService() in
    // your test for the reported service.
    DCHECK(service_ptr) << "Binding interface for unregistered service "
                        << target.name();
    (*service_ptr)
        ->OnBindInterface(service_manager::BindSourceInfo(
                              service_manager::Identity("TestConnectorFactory",
                                                        test_user_id_),
                              service_manager::CapabilitySet()),
                          interface_name, std::move(interface_pipe),
                          base::DoNothing());
    std::move(callback).Run(service_manager::mojom::ConnectResult::SUCCEEDED,
                            service_manager::Identity());
  }

  void StartService(const service_manager::Identity& target,
                    StartServiceCallback callback) override {
    NOTREACHED();
  }

  void QueryService(const service_manager::Identity& target,
                    QueryServiceCallback callback) override {
    NOTREACHED();
  }

  void StartServiceWithProcess(
      const service_manager::Identity& identity,
      mojo::ScopedMessagePipeHandle service,
      service_manager::mojom::PIDReceiverRequest pid_receiver_request,
      StartServiceWithProcessCallback callback) override {
    NOTREACHED();
  }

  void Clone(service_manager::mojom::ConnectorRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  void FilterInterfaces(
      const std::string& spec,
      const service_manager::Identity& source,
      service_manager::mojom::InterfaceProviderRequest source_request,
      service_manager::mojom::InterfaceProviderPtr target) override {
    NOTREACHED();
  }

  const std::string test_user_id_;
  mojo::BindingSet<service_manager::mojom::Connector> bindings_;
  service_manager::mojom::ServicePtr service_ptr_;

  DISALLOW_COPY_AND_ASSIGN(TestConnector);
};

AshTestHelper::AshTestHelper(AshTestEnvironment* ash_test_environment)
    : ash_test_environment_(ash_test_environment),
      command_line_(std::make_unique<base::test::ScopedCommandLine>()) {
  ui::test::EnableTestConfigForPlatformWindows();
}

AshTestHelper::~AshTestHelper() {
  // Ensure the next test starts with a null display::Screen. Done here because
  // some tests use Screen after TearDown().
  ScreenAsh::DeleteScreenForShutdown();
}

void AshTestHelper::SetUp(bool start_session, bool provide_local_state) {
#if !defined(NDEBUG)
  aura::Window::SetEnvArgRequired(
      "Within ash you must supply a non-null aura::Env when creating a Window, "
      "use window_factory, or supply the Env obtained from "
      "Shell::Get()->aura_env()");
#endif

  // TODO(jamescook): Can we do this without changing command line?
  // Use the origin (1,1) so that it doesn't over
  // lap with the native mouse cursor.
  if (!command_line_->GetProcessCommandLine()->HasSwitch(
          ::switches::kHostWindowBounds)) {
    command_line_->GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kHostWindowBounds, "1+1-800x600");
  }

  // TODO(wutao): We enabled a smooth screen rotation animation, which is using
  // an asynchronous method. However for some tests require to evaluate the
  // screen rotation immediately after the operation of setting display
  // rotation, we need to append a slow screen rotation animation flag to pass
  // the tests. When we remove the flag "ash-disable-smooth-screen-rotation", we
  // need to disable the screen rotation animation in the test.
  if (!command_line_->GetProcessCommandLine()->HasSwitch(
          switches::kAshDisableSmoothScreenRotation)) {
    command_line_->GetProcessCommandLine()->AppendSwitch(
        switches::kAshDisableSmoothScreenRotation);
  }

  statistics_provider_ =
      std::make_unique<chromeos::system::ScopedFakeStatisticsProvider>();

  ui::test::EventGeneratorDelegate::SetFactoryFunction(base::BindRepeating(
      &aura::test::EventGeneratorDelegateAura::Create, nullptr));

  display::ResetDisplayIdForTest();
  wm_state_ = std::make_unique<::wm::WMState>();
  test_views_delegate_ = ash_test_environment_->CreateViewsDelegate();

  // Disable animations during tests.
  zero_duration_mode_.reset(new ui::ScopedAnimationDurationScaleMode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION));
  ui::InitializeInputMethodForTesting();

  // Creates Shell and hook with Desktop.
  if (!test_shell_delegate_)
    test_shell_delegate_ = new TestShellDelegate;

  if (!chromeos::DBusThreadManager::IsInitialized()) {
    chromeos::DBusThreadManager::Initialize(
        chromeos::DBusThreadManager::kShared);
    dbus_thread_manager_initialized_ = true;
  }

  if (!bluez::BluezDBusManager::IsInitialized()) {
    bluez::BluezDBusManager::Initialize();
    bluez_dbus_manager_initialized_ = true;
  }

  if (!chromeos::PowerPolicyController::IsInitialized()) {
    chromeos::PowerPolicyController::Initialize(
        chromeos::DBusThreadManager::Get()->GetPowerManagerClient());
    power_policy_controller_initialized_ = true;
  }

  // Create CrasAudioHandler for testing since g_browser_process is not
  // created in AshTestBase tests.
  chromeos::CrasAudioHandler::InitializeForTesting();
  chromeos::SystemSaltGetter::Initialize();

  ash_test_environment_->SetUp();
  // Reset the global state for the cursor manager. This includes the
  // last cursor visibility state, etc.
  ::wm::CursorManager::ResetCursorVisibilityStateForTest();

  ui::MaterialDesignController::Initialize();

  CreateShell();

  aura::test::SetEnvForTestWindows(Shell::Get()->aura_env());

  aura::test::EnvTestHelper(Shell::Get()->aura_env())
      .SetInputStateLookup(std::unique_ptr<aura::InputStateLookup>());

  Shell* shell = Shell::Get();

  // Cursor is visible by default in tests.
  // CursorManager is null on MASH.
  if (shell->cursor_manager())
    shell->cursor_manager()->ShowCursor();

  if (provide_local_state) {
    auto pref_service = std::make_unique<TestingPrefServiceSimple>();
    Shell::RegisterLocalStatePrefs(pref_service->registry(), true);
    Shell::Get()->OnLocalStatePrefServiceInitialized(std::move(pref_service));
  }

  session_controller_client_.reset(
      new TestSessionControllerClient(shell->session_controller()));
  session_controller_client_->InitializeAndBind();

  if (start_session)
    session_controller_client_->CreatePredefinedUserSessions(1);

  // Tests that change the display configuration generally don't care about
  // the notifications and the popup UI can interfere with things like
  // cursors.
  shell->screen_layout_observer()->set_show_notifications_for_testing(false);

  display::test::DisplayManagerTestApi(shell->display_manager())
      .DisableChangeDisplayUponHostResize();
  DisplayConfigurationControllerTestApi(
      shell->display_configuration_controller())
      .SetDisplayAnimator(false);

  app_list_test_helper_ = std::make_unique<AppListTestHelper>();

  CreateWindowService();
}

void AshTestHelper::TearDown() {
  app_list_test_helper_.reset();

  aura::test::SetEnvForTestWindows(nullptr);

  Shell::DeleteInstance();

  // Suspend the tear down until all resources are returned via
  // CompositorFrameSinkClient::ReclaimResources()
  base::RunLoop().RunUntilIdle();
  ash_test_environment_->TearDown();

  chromeos::SystemSaltGetter::Shutdown();
  chromeos::CrasAudioHandler::Shutdown();

  if (power_policy_controller_initialized_) {
    chromeos::PowerPolicyController::Shutdown();
    power_policy_controller_initialized_ = false;
  }

  if (bluez_dbus_manager_initialized_) {
    device::BluetoothAdapterFactory::Shutdown();
    bluez::BluezDBusManager::Shutdown();
    bluez_dbus_manager_initialized_ = false;
  }

  if (dbus_thread_manager_initialized_) {
    chromeos::DBusThreadManager::Shutdown();
    dbus_thread_manager_initialized_ = false;
  }

  ui::TerminateContextFactoryForTests();

  // ui::TerminateContextFactoryForTests() destroyed the context factory (and
  // context factory private) referenced by Env. Reset Env's members in case
  // some other test tries to use it. This matters if someone else created Env
  // (such as the test suite) and is long lived.
  if (aura::Env::HasInstance()) {
    aura::Env::GetInstance()->set_context_factory(nullptr);
    aura::Env::GetInstance()->set_context_factory_private(nullptr);
  }

  ui::ShutdownInputMethodForTesting();
  zero_duration_mode_.reset();

  test_views_delegate_.reset();
  wm_state_.reset();

  command_line_.reset();

  display::Display::ResetForceDeviceScaleFactorForTesting();
  env_window_tree_client_setter_.reset();

  CHECK(!::wm::CaptureController::Get());
#if !defined(NDEBUG)
  aura::Window::SetEnvArgRequired(nullptr);
#endif

  ui::test::EventGeneratorDelegate::SetFactoryFunction(
      ui::test::EventGeneratorDelegate::FactoryFunction());

  statistics_provider_.reset();
}

void AshTestHelper::SetRunningOutsideAsh() {
  test_views_delegate_->set_running_outside_ash();
#if DCHECK_IS_ON()
  aura::Window::SetEnvArgRequired(nullptr);
#endif
}

PrefService* AshTestHelper::GetLocalStatePrefService() {
  return Shell::Get()->local_state_.get();
}

aura::Window* AshTestHelper::CurrentContext() {
  aura::Window* root_window = Shell::GetRootWindowForNewWindows();
  if (!root_window)
    root_window = Shell::GetPrimaryRootWindow();
  DCHECK(root_window);
  return root_window;
}

display::Display AshTestHelper::GetSecondaryDisplay() {
  return Shell::Get()->display_manager()->GetSecondaryDisplay();
}

service_manager::Connector* AshTestHelper::GetWindowServiceConnector() {
  return window_service_connector_.get();
}

void AshTestHelper::CreateWindowService() {
  test_connector_ = std::make_unique<TestConnector>();
  Shell::Get()->window_service_owner()->BindWindowService(
      test_connector_->GenerateServiceRequest());
  test_connector_->Start();
  // WindowService::OnStart() is not immediately called (it happens async over
  // mojo). If this becomes a problem we could run the MessageLoop here.
  // Surprisingly running the MessageLooop results in some test failures. These
  // failures seem to be because spinning the messageloop causes some timers to
  // fire (perhaps animations too) the results in a slightly different Shell
  // state.
  window_service_connector_ = test_connector_->CreateConnector();
}

void AshTestHelper::CreateShell() {
  ui::ContextFactory* context_factory = nullptr;
  ui::ContextFactoryPrivate* context_factory_private = nullptr;
  bool enable_pixel_output = false;
  ui::InitializeContextFactoryForTests(enable_pixel_output, &context_factory,
                                       &context_factory_private);
  ShellInitParams init_params;
  init_params.delegate.reset(test_shell_delegate_);
  init_params.context_factory = context_factory;
  init_params.context_factory_private = context_factory_private;
  init_params.gpu_interface_provider =
      std::make_unique<TestGpuInterfaceProvider>();
  Shell::CreateInstance(std::move(init_params));
}

}  // namespace ash

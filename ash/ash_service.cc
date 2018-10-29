// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_service.h"

#include "ash/mojo_interface_factory.h"
#include "ash/network_connect_delegate_mus.h"
#include "ash/shell.h"
#include "ash/shell_delegate_mash.h"
#include "ash/shell_init_params.h"
#include "ash/ws/ash_gpu_interface_provider.h"
#include "ash/ws/window_service_owner.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/switches.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/embedded_service_info.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/ws/gpu_host/gpu_host.h"
#include "services/ws/host_context_factory.h"
#include "services/ws/public/cpp/gpu/gpu.h"
#include "services/ws/public/cpp/input_devices/input_device_controller.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/window_service.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/views_delegate.h"
#include "ui/wm/core/wm_state.h"

namespace ash {
namespace {

std::unique_ptr<service_manager::Service> CreateAshService() {
  return std::make_unique<AshService>();
}

class AshViewsDelegate : public views::ViewsDelegate {
 public:
  AshViewsDelegate() = default;
  ~AshViewsDelegate() override = default;

 private:
  // views::ViewsDelegate:
  void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) override {}

  // TODO: this may need to create ChromeLayoutProvider.
  // https://crbug.com/853989 .
  views::LayoutProvider layout_provider_;

  DISALLOW_COPY_AND_ASSIGN(AshViewsDelegate);
};

}  // namespace

AshService::AshService() = default;

AshService::~AshService() {
  if (!base::FeatureList::IsEnabled(features::kMash))
    return;

  // Shutdown part of GpuHost before deleting Shell. This is necessary to
  // avoid a deadlock between the raster thread and main thread. GpuHost can't
  // be deleted here as that causes other DCHECKs.
  gpu_host_->Shutdown();

  Shell::DeleteInstance();

  statistics_provider_.reset();
  // NOTE: PowerStatus is shutdown by Shell.
  chromeos::SystemSaltGetter::Shutdown();
  chromeos::CrasAudioHandler::Shutdown();
  chromeos::NetworkConnect::Shutdown();
  network_connect_delegate_.reset();
  // We may not have started the NetworkHandler.
  if (network_handler_initialized_)
    chromeos::NetworkHandler::Shutdown();
  device::BluetoothAdapterFactory::Shutdown();
  bluez::BluezDBusManager::Shutdown();
  chromeos::PowerPolicyController::Shutdown();
  if (dbus_thread_manager_initialized_)
    chromeos::DBusThreadManager::Shutdown();

  // |gpu_host_| must be completely destroyed before Env as GpuHost depends on
  // Ozone, which Env owns.
  gpu_host_.reset();
}

// static
service_manager::EmbeddedServiceInfo AshService::CreateEmbeddedServiceInfo() {
  service_manager::EmbeddedServiceInfo info;
  info.factory = base::BindRepeating(&CreateAshService);
  info.task_runner = base::ThreadTaskRunnerHandle::Get();
  return info;
}

void AshService::InitForMash() {
  wm_state_ = std::make_unique<::wm::WMState>();

  discardable_shared_memory_manager_ =
      std::make_unique<discardable_memory::DiscardableSharedMemoryManager>();

  gpu_host_ = std::make_unique<ws::gpu_host::GpuHost>(
      this, context()->connector(), discardable_shared_memory_manager_.get());

  host_frame_sink_manager_ = std::make_unique<viz::HostFrameSinkManager>();
  CreateFrameSinkManager();
  io_thread_ = std::make_unique<base::Thread>("IOThread");
  base::Thread::Options thread_options(base::MessageLoop::TYPE_IO, 0);
  thread_options.priority = base::ThreadPriority::NORMAL;
  CHECK(io_thread_->StartWithOptions(thread_options));
  gpu_ = ws::Gpu::Create(context()->connector(), ws::mojom::kServiceName,
                         io_thread_->task_runner());

  context_factory_ = std::make_unique<ws::HostContextFactory>(
      gpu_.get(), host_frame_sink_manager_.get());

  env_ = aura::Env::CreateInstanceToHostViz(context()->connector());

  views_delegate_ = std::make_unique<AshViewsDelegate>();

  // Must occur after mojo::ApplicationRunner has initialized AtExitManager, but
  // before WindowManager::Init(). Tests might initialize their own instance.
  if (!chromeos::DBusThreadManager::IsInitialized()) {
    chromeos::DBusThreadManager::Initialize(
        chromeos::DBusThreadManager::kShared);
    dbus_thread_manager_initialized_ = true;
  }
  chromeos::PowerPolicyController::Initialize(
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient());

  // The initialization matches that in ChromeBrowserMainPartsChromeos.

  bluez::BluezDBusManager::Initialize();
  if (!chromeos::NetworkHandler::IsInitialized()) {
    chromeos::NetworkHandler::Initialize();
    network_handler_initialized_ = true;
  }
  network_connect_delegate_ = std::make_unique<NetworkConnectDelegateMus>();
  chromeos::NetworkConnect::Initialize(network_connect_delegate_.get());
  // TODO(jamescook): Initialize real audio handler.
  chromeos::CrasAudioHandler::InitializeForTesting();
  chromeos::SystemSaltGetter::Initialize();

  // TODO(jamescook): Refactor StatisticsProvider so we can get just the data
  // we need in ash. Right now StatisticsProviderImpl launches the crossystem
  // binary to get system data, which we don't want to do twice on startup.
  statistics_provider_.reset(
      new chromeos::system::ScopedFakeStatisticsProvider());
  statistics_provider_->SetMachineStatistic("initial_locale", "en-US");
  statistics_provider_->SetMachineStatistic("keyboard_layout", "");

  ShellInitParams shell_init_params;
  shell_init_params.delegate = std::make_unique<ShellDelegateMash>();
  shell_init_params.context_factory = context_factory_.get();
  shell_init_params.context_factory_private =
      context_factory_->GetContextFactoryPrivate();
  shell_init_params.connector = context()->connector();
  shell_init_params.gpu_interface_provider =
      std::make_unique<AshGpuInterfaceProvider>(
          gpu_host_.get(), discardable_shared_memory_manager_.get());
  Shell::CreateInstance(std::move(shell_init_params));
  Shell::GetPrimaryRootWindow()->GetHost()->Show();
}

void AshService::OnStart() {
  mojo_interface_factory::RegisterInterfaces(
      &registry_, base::ThreadTaskRunnerHandle::Get());
  registry_.AddInterface(base::BindRepeating(&AshService::BindServiceFactory,
                                             base::Unretained(this)));

  if (base::FeatureList::IsEnabled(features::kMash))
    InitForMash();
}

void AshService::OnBindInterface(
    const service_manager::BindSourceInfo& remote_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle handle) {
  registry_.BindInterface(interface_name, std::move(handle));
}

void AshService::CreateService(
    service_manager::mojom::ServiceRequest service,
    const std::string& name,
    service_manager::mojom::PIDReceiverPtr pid_receiver) {
  DCHECK_EQ(name, ws::mojom::kServiceName);
  Shell::Get()->window_service_owner()->BindWindowService(std::move(service));
  if (base::FeatureList::IsEnabled(features::kMash)) {
    ws::WindowService* window_service =
        Shell::Get()->window_service_owner()->window_service();
    input_device_controller_ = std::make_unique<ws::InputDeviceController>();
    input_device_controller_->AddInterface(window_service->registry());
  }
  pid_receiver->SetPID(base::GetCurrentProcId());
}

void AshService::BindServiceFactory(
    service_manager::mojom::ServiceFactoryRequest request) {
  service_factory_bindings_.AddBinding(this, std::move(request));
}

void AshService::CreateFrameSinkManager() {
  viz::mojom::FrameSinkManagerPtr frame_sink_manager;
  viz::mojom::FrameSinkManagerRequest frame_sink_manager_request =
      mojo::MakeRequest(&frame_sink_manager);
  viz::mojom::FrameSinkManagerClientPtr frame_sink_manager_client;
  viz::mojom::FrameSinkManagerClientRequest frame_sink_manager_client_request =
      mojo::MakeRequest(&frame_sink_manager_client);

  gpu_host_->CreateFrameSinkManager(std::move(frame_sink_manager_request),
                                    frame_sink_manager_client.PassInterface());

  host_frame_sink_manager_->BindAndSetManager(
      std::move(frame_sink_manager_client_request), nullptr /* task_runner */,
      std::move(frame_sink_manager));
}

void AshService::OnGpuServiceInitialized() {}

}  // namespace ash

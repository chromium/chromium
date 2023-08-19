// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/memory_pressure/arc_memory_pressure_bridge.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/mojom/process.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/logging.h"
#include "base/memory/singleton.h"

namespace arc {
namespace {

// Singleton factory for ArcMemoryPressureBridge.
class ArcMemoryPressureBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcMemoryPressureBridge,
          ArcMemoryPressureBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcMemoryPressureBridgeFactory";

  static ArcMemoryPressureBridgeFactory* GetInstance() {
    return base::Singleton<ArcMemoryPressureBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcMemoryPressureBridgeFactory>;

  ArcMemoryPressureBridgeFactory() {
    DependsOn(ArcMetricsService::GetFactory());
  }

  ~ArcMemoryPressureBridgeFactory() override = default;
};

}  // namespace

// static
ArcMemoryPressureBridge* ArcMemoryPressureBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcMemoryPressureBridgeFactory::GetForBrowserContext(context);
}

ArcMemoryPressureBridge*
ArcMemoryPressureBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcMemoryPressureBridgeFactory::GetForBrowserContextForTesting(
      context);
}

ArcMemoryPressureBridge::ArcMemoryPressureBridge(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      arc_metrics_service_(ArcMetricsService::GetForBrowserContext(context)) {
  DCHECK(arc_metrics_service_ != nullptr);
  arc_bridge_service_->process()->AddObserver(this);
}

ArcMemoryPressureBridge::~ArcMemoryPressureBridge() {
  // NB: It is safe to have too many calls to RemoveObserver, so we add one
  // here just in case we are destructed while the ProcessInstance connection
  // is up.
  ash::ResourcedClient* client = ash::ResourcedClient::Get();
  if (client)
    client->RemoveArcVmObserver(this);
  arc_bridge_service_->process()->RemoveObserver(this);
}

void ArcMemoryPressureBridge::OnMemoryPressure(
    ash::ResourcedClient::PressureLevelArcVm level,
    uint64_t reclaim_target_kb) {
  if (memory_pressure_in_flight_)
    return;
  mojom::PressureLevel arc_level;
  mojom::ProcessState arc_level_deprecated;
  switch (level) {
    case ash::ResourcedClient::PressureLevelArcVm::NONE:
      return;

    case ash::ResourcedClient::PressureLevelArcVm::CACHED:
      arc_level = mojom::PressureLevel::kCached;
      arc_level_deprecated = mojom::ProcessState::R_CACHED_ACTIVITY_CLIENT;
      break;

    case ash::ResourcedClient::PressureLevelArcVm::PERCEPTIBLE:
      arc_level = mojom::PressureLevel::kPerceptible;
      arc_level_deprecated = mojom::ProcessState::R_TOP;
      break;

    case ash::ResourcedClient::PressureLevelArcVm::FOREGROUND:
      arc_level = mojom::PressureLevel::kForeground;
      arc_level_deprecated = mojom::ProcessState::R_TOP;
      break;

    default:
      LOG(ERROR) << "ArcMemoryPressureBridge::OnMemoryPressure unknown level "
                 << static_cast<int>(level);
      return;
  }
  auto* process_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->process(), ApplyHostMemoryPressure);
  if (process_instance) {
    process_instance->ApplyHostMemoryPressure(
        arc_level, reclaim_target_kb * 1024,
        base::BindOnce(&ArcMemoryPressureBridge::OnHostMemoryPressureComplete,
                       weak_ptr_factory_.GetWeakPtr()));
    memory_pressure_in_flight_ = true;
    return;
  }
  auto* process_instance_deprecated = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->process(), ApplyHostMemoryPressureDeprecated);
  if (process_instance_deprecated) {
    process_instance_deprecated->ApplyHostMemoryPressureDeprecated(
        arc_level_deprecated, reclaim_target_kb * 1024,
        base::BindOnce(&ArcMemoryPressureBridge::OnHostMemoryPressureComplete,
                       weak_ptr_factory_.GetWeakPtr()));
    memory_pressure_in_flight_ = true;
    return;
  }
  LOG(ERROR) << "ArcMemoryPressureBridge::OnMemoryPressure event, but no "
                "process_instance or process_instance_deprecated";
}

void ArcMemoryPressureBridge::OnConnectionReady() {
  // Only listen to memory pressure signals when ARCVM is running.
  ash::ResourcedClient* client = ash::ResourcedClient::Get();
  DCHECK(client);
  client->AddArcVmObserver(this);
}

void ArcMemoryPressureBridge::OnConnectionClosed() {
  ash::ResourcedClient* client = ash::ResourcedClient::Get();
  DCHECK(client);
  client->RemoveArcVmObserver(this);

  // The connection to ArcProcessService has been closed, there can not be any
  // memory pressure signals in flight.
  memory_pressure_in_flight_ = false;
}

void ArcMemoryPressureBridge::OnHostMemoryPressureComplete(uint32_t killed,
                                                           uint64_t reclaimed) {
  DCHECK(memory_pressure_in_flight_);
  memory_pressure_in_flight_ = false;
  arc_metrics_service_->ReportMemoryPressureArcVmKills(killed,
                                                       reclaimed / 1024);
}

// static
void ArcMemoryPressureBridge::EnsureFactoryBuilt() {
  ArcMemoryPressureBridgeFactory::GetInstance();
}

}  // namespace arc

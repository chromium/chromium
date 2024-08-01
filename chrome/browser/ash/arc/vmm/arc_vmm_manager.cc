// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/vmm/arc_vmm_manager.h"

#include <optional>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_session.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/shell.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/vmm/arc_vmm_swap_scheduler.h"
#include "chrome/browser/ash/arc/vmm/arcvm_working_set_trim_executor.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "ui/base/accelerators/accelerator.h"

namespace arc {

namespace {
class ArcVmmManagerFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcVmmManager,
          ArcVmmManagerFactory> {
 public:
  static constexpr const char* kName = "ArcVmmManagerFactory";
  static ArcVmmManagerFactory* GetInstance() {
    static base::NoDestructor<ArcVmmManagerFactory> instance;
    return instance.get();
  }

 private:
  friend class base::NoDestructor<ArcVmmManagerFactory>;

  ArcVmmManagerFactory() = default;
  ~ArcVmmManagerFactory() override = default;
};

}  // namespace

// static
ArcVmmManager* ArcVmmManager::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcVmmManagerFactory::GetForBrowserContext(context);
}

// static
void ArcVmmManager::EnsureFactoryBuilt() {
  ArcVmmManagerFactory::GetInstance();
}

// static
ArcVmmManager* ArcVmmManager::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcVmmManagerFactory::GetForBrowserContextForTesting(context);
}

ArcVmmManager::ArcVmmManager(content::BrowserContext* context,
                             ArcBridgeService* bridge)
    : context_(context), bridge_service_(bridge) {
  app_instance_observation_.Observe(bridge_service_->app());

  auto* client = ash::ConciergeClient::Get();
  DCHECK(client);
  if (client) {
    concierge_observation_.Observe(client);
  } else {
    LOG(FATAL) << "ArcVmmManager initialized but failed to register observer "
                  "on Concierge.";
  }

  if (base::FeatureList::IsEnabled(kVmmSwapKeyboardShortcut)) {
    accelerator_ = std::make_unique<AcceleratorTarget>(this);
  }
  if (base::FeatureList::IsEnabled(kVmmSwapPolicy)) {
    swap_out_delay_ = base::Seconds(kVmmSwapOutDelaySecond.Get());
    scheduler_ = std::make_unique<ArcVmmSwapScheduler>(
        base::BindRepeating(
            [](base::WeakPtr<ArcVmmManager> manager, bool enable) {
              if (manager) {
                manager->SetSwapState(enable ? SwapState::ENABLE
                                             : SwapState::DISABLE);
              }
            },
            weak_ptr_factory_.GetWeakPtr()),
        /* minimum_swapout_interval= */
        base::Seconds(kVmmSwapOutTimeIntervalSecond.Get()),
        /* swappable_checking_period= */
        base::Seconds(kVmmSwapArcSilenceIntervalSecond.Get()),
        std::make_unique<ArcSystemStateObservation>(context));
  }
  trim_call_ =
      base::BindRepeating(&ArcVmWorkingSetTrimExecutor::Trim, context_);
}

ArcVmmManager::~ArcVmmManager() = default;

void ArcVmmManager::SetSwapState(SwapState state) {
  if (!IsArcVmEnabled() || !arc_connected_) {
    LOG(ERROR) << "Failed to SetSwapState, ARCVM not enabled or connected.";
    return;
  }
  DVLOG(1) << "SetSwapState " << static_cast<int>(state);
  vm_tools::concierge::SwapOperation op;
  switch (state) {
    case SwapState::ENABLE:
      op = vm_tools::concierge::SwapOperation::ENABLE;
      break;
    case SwapState::FORCE_ENABLE:
      op = vm_tools::concierge::SwapOperation::FORCE_ENABLE;
      break;
    case SwapState::DISABLE:
      op = vm_tools::concierge::SwapOperation::DISABLE;
      break;
  }

  if (state == SwapState::DISABLE) {
    if (latest_swap_state_ == state) {
      return;
    }
    latest_swap_state_ = state;
    // The disable request will be sent immediately so the verify is
    // unnecessarily.
    SendSwapRequest(op, base::DoNothing());
    enabled_state_heartbeat_timer_.Stop();
    return;
  }

  // Do not re-send "enable" signal if the timer is waiting for resend it. But
  // allow "force-enable" bypass this restriction and redo the entire swap
  // process.
  if (latest_swap_state_ == SwapState::ENABLE && latest_swap_state_ == state &&
      enabled_state_heartbeat_timer_.IsRunning()) {
    // The state is not update, do not send request now but leave it to heart
    // beat timer.
    return;
  }

  latest_swap_state_ = state;

  // Reset the timer anyway since the enable state and force enable state may
  // overwrite each other.
  enabled_state_heartbeat_timer_.Start(
      FROM_HERE, kVmmSwapTrimInterval.Get(),
      base::BindRepeating(&ArcVmmManager::SetSwapState,
                          weak_ptr_factory_.GetWeakPtr(), state));

  // Enable or ForceEnable need shrink ARCVM memory first.
  if (!last_shrink_timestamp_ ||
      base::Time::Now() - last_shrink_timestamp_.value() >
          kVmmSwapMinShrinkInterval.Get()) {
    last_shrink_timestamp_ = base::Time::Now();
    last_shrink_result_ = false;
    // Following attempts to enable vmm-swap will be ignored until
    // `ShrinkArcVmMemoryAndEnableSwap()` finish. As a result, it will send an
    // enable swap request as a coalesced request if it succeeds to shrink the
    // ARCVM memory.
    ShrinkArcVmMemoryAndEnableSwap(op);
  } else {
    if (last_shrink_result_.value_or(false)) {
      // If recently the memory shrinking succeed, just send enable request
      // rather than shrink memory again.
      VerifyThenSendSwapRequest(op, base::DoNothing());
    } else {
      // If recently the memory failed to shrink, skip the request.
      VLOG(0) << "Skip enable swap request due to last arcvm memory shrink "
                 "failure";
    }
  }
}

bool ArcVmmManager::IsSwapped() const {
  // Currently ArcVmmManager assume after set vmm swap enabled, the system
  // under the "swapped" state.
  // In the future, is should be replaced by real swap state from the concierge,
  // because only the memory swapped and has been written to the disk can be
  // assumed as "swapped".
  return latest_swap_state_ != SwapState::DISABLE;
}

void ArcVmmManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}
void ArcVmmManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ArcVmmManager::OnConnectionReady() {
  arc_connected_ = true;
}
void ArcVmmManager::OnConnectionClosed() {
  arc_connected_ = false;
}

void ArcVmmManager::OnVmSwapping(
    const vm_tools::concierge::VmSwappingSignal& signal) {
  if (signal.name() != kArcVmName) {
    return;
  }
  if (signal.state() == vm_tools::concierge::SWAPPING_OUT) {
    VLOG(1) << "ArcVm swapping out.";
    for (auto& observer : observer_list_) {
      observer.OnArcVmSwappingOut();
    }
  } else if (signal.state() == vm_tools::concierge::SWAPPING_IN) {
    VLOG(1) << "ArcVm swapping in.";
    for (auto& observer : observer_list_) {
      observer.OnArcVmSwappingIn();
    }
  }
}

void ArcVmmManager::SendSwapRequest(
    vm_tools::concierge::SwapOperation operation,
    base::OnceClosure success_callback) {
  auto* client = ash::ConciergeClient::Get();
  if (!client) {
    LOG(ERROR) << "Cannot find concierge client to swap ARCVM";
    return;
  }

  VLOG(0) << "SendSwapRequest " << static_cast<int>(operation)
          << " to concierge.";
  vm_tools::concierge::SwapVmRequest request;
  request.set_name(kArcVmName);
  request.set_owner_id(user_id_hash_);
  request.set_operation(operation);
  client->SwapVm(
      request,
      base::BindOnce(
          [](vm_tools::concierge::SwapOperation op, base::OnceClosure cb,
             std::optional<vm_tools::concierge::SwapVmResponse> response) {
            if (!response.has_value()) {
              LOG(ERROR) << "Failed to receive SwapVm response.";
            } else if (!response->success()) {
              LOG(ERROR) << "Failed to send request: "
                         << vm_tools::concierge::SwapOperation_Name(op)
                         << ". Reason: " << response->failure_reason();
            } else {
              std::move(cb).Run();
            }
          },
          operation, std::move(success_callback)));
}

void ArcVmmManager::VerifyThenSendSwapRequest(
    vm_tools::concierge::SwapOperation operation,
    base::OnceClosure success_callback) {
  auto request_disable = latest_swap_state_ == SwapState::DISABLE;
  auto going_to_disable =
      operation == vm_tools::concierge::SwapOperation::DISABLE;
  if (request_disable != going_to_disable) {
    LOG(WARNING) << "Vmm swap request conflict in callback chain, ignored. "
                    "latest state: "
                 << static_cast<int>(latest_swap_state_)
                 << ", pending operation: " << static_cast<int>(operation);
    return;
  }
  SendSwapRequest(operation, std::move(success_callback));
}

void ArcVmmManager::SendAggressiveBalloonRequest(
    bool enable,
    base::OnceClosure success_callback) {
  auto* client = ash::ConciergeClient::Get();
  if (!client) {
    LOG(ERROR) << "Cannot find concierge client to swap ARCVM";
    return;
  }

  DVLOG(1) << "SendAggressiveBalloon state change " << enable
           << " request to concierge";
  vm_tools::concierge::AggressiveBalloonRequest request;
  request.set_name(kArcVmName);
  request.set_owner_id(user_id_hash_);
  request.set_enable(enable);
  client->AggressiveBalloon(
      request,
      base::BindOnce(
          [](bool enabled, base::OnceClosure cb,
             std::optional<vm_tools::concierge::AggressiveBalloonResponse>
                 response) {
            if (!response.has_value()) {
              LOG(ERROR) << "Failed to receive aggressive ballon response.";
            } else if (!response->success()) {
              LOG(ERROR) << "Failed to send aggressive balloon request: "
                         << enabled
                         << ". Reason: " << response->failure_reason();
            } else {
              std::move(cb).Run();
            }
          },
          enable, std::move(success_callback)));
}

void ArcVmmManager::VerifyThenSendAggressiveBalloonRequest(
    bool enable,
    base::OnceClosure success_callback) {
  auto request_disable = latest_swap_state_ == SwapState::DISABLE;
  if (request_disable == enable) {
    LOG(WARNING) << "Vmm swap request conflict in callback chain, ignored. "
                    "latest state: "
                 << static_cast<int>(latest_swap_state_)
                 << ", pending aggressive balloon: " << enable;
    return;
  }
  SendAggressiveBalloonRequest(enable, std::move(success_callback));
}

void ArcVmmManager::PostWithSwapDelay(base::OnceClosure callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, std::move(callback), swap_out_delay_);
}

void ArcVmmManager::ShrinkArcVmMemoryAndEnableSwap(
    vm_tools::concierge::SwapOperation requested_operation) {
  // Trim ARCVM memory before enable vmm swap in order to squeeze the vm
  // memory. Send enable operation if trim success.
  DCHECK(!trim_call_.is_null());
  DVLOG(1) << "ShrinkArcVmMemoryAndEnableSwap with request "
           << static_cast<int>(requested_operation);
  trim_call_.Run(
      base::BindOnce(
          [](base::OnceClosure success_closure, bool success,
             const std::string& failure_reason) {
            if (success) {
              std::move(success_closure).Run();
            } else {
              LOG(ERROR) << "Failed to trim ARCVM memory when enable vmm "
                            "swap, reason: "
                         << failure_reason;
            }
          },
          // If successfully execute trim, request enable aggressive balloon.
          base::BindOnce(
              &ArcVmmManager::VerifyThenSendAggressiveBalloonRequest,
              weak_ptr_factory_.GetWeakPtr(), true,
              // If enable aggressive balloon successful, set shrink
              // result and re-send enable swap request.
              base::BindOnce(&ArcVmmManager::SetShrinkResult,
                             weak_ptr_factory_.GetWeakPtr(), true)
                  .Then(base::BindOnce(
                      &ArcVmmManager::VerifyThenSendSwapRequest,
                      weak_ptr_factory_.GetWeakPtr(), requested_operation,
                      // Drop ARCVM page cache after successful enable swap.
                      base::BindOnce(
                          trim_call_, base::DoNothing(),
                          arc::ArcVmReclaimType::kReclaimGuestPageCaches,
                          arc::ArcSession::kNoPageLimit))))),
      arc::ArcVmReclaimType::kReclaimAllGuestOnly,
      arc::ArcSession::kNoPageLimit);
}

void ArcVmmManager::SetShrinkResult(bool success) {
  last_shrink_result_ = success;
}

// ArcVmmManager::AcceleratorTarget --------------------------------------------

class ArcVmmManager::AcceleratorTarget : public ui::AcceleratorTarget {
 public:
  explicit AcceleratorTarget(ArcVmmManager* manager)
      : manager_(manager),
        vmm_swap_enabled_(ui::VKEY_O, ash::kDebugModifier),
        vmm_swap_disabled_(ui::VKEY_P, ash::kDebugModifier) {
    ash::Shell::Get()->accelerator_controller()->Register(
        {vmm_swap_enabled_, vmm_swap_disabled_}, this);
  }
  AcceleratorTarget(const AcceleratorTarget&) = delete;
  AcceleratorTarget& operator=(const AcceleratorTarget&) = delete;
  ~AcceleratorTarget() override = default;

 private:
  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    if (accelerator == vmm_swap_enabled_) {
      DVLOG(1) << "Set force enable vmm swap state by keyboard shortcut.";
      manager_->SetSwapState(SwapState::FORCE_ENABLE);
    } else if (accelerator == vmm_swap_disabled_) {
      DVLOG(1) << "Set diable vmm swap state by keyboard shortcut.";
      manager_->SetSwapState(SwapState::DISABLE);
    } else {
      NOTREACHED_IN_MIGRATION();
      return false;
    }
    return true;
  }

  bool CanHandleAccelerators() const override { return true; }

  // The manager responsible for executing vmm commands.
  const raw_ptr<ArcVmmManager> manager_;

  // The accelerator to enable vmm swap for ARCVM.
  const ui::Accelerator vmm_swap_enabled_;

  // The accelerator to disable vmm swap for ARCVM.
  const ui::Accelerator vmm_swap_disabled_;
};

}  // namespace arc

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/vmm/arc_vmm_manager.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_features.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/shell.h"
#include "base/feature_list.h"
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

ArcVmmManager::ArcVmmManager(content::BrowserContext* context,
                             ArcBridgeService* bridge) {
  if (base::FeatureList::IsEnabled(kVmmSwapKeyboardShortcut)) {
    accelerator_ = std::make_unique<AcceleratorTarget>(this);
  }
}

ArcVmmManager::~ArcVmmManager() = default;

void ArcVmmManager::SetSwapState(bool enable) {
  NOTIMPLEMENTED();
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
      manager_->SetSwapState(true);
    } else if (accelerator == vmm_swap_disabled_) {
      manager_->SetSwapState(false);
    } else {
      NOTREACHED();
      return false;
    }
    return true;
  }

  bool CanHandleAccelerators() const override { return true; }

  // The manager responsible for executing vmm commands.
  ArcVmmManager* const manager_;

  // The accelerator to enable vmm swap for ARCVM.
  const ui::Accelerator vmm_swap_enabled_;

  // The accelerator to disable vmm swap for ARCVM.
  const ui::Accelerator vmm_swap_disabled_;
};

}  // namespace arc

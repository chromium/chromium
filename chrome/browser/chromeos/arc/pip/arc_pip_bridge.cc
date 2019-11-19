// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/pip/arc_pip_bridge.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/arc/pip/arc_picture_in_picture_window_controller_impl.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_features.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/picture_in_picture_window_controller.h"

namespace arc {

namespace {

// Singleton factory for ArcPipBridge.
class ArcPipBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcPipBridge,
          ArcPipBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcPipBridgeFactory";

  static ArcPipBridgeFactory* GetInstance() {
    return base::Singleton<ArcPipBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcPipBridgeFactory>;

  ArcPipBridgeFactory() = default;
  ~ArcPipBridgeFactory() override = default;
};

}  // namespace

// static
ArcPipBridge* ArcPipBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcPipBridgeFactory::GetForBrowserContext(context);
}

ArcPipBridge::ArcPipBridge(content::BrowserContext* context,
                           ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  DVLOG(2) << "ArcPipBridge::ArcPipBridge";
  arc_bridge_service_->pip()->SetHost(this);
  arc_bridge_service_->pip()->AddObserver(this);
}

ArcPipBridge::~ArcPipBridge() {
  DVLOG(2) << "ArcPipBridge::~ArcPipBridge";
  arc_bridge_service_->pip()->RemoveObserver(this);
  arc_bridge_service_->pip()->SetHost(nullptr);
}

void ArcPipBridge::OnConnectionReady() {
  DVLOG(1) << "ArcPipBridge::OnConnectionReady";
}

void ArcPipBridge::OnConnectionClosed() {
  DVLOG(1) << "ArcPipBridge::OnConnectionClosed";
}

void ArcPipBridge::OnPipEvent(arc::mojom::ArcPipEvent event) {
  DVLOG(1) << "ArcPipBridge::OnPipEvent";

  switch (event) {
    case mojom::ArcPipEvent::ENTER: {
      auto pip_window_controller =
          std::make_unique<ArcPictureInPictureWindowControllerImpl>(this);
      // Make sure not to close PIP if we are replacing an existing Android PIP.
      base::AutoReset<bool> auto_prevent_closing_pip(&prevent_closing_pip_,
                                                     true);
      PictureInPictureWindowManager::GetInstance()
          ->EnterPictureInPictureWithController(pip_window_controller.get());
      pip_window_controller_ = std::move(pip_window_controller);
      break;
    }
  }
}

void ArcPipBridge::ClosePip() {
  DVLOG(1) << "ArcPipBridge::ClosePip";

  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->pip(), ClosePip);
  if (!instance)
    return;

  if (!prevent_closing_pip_)
    instance->ClosePip();
}

}  // namespace arc

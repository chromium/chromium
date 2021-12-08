// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/arc_ash.h"

#include <utility>

#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

namespace crosapi {

ArcAsh::ArcAsh() = default;
ArcAsh::~ArcAsh() = default;

void ArcAsh::MaybeSetProfile(Profile* profile) {
  if (profile_) {
    LOG(WARNING) << "profile_ is already initialized. Ignoring SetProfile.";
    return;
  }

  profile_ = std::move(profile);
  auto* bridge = arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (bridge)
    bridge->AddObserver(this);
}

void ArcAsh::BindReceiver(mojo::PendingReceiver<mojom::Arc> receiver) {
  // profile_ should be set beforehand.
  DCHECK(profile_);
  receivers_.Add(this, std::move(receiver));
}

void ArcAsh::AddObserver(mojo::PendingRemote<mojom::ArcObserver> observer) {
  mojo::Remote<mojom::ArcObserver> remote(std::move(observer));
  observers_.Add(std::move(remote));
}

void ArcAsh::OnArcIntentHelperBridgeDestruction() {
  // This method should not be called if profie_ is not set.
  DCHECK(profile_);

  // Remove observers here instead of ~ArcAsh() since ArcIntentHelperBridge
  // is destroyed before ~ArcAsh() is called.
  // Both of them are destroyed in
  // ChromeBrowserMainPartsAsh::PostMainMessageLoopRun(), but
  // ArcIntentHelperBridge is destroyed in
  // ChromeBrowserMainPartsLinux::PostMainMessageLoopRun() while ArcAsh is
  // destroyed in crosapi_manager_.reset() which runs later.
  auto* bridge = arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (bridge)
    bridge->RemoveObserver(this);
}

void ArcAsh::RequestActivityIcons(
    std::vector<mojom::ActivityNamePtr> activities,
    mojom::ScaleFactor scale_factor,
    RequestActivityIconsCallback) {
  NOTIMPLEMENTED();
}

void ArcAsh::RequestUrlHandlerList(const std::string& url,
                                   RequestUrlHandlerListCallback callback) {
  NOTIMPLEMENTED();
}

void ArcAsh::OnIconInvalidated(const std::string& package_name) {
  for (auto& observer : observers_)
    observer->OnIconInvalidated(package_name);
}

}  // namespace crosapi

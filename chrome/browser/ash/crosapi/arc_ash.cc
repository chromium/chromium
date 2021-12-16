// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/arc_ash.h"

#include <utility>

#include "ash/components/arc/mojom/scale_factor.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

// Check ScaleFactor values are the same for arc::mojom and crosapi::mojom.
#define STATIC_ASSERT_SCALE_FACTOR(v)                               \
  static_assert(static_cast<int>(crosapi::mojom::ScaleFactor::v) == \
                    static_cast<int>(arc::mojom::ScaleFactor::v),   \
                "mismatching enums: " #v)
STATIC_ASSERT_SCALE_FACTOR(SCALE_FACTOR_NONE);
STATIC_ASSERT_SCALE_FACTOR(SCALE_FACTOR_100P);
STATIC_ASSERT_SCALE_FACTOR(SCALE_FACTOR_200P);
STATIC_ASSERT_SCALE_FACTOR(SCALE_FACTOR_300P);

namespace crosapi {

namespace {

// Retrurns IntentHelperHolder for getting mojom API.
// Return nullptr if not ready or supported.
arc::ConnectionHolder<arc::mojom::IntentHelperInstance,
                      arc::mojom::IntentHelperHost>*
GetIntentHelperHolder() {
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    LOG(WARNING) << "ARC is not ready";
    return nullptr;
  }

  auto* intent_helper_holder =
      arc_service_manager->arc_bridge_service()->intent_helper();
  if (!intent_helper_holder->IsConnected()) {
    LOG(WARNING) << "ARC intent helper instance is not ready.";
    return nullptr;
  }

  return intent_helper_holder;
}

}  // namespace

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
    RequestActivityIconsCallback callback) {
  auto* intent_helper_holder = GetIntentHelperHolder();
  if (!intent_helper_holder)
    return;

  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(intent_helper_holder, RequestActivityIcons);
  if (!instance) {
    LOG(WARNING) << "RequestActivityIcons is not supported.";
    return;
  }

  // Convert activities to arc::mojom::ActivityNamePtr from
  // crosapi::mojom::ActivityNamePtr.
  std::vector<arc::mojom::ActivityNamePtr> converted_activities;
  for (const auto& activity : activities) {
    converted_activities.push_back(arc::mojom::ActivityName::New(
        activity->package_name, activity->activity_name));
  }
  instance->RequestActivityIcons(
      std::move(converted_activities), arc::mojom::ScaleFactor(scale_factor),
      base::BindOnce(&ArcAsh::ConvertActivityIcons,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcAsh::ConvertActivityIcons(
    RequestActivityIconsCallback callback,
    std::vector<arc::mojom::ActivityIconPtr> icons) {
  // Convert icons to crosapi::mojom::ActivityIconPtr from
  // arc::mojom::ActivityIconPtr.
  std::vector<mojom::ActivityIconPtr> converted_icons;
  for (const auto& icon : icons) {
    converted_icons.push_back(mojom::ActivityIcon::New(
        mojom::ActivityName::New(icon->activity->package_name,
                                 icon->activity->activity_name),
        icon->width, icon->height, icon->icon,
        mojom::RawIconPngData::New(
            icon->icon_png_data->is_adaptive_icon,
            icon->icon_png_data->icon_png_data,
            icon->icon_png_data->foreground_icon_png_data,
            icon->icon_png_data->background_icon_png_data)));
  }
  std::move(callback).Run(std::move(converted_icons));
}

void ArcAsh::RequestUrlHandlerList(const std::string& url,
                                   RequestUrlHandlerListCallback callback) {
  auto* intent_helper_holder = GetIntentHelperHolder();
  if (!intent_helper_holder)
    return;

  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(intent_helper_holder, RequestUrlHandlerList);
  if (!instance) {
    LOG(WARNING) << "RequestUrlHandlerList is not supported.";
    return;
  }

  instance->RequestUrlHandlerList(
      url, base::BindOnce(&ArcAsh::ConvertIntentHandlerInfo,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcAsh::ConvertIntentHandlerInfo(
    RequestUrlHandlerListCallback callback,
    std::vector<arc::mojom::IntentHandlerInfoPtr> handlers) {
  // Convert handlers to crosapi::mojom::IntentHandlerInfoPtr from
  // arc::mojom::IntentHandlerInfoPtr.
  std::vector<mojom::IntentHandlerInfoPtr> converted_handlers;
  for (const auto& handler : handlers) {
    mojom::IntentHandlerInfoPtr converted_handler(mojom::IntentHandlerInfo::New(
        handler->name, handler->package_name, handler->activity_name));
    converted_handlers.push_back(std::move(converted_handler));
  }
  std::move(callback).Run(std::move(converted_handlers));
}

void ArcAsh::HandleUrl(const std::string& url,
                       const std::string& package_name) {
  auto* intent_helper_holder = GetIntentHelperHolder();
  if (!intent_helper_holder)
    return;

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(intent_helper_holder, HandleUrl);
  if (!instance) {
    LOG(WARNING) << "HandleUrl is not supported.";
    return;
  }

  instance->HandleUrl(url, package_name);
}

void ArcAsh::OnIconInvalidated(const std::string& package_name) {
  for (auto& observer : observers_)
    observer->OnIconInvalidated(package_name);
}

}  // namespace crosapi

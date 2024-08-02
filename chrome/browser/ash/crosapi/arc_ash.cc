// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/crosapi/arc_ash.h"

#include <utility>

#include "ash/components/arc/mojom/scale_factor.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
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

// Used for converting RawIconPngData to ImageSkia.
// These petemeters must be consistent with SmartSelection's icon configuration.
constexpr size_t kBytesPerPixel = 4;  // BGRA
constexpr size_t kSmallIconSizeInDip = 16;
constexpr size_t kMaxIconSizeInPx = 200;

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

mojom::ActivityNamePtr ConvertArcActivityName(
    arc::mojom::ActivityNamePtr activity) {
  return mojom::ActivityName::New(activity->package_name,
                                  activity->activity_name);
}

mojom::IntentInfoPtr ConvertArcIntentInfo(arc::mojom::IntentInfoPtr intent) {
  return mojom::IntentInfo::New(intent->action, intent->categories,
                                intent->data, intent->type, intent->ui_bypassed,
                                intent->extras);
}

void OnIsInstallable(mojom::Arc::IsInstallableCallback callback,
                     bool installable) {
  std::move(callback).Run(
      installable ? crosapi::mojom::IsInstallableResult::kInstallable
                  : crosapi::mojom::IsInstallableResult::kNotInstallable);
}

}  // namespace

ArcAsh::ArcAsh() = default;

ArcAsh::~ArcAsh() = default;

void ArcAsh::MaybeSetProfile(Profile* profile) {
  CHECK(profile);
  if (profile_) {
    VLOG(1) << "ArcAsh service is already initialized. Skip init.";
    return;
  }

  profile_ = profile;
  auto* bridge = arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (bridge) {
    bridge->AddObserver(this);
  }
  profile_observation_.Observe(profile_);
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

void ArcAsh::OnArcIntentHelperBridgeShutdown(
    arc::ArcIntentHelperBridge* bridge) {
  // Remove observers here instead of ~ArcAsh() since ArcIntentHelperBridge
  // is shut down before ~ArcAsh() is called.
  // Both of them are destroyed in
  // ChromeBrowserMainPartsAsh::PostMainMessageLoopRun(), but
  // ArcIntentHelperBridge is shut down and destroyed in
  // ChromeBrowserMainPartsLinux::PostMainMessageLoopRun() while ArcAsh is
  // destroyed in crosapi_manager_.reset() which runs later.
  if (bridge) {
    bridge->RemoveObserver(this);
  }
}

void ArcAsh::RequestActivityIcons(
    std::vector<mojom::ActivityNamePtr> activities,
    mojom::ScaleFactor scale_factor,
    RequestActivityIconsCallback callback) {
  auto* intent_helper_holder = GetIntentHelperHolder();
  if (!intent_helper_holder) {
    std::move(callback).Run(
        std::vector<mojom::ActivityIconPtr>(),
        mojom::RequestActivityIconsStatus::kArcNotAvailable);
    return;
  }

  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(intent_helper_holder, RequestActivityIcons);
  if (!instance) {
    LOG(WARNING) << "RequestActivityIcons is not supported.";
    std::move(callback).Run(
        std::vector<mojom::ActivityIconPtr>(),
        mojom::RequestActivityIconsStatus::kArcNotAvailable);
    return;
  }

  // Convert activities to arc::mojom::ActivityNamePtr from
  // crosapi::mojom::ActivityNamePtr.
  std::vector<arc::mojom::ActivityNamePtr> converted_activities;
  converted_activities.reserve(activities.size());
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
  converted_icons.reserve(icons.size());
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
  std::move(callback).Run(std::move(converted_icons),
                          mojom::RequestActivityIconsStatus::kSuccess);
}

void ArcAsh::RequestUrlHandlerList(const std::string& url,
                                   RequestUrlHandlerListCallback callback) {
  auto* intent_helper_holder = GetIntentHelperHolder();
  if (!intent_helper_holder) {
    std::move(callback).Run(
        std::vector<mojom::IntentHandlerInfoPtr>(),
        mojom::RequestUrlHandlerListStatus::kArcNotAvailable);
    return;
  }

  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(intent_helper_holder, RequestUrlHandlerList);
  if (!instance) {
    LOG(WARNING) << "RequestUrlHandlerList is not supported.";
    std::move(callback).Run(
        std::vector<mojom::IntentHandlerInfoPtr>(),
        mojom::RequestUrlHandlerListStatus::kArcNotAvailable);
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
  std::move(callback).Run(std::move(converted_handlers),
                          mojom::RequestUrlHandlerListStatus::kSuccess);
}

void ArcAsh::RequestTextSelectionActions(
    const std::string& text,
    mojom::ScaleFactor scale_factor,
    RequestTextSelectionActionsCallback callback) {
  auto* intent_helper_holder = GetIntentHelperHolder();
  if (!intent_helper_holder) {
    std::move(callback).Run(
        mojom::RequestTextSelectionActionsStatus::kArcNotAvailable,
        std::vector<mojom::TextSelectionActionPtr>());
    return;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(intent_helper_holder,
                                               RequestTextSelectionActions);
  if (!instance) {
    LOG(WARNING) << "RequestTextSelectionActions is not supported.";
    std::move(callback).Run(
        mojom::RequestTextSelectionActionsStatus::kArcNotAvailable,
        std::vector<mojom::TextSelectionActionPtr>());
    return;
  }

  instance->RequestTextSelectionActions(
      text, arc::mojom::ScaleFactor(scale_factor),
      base::BindOnce(&ArcAsh::ConvertTextSelectionActions,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcAsh::ConvertTextSelectionActions(
    RequestTextSelectionActionsCallback callback,
    std::vector<arc::mojom::TextSelectionActionPtr> actions) {
  size_t actions_count = actions.size();
  auto converted_actions =
      std::vector<mojom::TextSelectionActionPtr>(actions_count);
  auto* converted_actions_ptr = converted_actions.data();

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      actions_count,
      base::BindOnce(
          [](std::vector<mojom::TextSelectionActionPtr> actions,
             RequestTextSelectionActionsCallback cb) {
            std::move(cb).Run(
                mojom::RequestTextSelectionActionsStatus::kSuccess,
                std::move(actions));
          },
          std::move(converted_actions), std::move(callback)));

  for (size_t idx = 0; idx < actions_count; ++idx) {
    auto action = std::move(actions[idx]);
    auto* converted_action = &converted_actions_ptr[idx];

    // If actions[idx]->icon doesn't meet the size condition, skip generating
    // image.
    if (action->icon->width > kMaxIconSizeInPx ||
        action->icon->height > kMaxIconSizeInPx || action->icon->width == 0 ||
        action->icon->height == 0 ||
        action->icon->icon.size() !=
            (action->icon->width * action->icon->height * kBytesPerPixel)) {
      ConvertTextSelectionAction(converted_action, std::move(action),
                                 barrier_closure, gfx::ImageSkia());
      continue;
    }

    // Generate ImageSkia icon.
    auto icon_png_data = std::move(action->icon->icon_png_data);
    apps::ArcRawIconPngDataToImageSkia(
        std::move(icon_png_data), kSmallIconSizeInDip,
        base::BindOnce(&ArcAsh::ConvertTextSelectionAction,
                       weak_ptr_factory_.GetWeakPtr(), converted_action,
                       std::move(action), barrier_closure));
  }
}

void ArcAsh::ConvertTextSelectionAction(
    mojom::TextSelectionActionPtr* converted_action,
    arc::mojom::TextSelectionActionPtr action,
    base::OnceClosure callback,
    const gfx::ImageSkia& image) {
  // Convert actions to crosapi::mojom::TextSelectionActionPtr from
  // arc::mojom::TextSelectionActionPtr and ImageSkia icon.

  // Generate app_id by looking up ArcAppListPrefs.
  std::string app_id = ArcAppListPrefs::Get(profile_)->GetAppIdByPackageName(
      action->activity->package_name);
  *converted_action = mojom::TextSelectionAction::New(
      std::move(app_id), image,
      ConvertArcActivityName(std::move(action->activity)),
      std::move(action->title),
      ConvertArcIntentInfo(std::move(action->action_intent)));

  std::move(callback).Run();
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

void ArcAsh::HandleIntent(mojom::IntentInfoPtr intent,
                          mojom::ActivityNamePtr activity) {
  auto* intent_helper_holder = GetIntentHelperHolder();
  if (!intent_helper_holder)
    return;

  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(intent_helper_holder, HandleIntent);
  if (!instance) {
    LOG(WARNING) << "HandleIntent is not supported.";
    return;
  }

  arc::mojom::IntentInfoPtr converted_intent = arc::mojom::IntentInfo::New();
  converted_intent->action = intent->action;
  converted_intent->categories = intent->categories;
  converted_intent->data = intent->data;
  converted_intent->type = intent->type;
  converted_intent->ui_bypassed = intent->ui_bypassed;
  converted_intent->extras = intent->extras;
  instance->HandleIntent(std::move(converted_intent),
                         arc::mojom::ActivityName::New(
                             activity->package_name, activity->activity_name));
}

void ArcAsh::AddPreferredPackage(const std::string& package_name) {
  auto* intent_helper_holder = GetIntentHelperHolder();
  if (!intent_helper_holder)
    return;

  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(intent_helper_holder, AddPreferredPackage);
  if (!instance) {
    LOG(WARNING) << "AddPreferredPackage is not supported.";
    return;
  }

  instance->AddPreferredPackage(package_name);
}

void ArcAsh::IsInstallable(const std::string& package_name,
                           IsInstallableCallback callback) {
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    std::move(callback).Run(
        crosapi::mojom::IsInstallableResult::kArcIsNotRunning);
    return;
  }
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->app(), IsInstallable);
  if (!instance) {
    std::move(callback).Run(crosapi::mojom::IsInstallableResult::kArcIsTooOld);
    return;
  }
  instance->IsInstallable(
      package_name, base::BindOnce(&OnIsInstallable, std::move(callback)));
}

void ArcAsh::OnIconInvalidated(const std::string& package_name) {
  for (auto& observer : observers_)
    observer->OnIconInvalidated(package_name);
}

void ArcAsh::OnProfileWillBeDestroyed(Profile* profile) {
  CHECK_EQ(profile_, profile);
  profile_ = nullptr;
  profile_observation_.Reset();
}

}  // namespace crosapi

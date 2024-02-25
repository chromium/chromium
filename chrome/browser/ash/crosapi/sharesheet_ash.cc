// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/sharesheet_ash.h"

#include <utility>

#include "ash/shell.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/ash/crosapi/window_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
#include "chromeos/crosapi/mojom/sharesheet_mojom_traits.h"
#include "ui/views/widget/widget.h"

namespace {

gfx::NativeWindow GetNativeWindowFromId(const std::string& window_id) {
  return crosapi::GetShellSurfaceWindow(window_id);
}

void OnClosedCallbackWrapper(
    crosapi::SharesheetAsh::ShowBubbleWithOnClosedCallback callback,
    views::Widget::ClosedReason reason) {
  if (callback)
    std::move(callback).Run();
}

}  // namespace

namespace crosapi {

SharesheetAsh::SharesheetAsh() = default;
SharesheetAsh::~SharesheetAsh() = default;

void SharesheetAsh::MaybeSetProfile(Profile* profile) {
  CHECK(profile);
  if (profile_) {
    VLOG(1) << "SharesheetAsh is already initialized. Skip init.";
    return;
  }

  profile_ = profile;
  profile_observation_.Observe(profile_);
}

void SharesheetAsh::BindReceiver(
    mojo::PendingReceiver<mojom::Sharesheet> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void SharesheetAsh::ShowBubble(const std::string& window_id,
                               sharesheet::LaunchSource source,
                               crosapi::mojom::IntentPtr intent,
                               ShowBubbleCallback callback) {
  DCHECK(profile_);
  if (source == sharesheet::LaunchSource::kUnknown)
    return;

  sharesheet::SharesheetService* const sharesheet_service =
      sharesheet::SharesheetServiceFactory::GetForProfile(profile_);
  sharesheet_service->ShowBubble(
      apps_util::CreateAppServiceIntentFromCrosapi(intent, profile_), source,
      base::BindOnce(&GetNativeWindowFromId, window_id), std::move(callback));
}

void SharesheetAsh::ShowBubbleWithOnClosed(
    const std::string& window_id,
    sharesheet::LaunchSource source,
    crosapi::mojom::IntentPtr intent,
    ShowBubbleWithOnClosedCallback callback) {
  DCHECK(profile_);
  if (source == sharesheet::LaunchSource::kUnknown)
    return;

  sharesheet::SharesheetService* const sharesheet_service =
      sharesheet::SharesheetServiceFactory::GetForProfile(profile_);
  sharesheet_service->ShowBubble(
      apps_util::CreateAppServiceIntentFromCrosapi(intent, profile_), source,
      base::BindOnce(&GetNativeWindowFromId, window_id), base::DoNothing(),
      base::BindOnce(&OnClosedCallbackWrapper, std::move(callback)));
}

void SharesheetAsh::CloseBubble(const std::string& window_id) {
  gfx::NativeWindow window = crosapi::GetShellSurfaceWindow(window_id);
  if (!window)
    return;

  sharesheet::SharesheetService* const sharesheet_service =
      sharesheet::SharesheetServiceFactory::GetForProfile(profile_);
  sharesheet::SharesheetController* sharesheet_controller =
      sharesheet_service->GetSharesheetController(window);
  if (!sharesheet_controller)
    return;

  sharesheet_controller->CloseBubble(sharesheet::SharesheetResult::kCancel);
}

void SharesheetAsh::OnProfileWillBeDestroyed(Profile* profile) {
  CHECK_EQ(profile_, profile);
  profile_ = nullptr;
  profile_observation_.Reset();
}

}  // namespace crosapi

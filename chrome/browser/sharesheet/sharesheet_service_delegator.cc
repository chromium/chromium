// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_service_delegator.h"

#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_bubble_view_delegate.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_header_view.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/view.h"

namespace sharesheet {

SharesheetServiceDelegator::SharesheetServiceDelegator(
    gfx::NativeWindow native_window,
    SharesheetService* sharesheet_service)
    : native_window_(native_window), sharesheet_service_(sharesheet_service) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  sharesheet_controller_ =
      std::make_unique<ash::sharesheet::SharesheetBubbleViewDelegate>(
          native_window_, this);
#else
  NOTIMPLEMENTED();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

SharesheetServiceDelegator::~SharesheetServiceDelegator() = default;

gfx::NativeWindow SharesheetServiceDelegator::GetNativeWindow() {
  DCHECK(native_window_);
  return native_window_;
}

SharesheetController* SharesheetServiceDelegator::GetSharesheetController() {
  DCHECK(sharesheet_controller_);
  return sharesheet_controller_.get();
}

Profile* SharesheetServiceDelegator::GetProfile() {
  DCHECK(sharesheet_service_);
  return sharesheet_service_->GetProfile();
}

SharesheetUiDelegate* SharesheetServiceDelegator::GetUiDelegateForTesting() {
  return sharesheet_controller_.get();
}

void SharesheetServiceDelegator::ShowBubble(
    std::vector<TargetInfo> targets,
    apps::IntentPtr intent,
    DeliveredCallback delivered_callback,
    CloseCallback close_callback) {
  if (sharesheet_controller_) {
    sharesheet_controller_->ShowBubble(std::move(targets), std::move(intent),
                                       std::move(delivered_callback),
                                       std::move(close_callback));
    return;
  }
  NOTREACHED_IN_MIGRATION();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Skips the generic Sharesheet bubble and directly displays the
// NearbyShare bubble dialog.
void SharesheetServiceDelegator::ShowNearbyShareBubbleForArc(
    apps::IntentPtr intent,
    DeliveredCallback delivered_callback,
    CloseCallback close_callback) {
  DCHECK(sharesheet_controller_);
  sharesheet_controller_->ShowNearbyShareBubbleForArc(
      std::move(intent), std::move(delivered_callback),
      std::move(close_callback));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Invoked immediately after an action has launched in the event that UI
// changes need to occur at this point.
void SharesheetServiceDelegator::OnActionLaunched(bool has_action_view) {
  if (sharesheet_controller_) {
    sharesheet_controller_->OnActionLaunched(has_action_view);
    return;
  }
  NOTREACHED_IN_MIGRATION();
}

void SharesheetServiceDelegator::CloseBubble(SharesheetResult result) {
  if (sharesheet_controller_) {
    sharesheet_controller_->CloseBubble(result);
    return;
  }
  NOTREACHED_IN_MIGRATION();
}

void SharesheetServiceDelegator::OnBubbleClosed(
    const std::optional<ShareActionType>& share_action_type) {
  DCHECK(sharesheet_service_);
  sharesheet_service_->OnBubbleClosed(native_window_, share_action_type);
  // This object is now deleted and nothing can be accessed any more.
}

void SharesheetServiceDelegator::OnTargetSelected(
    const TargetType type,
    const std::optional<ShareActionType>& share_action_type,
    const std::optional<std::u16string>& app_name,
    apps::IntentPtr intent,
    views::View* share_action_view) {
  DCHECK(sharesheet_service_);
  sharesheet_service_->OnTargetSelected(native_window_, type, share_action_type,
                                        app_name, std::move(intent),
                                        share_action_view);
}

bool SharesheetServiceDelegator::OnAcceleratorPressed(
    const ui::Accelerator& accelerator,
    const ShareActionType share_action_type) {
  DCHECK(sharesheet_service_);
  return sharesheet_service_->OnAcceleratorPressed(accelerator,
                                                   share_action_type);
}

const gfx::VectorIcon* SharesheetServiceDelegator::GetVectorIcon(
    const std::optional<ShareActionType>& share_action_type) {
  DCHECK(sharesheet_service_);
  return sharesheet_service_->GetVectorIcon(share_action_type);
}

}  // namespace sharesheet

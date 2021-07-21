// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_service_delegate.h"

#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "ui/views/view.h"

namespace sharesheet {

SharesheetServiceDelegate::SharesheetServiceDelegate(
    gfx::NativeWindow native_window,
    SharesheetService* sharesheet_service)
    : native_window_(native_window), sharesheet_service_(sharesheet_service) {}

gfx::NativeWindow SharesheetServiceDelegate::GetNativeWindow() {
  return native_window_;
}

void SharesheetServiceDelegate::ShowBubble(
    std::vector<TargetInfo> targets,
    apps::mojom::IntentPtr intent,
    sharesheet::DeliveredCallback delivered_callback,
    sharesheet::CloseCallback close_callback) {
  NOTIMPLEMENTED();
}

void SharesheetServiceDelegate::OnActionLaunched() {
  NOTIMPLEMENTED();
}

void SharesheetServiceDelegate::OnBubbleClosed(
    const std::u16string& active_action) {
  sharesheet_service_->OnBubbleClosed(native_window_, active_action);
  // This object is now deleted and nothing can be accessed any more.
}

void SharesheetServiceDelegate::OnTargetSelected(
    const std::u16string& target_name,
    const TargetType type,
    apps::mojom::IntentPtr intent,
    views::View* share_action_view) {
  sharesheet_service_->OnTargetSelected(native_window_, target_name, type,
                                        std::move(intent), share_action_view);
}

bool SharesheetServiceDelegate::OnAcceleratorPressed(
    const ui::Accelerator& accelerator,
    const std::u16string& active_action) {
  return sharesheet_service_->OnAcceleratorPressed(accelerator, active_action);
}

const gfx::VectorIcon* SharesheetServiceDelegate::GetVectorIcon(
    const std::u16string& display_name) {
  return sharesheet_service_->GetVectorIcon(display_name);
}

Profile* SharesheetServiceDelegate::GetProfile() {
  return sharesheet_service_->GetProfile();
}

void SharesheetServiceDelegate::SetSharesheetSize(int width, int height) {}

void SharesheetServiceDelegate::CloseSharesheet(SharesheetResult result) {}

}  // namespace sharesheet

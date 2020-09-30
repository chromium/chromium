// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_service_delegate.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
#include "chrome/browser/ui/views/sharesheet/sharesheet_bubble_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/view.h"

namespace sharesheet {

SharesheetServiceDelegate::SharesheetServiceDelegate(
    uint32_t id,
    content::WebContents* web_contents,
    SharesheetService* sharesheet_service)
    : id_(id),
      sharesheet_bubble_view_(
          std::make_unique<SharesheetBubbleView>(web_contents, this)),
      sharesheet_service_(sharesheet_service) {}

SharesheetServiceDelegate::~SharesheetServiceDelegate() = default;

void SharesheetServiceDelegate::ShowBubble(
    std::vector<TargetInfo> targets,
    apps::mojom::IntentPtr intent,
    sharesheet::CloseCallback close_callback) {
  sharesheet_bubble_view_->ShowBubble(std::move(targets), std::move(intent),
                                      std::move(close_callback));
}

void SharesheetServiceDelegate::OnBubbleClosed(
    const base::string16& active_action) {
  sharesheet_bubble_view_.release();
  sharesheet_service_->OnBubbleClosed(id_, active_action);
}

void SharesheetServiceDelegate::OnActionLaunched() {
  sharesheet_bubble_view_->ShowActionView();
}

void SharesheetServiceDelegate::OnTargetSelected(
    const base::string16& target_name,
    const TargetType type,
    apps::mojom::IntentPtr intent,
    views::View* share_action_view) {
  sharesheet_service_->OnTargetSelected(id_, target_name, type,
                                        std::move(intent), share_action_view);
}

uint32_t SharesheetServiceDelegate::GetId() {
  return id_;
}

Profile* SharesheetServiceDelegate::GetProfile() {
  return sharesheet_service_->GetProfile();
}

void SharesheetServiceDelegate::SetSharesheetSize(const int& width,
                                                  const int& height) {
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);
  sharesheet_bubble_view_->ResizeBubble(width, height);
}

void SharesheetServiceDelegate::CloseSharesheet() {
  sharesheet_bubble_view_->CloseBubble();
}

}  // namespace sharesheet

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/annotator/annotator_client_impl.h"

#include "ash/annotator/annotator_controller.h"
#include "ash/public/cpp/annotator/annotator_tool.h"
#include "ash/shell.h"
#include "ash/webui/annotator/annotations_overlay_view_impl.h"
#include "ash/webui/annotator/untrusted_annotator_page_handler_impl.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"

AnnotatorClientImpl::AnnotatorClientImpl(
    ash::AnnotatorControllerBase* annotator_controller)
    : annotator_controller_(annotator_controller) {
  annotator_controller_->SetToolClient(this);
}

AnnotatorClientImpl::AnnotatorClientImpl()
    : AnnotatorClientImpl(ash::Shell::Get()->annotator_controller()) {}

AnnotatorClientImpl::~AnnotatorClientImpl() {
  annotator_controller_->SetToolClient(nullptr);
}

void AnnotatorClientImpl::SetAnnotatorPageHandler(
    ash::UntrustedAnnotatorPageHandlerImpl* handler) {
  annotator_handler_ = handler;
}

void AnnotatorClientImpl::ResetAnnotatorPageHandler(
    ash::UntrustedAnnotatorPageHandlerImpl* handler) {
  if (annotator_handler_ == handler) {
    annotator_handler_ = nullptr;
  }
}

void AnnotatorClientImpl::SetTool(const ash::AnnotatorTool& tool) {
  DCHECK(annotator_handler_);
  annotator_handler_->SetTool(tool);
}

void AnnotatorClientImpl::Clear() {
  DCHECK(annotator_handler_);
  annotator_handler_->Clear();
}

std::unique_ptr<ash::AnnotationsOverlayView>
AnnotatorClientImpl::CreateAnnotationsOverlayView() const {
  // TODO(crbug.com/278643115): Take the account_id from the callers.
  const auto* active_session =
      session_manager::SessionManager::Get()->GetActiveSession();

  return std::make_unique<AnnotationsOverlayViewImpl>(
      active_session
          ? ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
                active_session->account_id())
          : nullptr);
}

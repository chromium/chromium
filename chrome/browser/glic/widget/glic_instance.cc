// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_instance.h"

#include "base/notimplemented.h"
#include "chrome/browser/glic/glic_zero_state_suggestions_manager.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/glic_actor_controller.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/ui/browser_window.h"

namespace glic {

GlicInstance::GlicInstance() = default;

GlicInstance::~GlicInstance() = default;

void GlicInstance::AttachPanel() {
  if (!attachment_delegate_) {
    return;
  }
  attachment_delegate_->AttachPanel(this);
}

void GlicInstance::DetachPanel() {
  if (!attachment_delegate_) {
    return;
  }
  attachment_delegate_->DetachPanel(this);
}

bool GlicInstance::IsShowing() const {
  return embedder_ && embedder_->IsShowing();
}

void GlicInstance::ClosePanelAndShutdown() {
  NOTIMPLEMENTED();
}
void GlicInstance::CreateTab() {
  NOTIMPLEMENTED();
}
void GlicInstance::CreateTask() {
  NOTIMPLEMENTED();
}
void GlicInstance::PerformActions() {
  NOTIMPLEMENTED();
}
void GlicInstance::StopActorTask() {
  NOTIMPLEMENTED();
}
void GlicInstance::PauseActorTask() {
  NOTIMPLEMENTED();
}
void GlicInstance::ResumeActorTask() {
  NOTIMPLEMENTED();
}
void GlicInstance::GetZeroStateSuggestionsAndSubscribe() {
  NOTIMPLEMENTED();
}
void GlicInstance::GetZeroStateSuggestionsForFocusedTab() {
  NOTIMPLEMENTED();
}

}  // namespace glic

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_mediator.h"

#include "base/check_op.h"
#include "chrome/browser/ui/webui/ash/mako/mako_ui.h"

namespace ash {
namespace input_method {
namespace {

EditorMediator* g_instance_ = nullptr;

}  // namespace

EditorMediator::EditorMediator() : editor_instance_impl_(this) {
  DCHECK(!g_instance_);
  g_instance_ = this;
}

EditorMediator::~EditorMediator() {
  DCHECK_EQ(g_instance_, this);
  g_instance_ = nullptr;
}

EditorMediator* EditorMediator::Get() {
  return g_instance_;
}

void EditorMediator::BindEditorInstance(
    mojo::PendingReceiver<mojom::EditorInstance> pending_receiver) {
  editor_instance_impl_.BindReceiver(std::move(pending_receiver));
}

void EditorMediator::HandleTrigger() {
  mako_page_handler_ = std::make_unique<ash::MakoPageHandler>();
}

void EditorMediator::OnFocus(int context_id) {
  text_actuator_.OnFocus(context_id);
}

void EditorMediator::OnBlur() {
  text_actuator_.OnBlur();
}

void EditorMediator::CommitEditorResult(std::string_view text) {
  // This assumes that focus will return to the original text input client after
  // the mako web ui is hidden from view. Thus we queue the text to be inserted
  // here rather then insert it directly into the input.
  text_actuator_.InsertTextOnNextFocus(text);
  // After queuing the text to be inserted, closing the mako web ui should
  // return the focus back to the original input.
  if (mako_page_handler_ != nullptr) {
    mako_page_handler_->CloseUI();
    mako_page_handler_ = nullptr;
  }
}

}  // namespace input_method
}  // namespace ash

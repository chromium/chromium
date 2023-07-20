// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_mediator.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/webui/ash/mako/mako_ui.h"

namespace ash {
namespace input_method {
namespace {

EditorMediator* g_instance_ = nullptr;

}  // namespace

EditorMediator::EditorMediator() {
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

void EditorMediator::HandleTrigger() {
  MakoUntrustedUI::Show();
}

}  // namespace input_method
}  // namespace ash

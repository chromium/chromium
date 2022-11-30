// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/webauthn_dialog_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {
WebAuthNDialogController* g_instance = nullptr;
}

// static
WebAuthNDialogController* WebAuthNDialogController::Get() {
  return g_instance;
}

WebAuthNDialogController::WebAuthNDialogController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

WebAuthNDialogController::~WebAuthNDialogController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash

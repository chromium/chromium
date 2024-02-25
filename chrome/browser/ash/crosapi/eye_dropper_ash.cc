// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/eye_dropper_ash.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "components/eye_dropper/eye_dropper_view.h"
#include "ui/aura/window.h"

namespace crosapi {

EyeDropperAsh::EyeDropperAsh() = default;

EyeDropperAsh::~EyeDropperAsh() = default;

void EyeDropperAsh::BindReceiver(
    mojo::PendingReceiver<mojom::EyeDropper> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void EyeDropperAsh::ShowEyeDropper(
    mojo::PendingRemote<mojom::EyeDropperListener> listener) {
  listener_ = mojo::Remote<mojom::EyeDropperListener>(std::move(listener));
  listener_.set_disconnect_handler(
      base::BindOnce(&EyeDropperAsh::OnDisconnect, weak_factory_.GetWeakPtr()));
  auto* root = ash::Shell::GetRootWindowForNewWindows();
  auto* parent = root->GetChildById(ash::kShellWindowId_MenuContainer);
  eye_dropper_ =
      std::make_unique<eye_dropper::EyeDropperView>(parent, root, this);
}

void EyeDropperAsh::ColorSelected(SkColor color) {
  listener_->ColorSelected(color);
  eye_dropper_.reset();
}

void EyeDropperAsh::ColorSelectionCanceled() {
  listener_->ColorSelectionCanceled();
  eye_dropper_.reset();
}

void EyeDropperAsh::OnDisconnect() {
  eye_dropper_.reset();
}

}  // namespace crosapi

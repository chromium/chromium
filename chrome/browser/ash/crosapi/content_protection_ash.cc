// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/content_protection_ash.h"

#include "ash/display/output_protection_delegate.h"
#include "chrome/browser/ash/crosapi/window_util.h"

namespace crosapi {

ContentProtectionAsh::ContentProtectionAsh() = default;
ContentProtectionAsh::~ContentProtectionAsh() {
  for (auto& pair : output_protection_delegates_) {
    pair.first->RemoveObserver(this);
  }
}

void ContentProtectionAsh::BindReceiver(
    mojo::PendingReceiver<mojom::ContentProtection> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

ash::OutputProtectionDelegate*
ContentProtectionAsh::FindOrCreateOutputProtectionDelegate(
    aura::Window* window) {
  auto it = output_protection_delegates_.find(window);
  if (it != output_protection_delegates_.end())
    return it->second.get();

  auto delegate = std::make_unique<ash::OutputProtectionDelegate>(window);
  ash::OutputProtectionDelegate* ptr = delegate.get();
  output_protection_delegates_[window] = std::move(delegate);
  window->AddObserver(this);
  return ptr;
}

void ContentProtectionAsh::EnableWindowProtection(
    const std::string& window_id,
    uint32_t desired_protection_mask,
    EnableWindowProtectionCallback callback) {
  aura::Window* window = crosapi::GetShellSurfaceWindow(window_id);
  if (!window) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  ash::OutputProtectionDelegate* delegate =
      FindOrCreateOutputProtectionDelegate(window);
  delegate->SetProtection(desired_protection_mask, std::move(callback));
}

void ContentProtectionAsh::QueryWindowStatus(
    const std::string& window_id,
    QueryWindowStatusCallback callback) {
  aura::Window* window = crosapi::GetShellSurfaceWindow(window_id);
  if (!window) {
    ExecuteWindowStatusCallback(std::move(callback), /*success=*/false,
                                /*link_mask=*/0,
                                /*protection_mask=*/0);
    return;
  }

  ash::OutputProtectionDelegate* delegate =
      FindOrCreateOutputProtectionDelegate(window);
  delegate->QueryStatus(
      base::BindOnce(&ContentProtectionAsh::ExecuteWindowStatusCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ContentProtectionAsh::OnWindowDestroyed(aura::Window* window) {
  output_protection_delegates_.erase(window);
  // No need to call window->RemoveObserver() since Window* handles that before
  // calling this method.
}

void ContentProtectionAsh::ExecuteWindowStatusCallback(
    QueryWindowStatusCallback callback,
    bool success,
    uint32_t link_mask,
    uint32_t protection_mask) {
  if (success) {
    mojom::ContentProtectionWindowStatusPtr status =
        mojom::ContentProtectionWindowStatus::New();
    status->link_mask = link_mask;
    status->protection_mask = protection_mask;
    std::move(callback).Run(std::move(status));
  } else {
    std::move(callback).Run(nullptr);
  }
}

}  // namespace crosapi

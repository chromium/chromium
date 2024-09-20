// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/desk_template_ash.h"

#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"

namespace crosapi {

DeskTemplateAsh::Call::Call(
    uint32_t serial,
    const std::string& window_unique_id,
    uint32_t remote_count,
    base::OnceCallback<void(crosapi::mojom::DeskTemplateStatePtr)> callback)
    : serial(serial),
      window_unique_id(window_unique_id),
      remote_count(remote_count),
      callback(std::move(callback)) {}

DeskTemplateAsh::Call::~Call() = default;

DeskTemplateAsh::DeskTemplateAsh() = default;
DeskTemplateAsh::~DeskTemplateAsh() = default;

void DeskTemplateAsh::BindReceiver(
    mojo::PendingReceiver<mojom::DeskTemplate> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void DeskTemplateAsh::GetBrowserInformation(
    const std::string& window_unique_id,
    base::OnceCallback<void(crosapi::mojom::DeskTemplateStatePtr)> callback) {
  const auto current_serial = serial_++;
  calls_.emplace_back(current_serial, window_unique_id, remotes_.size(),
                      std::move(callback));
  for (const auto& remote : remotes_) {
    remote->GetBrowserInformation(
        current_serial, window_unique_id,
        base::BindOnce(&DeskTemplateAsh::OnGetBrowserInformationFromRemote,
                       weak_factory_.GetWeakPtr()));
  }
}

void DeskTemplateAsh::CreateBrowserWithRestoredData(
    const gfx::Rect& bounds,
    const ui::mojom::WindowShowState show_state,
    crosapi::mojom::DeskTemplateStatePtr additional_state) {
  if (remotes_.empty())
    return;

  remotes_.begin()->get()->CreateBrowserWithRestoredData(
      bounds, show_state, std::move(additional_state));
}

void DeskTemplateAsh::GetFaviconImage(
    const GURL& url,
    uint64_t lacros_profile_id,
    base::OnceCallback<void(const gfx::ImageSkia&)> callback) {
  if (remotes_.empty()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  remotes_.begin()->get()->GetFaviconImage(url, lacros_profile_id,
                                           std::move(callback));
}

void DeskTemplateAsh::AddDeskTemplateClient(
    mojo::PendingRemote<mojom::DeskTemplateClient> client) {
  remotes_.Add(mojo::Remote<mojom::DeskTemplateClient>(std::move(client)));
}

void DeskTemplateAsh::OnGetBrowserInformationFromRemote(
    uint32_t serial,
    const std::string& window_unique_id,
    mojom::DeskTemplateStatePtr state) {
  auto call_it = calls_.begin();
  while (call_it != calls_.end()) {
    if (call_it->window_unique_id == window_unique_id &&
        call_it->serial == serial) {
      break;
    }
    ++call_it;
  }
  if (call_it == calls_.end()) {
    DCHECK(state.is_null());
    return;
  }
  Call& call = *call_it;
  DCHECK(call.remote_count > 0);
  --call.remote_count;
  if (call.remote_count == 0 || !state.is_null()) {
    std::move(call.callback).Run(std::move(state));
    calls_.erase(call_it);
  }
}

}  // namespace crosapi

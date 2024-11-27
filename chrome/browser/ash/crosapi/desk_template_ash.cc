// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/desk_template_ash.h"

namespace crosapi {

DeskTemplateAsh::DeskTemplateAsh() = default;
DeskTemplateAsh::~DeskTemplateAsh() = default;

void DeskTemplateAsh::BindReceiver(
    mojo::PendingReceiver<mojom::DeskTemplate> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
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

}  // namespace crosapi

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/media_app_ash.h"

namespace crosapi {

MediaAppAsh::MediaAppAsh() {}
MediaAppAsh::~MediaAppAsh() = default;

void MediaAppAsh::BindRemote(mojo::PendingRemote<mojom::MediaApp> remote) {
  if (media_app_.is_bound() && media_app_.is_connected()) {
    return;
  }

  media_app_.reset();
  media_app_.Bind(std::move(remote));
}

void MediaAppAsh::SubmitForm(const GURL& url,
                             const std::vector<int8_t>& payload,
                             const std::string& header,
                             mojom::MediaApp::SubmitFormCallback callback) {
  media_app_->SubmitForm(url, payload, header, std::move(callback));
}

}  // namespace crosapi

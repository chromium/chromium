// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/arc_ash.h"

#include <utility>

#include "base/notreached.h"

namespace crosapi {

ArcAsh::ArcAsh() = default;
ArcAsh::~ArcAsh() = default;

void ArcAsh::BindReceiver(mojo::PendingReceiver<mojom::Arc> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ArcAsh::AddObserver(mojo::PendingRemote<mojom::ArcObserver> observer) {
  mojo::Remote<mojom::ArcObserver> remote(std::move(observer));
  observers_.Add(std::move(remote));
}

void ArcAsh::RequestActivityIcons(
    std::vector<mojom::ActivityNamePtr> activities,
    mojom::ScaleFactor scale_factor,
    RequestActivityIconsCallback) {
  NOTIMPLEMENTED();
}

void ArcAsh::RequestUrlHandlerList(const std::string& url,
                                   RequestUrlHandlerListCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace crosapi

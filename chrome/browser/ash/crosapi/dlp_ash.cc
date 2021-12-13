// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/dlp_ash.h"

namespace crosapi {

DlpAsh::DlpAsh() = default;

DlpAsh::~DlpAsh() = default;

void DlpAsh::BindReceiver(mojo::PendingReceiver<mojom::Dlp> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void DlpAsh::DlpRestrictionsUpdated(const std::string& window_id,
                                    mojom::DlpRestrictionSetPtr restrictions) {
  // TODO(crbug.com/1260467): Pass information to DlpContentManager in Ash.
}

}  // namespace crosapi

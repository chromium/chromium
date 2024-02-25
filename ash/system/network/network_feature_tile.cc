// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_feature_tile.h"

#include "base/check.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

NetworkFeatureTile::NetworkFeatureTile(Delegate* delegate,
                                       base::RepeatingCallback<void()> callback)
    : FeatureTile(callback), delegate_(delegate) {
  DCHECK(delegate);
}

NetworkFeatureTile::~NetworkFeatureTile() = default;

void NetworkFeatureTile::OnThemeChanged() {
  FeatureTile::OnThemeChanged();
  delegate_->OnFeatureTileThemeChanged();
}

BEGIN_METADATA(NetworkFeatureTile)
END_METADATA

}  // namespace ash

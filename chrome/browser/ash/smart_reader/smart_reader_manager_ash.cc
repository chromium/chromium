// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smart_reader/smart_reader_manager_ash.h"

namespace ash {

SmartReaderManagerAsh::SmartReaderManagerAsh() = default;

SmartReaderManagerAsh::~SmartReaderManagerAsh() = default;

void SmartReaderManagerAsh::BindRemote(
    mojo::PendingRemote<crosapi::mojom::SmartReaderClient> remote) {
  smart_reader_client_ =
      mojo::Remote<crosapi::mojom::SmartReaderClient>(std::move(remote));
}
}  // namespace ash

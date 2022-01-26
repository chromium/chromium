// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_cast_receiver_instance.h"

#include <utility>

namespace arc {

FakeCastReceiverInstance::FakeCastReceiverInstance() = default;
FakeCastReceiverInstance::~FakeCastReceiverInstance() = default;

void FakeCastReceiverInstance::GetName(GetNameCallback callback) {
  std::move(callback).Run(mojom::CastReceiverInstance::Result::SUCCESS,
                          "cast name");
}

void FakeCastReceiverInstance::SetEnabled(bool enabled,
                                          SetEnabledCallback callback) {
  last_enabled_ = enabled;
  std::move(callback).Run(mojom::CastReceiverInstance::Result::SUCCESS);
}

void FakeCastReceiverInstance::SetName(const std::string& name,
                                       SetNameCallback callback) {
  last_name_ = name;
  std::move(callback).Run(mojom::CastReceiverInstance::Result::SUCCESS);
}

}  // namespace arc

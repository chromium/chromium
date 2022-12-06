// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_tray_effects_delegate.h"

namespace ash {

VcEffectsDelegate::VcEffectsDelegate() = default;

VcEffectsDelegate::~VcEffectsDelegate() = default;

void VcEffectsDelegate::AddEffect(const VcHostedEffect* effect) {
  effects_.push_back(effect);
}

}  // namespace ash
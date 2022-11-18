// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/test_system_sounds_delegate.h"

namespace ash {

TestSystemSoundsDelegate::TestSystemSoundsDelegate() = default;

TestSystemSoundsDelegate::~TestSystemSoundsDelegate() = default;

void TestSystemSoundsDelegate::Init() {}

void TestSystemSoundsDelegate::Play(Sound sound_key) {
  last_played_sound_keys_.push_back(sound_key);
}

}  // namespace ash

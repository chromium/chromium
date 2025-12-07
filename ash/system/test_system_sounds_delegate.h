// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TEST_SYSTEM_SOUNDS_DELEGATE_H_
#define ASH_SYSTEM_TEST_SYSTEM_SOUNDS_DELEGATE_H_

#include <vector>

#include "ash/ash_export.h"
#include "chromeos/ash/components/audio/sounds.h"
#include "chromeos/ash/components/audio/system_sounds_delegate.h"

namespace ash {

class ASH_EXPORT TestSystemSoundsDelegate : public SystemSoundsDelegate {
 public:
  using SoundKeys = std::vector<Sound>;

  TestSystemSoundsDelegate();
  TestSystemSoundsDelegate(TestSystemSoundsDelegate&) = delete;
  TestSystemSoundsDelegate& operator=(TestSystemSoundsDelegate&) = delete;
  ~TestSystemSoundsDelegate() override;

  const SoundKeys& last_played_sound_keys() const {
    return last_played_sound_keys_;
  }

  // Resets the `last_played_sound_keys_` to be empty.
  void reset() { last_played_sound_keys_.clear(); }
  bool empty() const { return last_played_sound_keys_.empty(); }

  // ash::SystemSoundsDelegate:
  void Init() override;
  void Play(Sound sound_key) override;

 private:
  SoundKeys last_played_sound_keys_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TEST_SYSTEM_SOUNDS_DELEGATE_H_

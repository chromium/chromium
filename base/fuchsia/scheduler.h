// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_SCHEDULER_H_
#define BASE_FUCHSIA_SCHEDULER_H_

#include "base/strings/string_piece.h"
#include "base/time/time.h"

namespace base {

// Scheduling interval to use for realtime audio threads.
// TODO(crbug.com/1224707): Add scheduling period to Thread::Options and remove
// this constants.
constexpr TimeDelta kAudioSchedulingPeriod = TimeDelta::FromMilliseconds(10);

// Reserve 10% or one CPU core for audio threads.
// TODO(crbug.com/1174811): A different value may need to be used for WebAudio
// threads (see media::FuchsiaAudioOutputDevice). A higher capacity may need to
// be allocated in that case.
constexpr float kAudioSchedulingCapacity = 0.1;

// Scheduling interval to use for display threads.
// TODO(crbug.com/1224707): Add scheduling period to Thread::Options and remove
// this constants.
constexpr TimeDelta kDisplaySchedulingPeriod = TimeDelta::FromSeconds(1) / 60;

// Reserve 50% of one CPU core for display threads.
// TODO(crbug.com/1181421): Currently DISPLAY priority is not enabled for any
// thread on Fuchsia. The value below will need to be fine-tuned when it's
// enabled.
const float kDisplaySchedulingCapacity = 0.5;

}  // namespace base

#endif  // BASE_FUCHSIA_SCHEDULER_H_

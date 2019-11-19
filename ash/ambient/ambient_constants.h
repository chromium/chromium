// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_CONSTANTS_H_
#define ASH_AMBIENT_AMBIENT_CONSTANTS_H_

namespace ash {

// Duration of the slide show animation. Also used as |delay| in posted task to
// download images.
constexpr base::TimeDelta kAnimationDuration =
    base::TimeDelta::FromMilliseconds(250);

// PhotoModel has a local in memory cache of downloaded photos. This is the
// desired max number of photos stored in cache. If this is an even number,
// the max number could be one larger.
constexpr int kImageBufferLength = 5;

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_CONSTANTS_H_

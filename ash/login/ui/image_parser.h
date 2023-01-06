// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_IMAGE_PARSER_H_
#define ASH_LOGIN_UI_IMAGE_PARSER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/login/ui/animation_frame.h"
#include "base/functional/callback_forward.h"

namespace ash {

using OnDecoded = base::OnceCallback<void(AnimationFrames animation)>;

// Do an async animation decode; |on_decoded| is run on the calling thread when
// the decode has finished.
//
// This uses blink's image decoder and supports APNG.
ASH_EXPORT void DecodeAnimation(const std::vector<uint8_t>& image_data,
                                OnDecoded on_decoded);

}  // namespace ash

#endif  // ASH_LOGIN_UI_IMAGE_PARSER_H_
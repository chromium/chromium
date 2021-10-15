// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LOTTIE_TEST_DATA_H_
#define CC_TEST_LOTTIE_TEST_DATA_H_

#include "base/strings/string_piece.h"

namespace cc {

// A skottie animation with solid green color for the first 2.5 seconds and then
// a solid blue color for the next 2.5 seconds.
constexpr base::StringPiece kLottieDataWithoutAssets1 =
    R"({
      "v" : "4.12.0",
      "fr": 30,
      "w" : 400,
      "h" : 200,
      "ip": 0,
      "op": 150,
      "assets": [],
      "layers": [
        {
          "ty": 1,
          "sw": 400,
          "sh": 200,
          "sc": "#00ff00",
          "ip": 0,
          "op": 75
        },
        {
          "ty": 1,
          "sw": 400,
          "sh": 200,
          "sc": "#0000ff",
          "ip": 76,
          "op": 150
        }
      ]
    })";

// A skottie animation with solid green color for the first second and then
// a solid blue color for the next second.
constexpr base::StringPiece kLottieDataWithoutAssets2 =
    R"({
      "v" : "4.12.0",
      "fr": 30,
      "w" : 400,
      "h" : 200,
      "ip": 0,
      "op": 60,
      "assets": [],
      "layers": [
        {
          "ty": 1,
          "sw": 400,
          "sh": 200,
          "sc": "#00ff00",
          "ip": 0,
          "op": 30
        },
        {
          "ty": 1,
          "sw": 400,
          "sh": 200,
          "sc": "#0000ff",
          "ip": 31,
          "op": 60
        }
      ]
    })";

}  // namespace cc

#endif  // CC_TEST_LOTTIE_TEST_DATA_H_

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

// A test animation generated in Adobe AE that displays image_0 for a second,
// then image_1 for another second.
constexpr int kLottieDataWith2AssetsWidth = 690;
constexpr int kLottieDataWith2AssetsHeight = 455;
constexpr base::StringPiece kLottieDataWith2Assets =
    R"({
  "v": "5.7.4",
  "fr": 60,
  "ip": 0,
  "op": 119,
  "w": 690,
  "h": 455,
  "nm": "2 photo - Cut",
  "ddd": 0,
  "assets": [
    {
      "id": "image_0",
      "w": 690,
      "h": 455,
      "u": "images/",
      "p": "img_0.jpg",
      "e": 0
    },
    {
      "id": "image_1",
      "w": 690,
      "h": 455,
      "u": "images/",
      "p": "img_1.jpg",
      "e": 0
    }
  ],
  "layers": [
    {
      "ddd": 0,
      "ind": 1,
      "ty": 2,
      "nm": "CODERED_B1_landscape_P2a_690x455.jpg.img.jpg",
      "cl": "jpg img jpg",
      "refId": "image_0",
      "sr": 1,
      "ks": {
        "o": {
          "a": 0,
          "k": 100,
          "ix": 11
        },
        "r": {
          "a": 0,
          "k": 0,
          "ix": 10
        },
        "p": {
          "a": 0,
          "k": [
            345,
            227.5,
            0
          ],
          "ix": 2,
          "l": 2
        },
        "a": {
          "a": 0,
          "k": [
            345,
            227.5,
            0
          ],
          "ix": 1,
          "l": 2
        },
        "s": {
          "a": 0,
          "k": [
            100,
            100,
            100
          ],
          "ix": 6,
          "l": 2
        }
      },
      "ao": 0,
      "ip": 0,
      "op": 61,
      "st": 0,
      "bm": 0
    },
    {
      "ddd": 0,
      "ind": 2,
      "ty": 2,
      "nm": "CODERED_B1_landscape_P2b_690x455.jpg.img.jpg",
      "cl": "jpg img jpg",
      "refId": "image_1",
      "sr": 1,
      "ks": {
        "o": {
          "a": 0,
          "k": 100,
          "ix": 11
        },
        "r": {
          "a": 0,
          "k": 0,
          "ix": 10
        },
        "p": {
          "a": 0,
          "k": [
            345,
            227.5,
            0
          ],
          "ix": 2,
          "l": 2
        },
        "a": {
          "a": 0,
          "k": [
            345,
            227.5,
            0
          ],
          "ix": 1,
          "l": 2
        },
        "s": {
          "a": 0,
          "k": [
            100,
            100,
            100
          ],
          "ix": 6,
          "l": 2
        }
      },
      "ao": 0,
      "ip": 61,
      "op": 120,
      "st": 61,
      "bm": 0
    },
    {
      "ddd": 0,
      "ind": 3,
      "ty": 1,
      "nm": "White Solid 2",
      "sr": 1,
      "ks": {
        "o": {
          "a": 0,
          "k": 100,
          "ix": 11
        },
        "r": {
          "a": 0,
          "k": 0,
          "ix": 10
        },
        "p": {
          "a": 0,
          "k": [
            345,
            227.5,
            0
          ],
          "ix": 2,
          "l": 2
        },
        "a": {
          "a": 0,
          "k": [
            345,
            227.5,
            0
          ],
          "ix": 1,
          "l": 2
        },
        "s": {
          "a": 0,
          "k": [
            100,
            100,
            100
          ],
          "ix": 6,
          "l": 2
        }
      },
      "ao": 0,
      "sw": 690,
      "sh": 455,
      "sc": "#ffffff",
      "ip": 0,
      "op": 1769,
      "st": 0,
      "bm": 0
    }
  ],
  "markers": []
})";

}  // namespace cc

#endif  // CC_TEST_LOTTIE_TEST_DATA_H_

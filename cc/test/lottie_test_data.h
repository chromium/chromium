// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LOTTIE_TEST_DATA_H_
#define CC_TEST_LOTTIE_TEST_DATA_H_

#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cc {

// A skottie animation with solid green color for the first 2.5 seconds and then
// a solid blue color for the next 2.5 seconds.
constexpr std::string_view kLottieDataWithoutAssets1 =
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
          "nm": "Color 1",
          "ty": 1,
          "sw": 400,
          "sh": 200,
          "sc": "#00ff00",
          "ip": 0,
          "op": 75
        },
        {
          "nm": "Color 2",
          "ty": 1,
          "sw": 400,
          "sh": 200,
          "sc": "#0000ff",
          "ip": 76,
          "op": 150
        }
      ]
    })";

constexpr std::string_view kLottieDataWithoutAssets1Color1Node = "Color 1";
constexpr std::string_view kLottieDataWithoutAssets1Color2Node = "Color 2";
constexpr SkColor kLottieDataWithoutAssets1Color1 = SK_ColorGREEN;
constexpr SkColor kLottieDataWithoutAssets1Color2 = SK_ColorBLUE;

// Returns an animation with the same structure as |kLottieDataWithoutAssets1|
// except with color node names specified by the caller.
std::string CreateCustomLottieDataWith2ColorNodes(
    std::string_view color_node_1,
    std::string_view color_node_2);

// A skottie animation with solid green color for the first second and then
// a solid blue color for the next second.
constexpr std::string_view kLottieDataWithoutAssets2 =
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
constexpr base::TimeDelta kLottieDataWith2AssetsDuration = base::Seconds(2);
constexpr std::string_view kLottieDataWith2Assets =
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

// Returns an animation with the same structure as |kLottieDataWith2Assets|
// except with asset ids specified by the caller.
std::string CreateCustomLottieDataWith2Assets(
    std::string_view custom_asset_id_0,
    std::string_view custom_asset_id_1);

// cc/test/data/lottie/animation_with_2_text_nodes.json
constexpr base::FilePath::CharType kLottieDataWith2TextFileName[] =
    FILE_PATH_LITERAL("animation_with_2_text_nodes.json");
constexpr std::string_view kLottieDataWith2TextNode1 = "text_node_1";
constexpr std::string_view kLottieDataWith2TextNode1Text = "test_text_1";
constexpr std::string_view kLottieDataWith2TextNode2 = "text_node_2";
constexpr std::string_view kLottieDataWith2TextNode2Text = "test_text_2";
constexpr gfx::RectF kLottieDataWith2TextNode1Box =
    gfx::RectF(10, 20, 100, 200);
constexpr gfx::RectF kLottieDataWith2TextNode2Box =
    gfx::RectF(20, 40, 200, 400);
constexpr gfx::PointF kLottieDataWith2TextNode1Position = gfx::PointF(100, 200);
constexpr gfx::PointF kLottieDataWith2TextNode2Position = gfx::PointF(200, 400);

// Returns an animation with the same structure as
// |kLottieDataWith2TextFileName| except with text node names specified by the
// caller.
std::string CreateCustomLottieDataWith2TextNodes(
    std::string_view custom_text_node_name_0,
    std::string_view custom_text_node_name_1);

constexpr std::string_view kLottieDataWith2MarkersMarker1 = "TestMarker1";
constexpr std::string_view kLottieDataWith2MarkersMarker2 = "TestMarker2";
constexpr float kLottieDataWith2MarkersMarker1Time = .33f;
constexpr float kLottieDataWith2MarkersMarker2Time = .67f;
// Duration: 6 seconds. Marker 1 is at 2 seconds, and marker 2 is at 4 seconds.
constexpr std::string_view kLottieDataWith2Markers =
    R"({
      "v" : "4.12.0",
      "fr": 30,
      "w" : 400,
      "h" : 200,
      "ip": 0,
      "op": 180,
      "assets": [],
      "layers": [
        {
          "ty": 1,
          "sw": 400,
          "sh": 200,
          "sc": "#00ff00",
          "ip": 0,
          "op": 180
        }
      ],
      "markers": [
        {
          "tm": 60,
          "cm": "TestMarker1",
          "dr": 0
        },
        {
          "tm": 120,
          "cm": "TestMarker2",
          "dr": 0
        }
      ]
    })";

}  // namespace cc

#endif  // CC_TEST_LOTTIE_TEST_DATA_H_

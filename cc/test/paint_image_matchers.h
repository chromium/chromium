// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PAINT_IMAGE_MATCHERS_H_
#define CC_TEST_PAINT_IMAGE_MATCHERS_H_

#include "cc/paint/paint_image.h"
#include "cc/paint/skottie_frame_data.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cc {

// Checks `arg.IsSameForTesting(image)`.
// Mainly used in container matchers:
//    std::vector<PaintImage> images;
//    PaintImage image;
//    EXPECT_THAT(images, Contains(ImageIsSame(image)));
MATCHER_P(ImageIsSame, image, "") {
  return arg.IsSameForTesting(image);
}

// Checks the values of `arg` (a 2-tuple) are the same image. Used by
// ImagesAreSame().
MATCHER(TwoImagesAreSame, "") {
  return std::get<0>(arg).IsSameForTesting(std::get<1>(arg));
}

// Checks two containers contain the same images (PaintImage or DrawImage) in
// the same order.
// Example use:
//    std::vector<PaintImage> images1, images2;
//    EXPECT_THAT(images1, ImagesAreSame(images2));
template <typename Images>
auto ImagesAreSame(const Images& images) {
  return ::testing::Pointwise(TwoImagesAreSame(), images);
}
template <typename Image>
auto ImagesAreSame(std::initializer_list<Image> list) {
  return ::testing::Pointwise(TwoImagesAreSame(), list);
}

MATCHER_P2(SkottieImageIs, resource_id, frame_data, "") {
  return arg.first == HashSkottieResourceId(resource_id) &&
         arg.second.image.IsSameForTesting(frame_data.image) &&
         arg.second.quality == frame_data.quality;
}

MATCHER_P3(SkottieImageIs, resource_id, image, quality, "") {
  return arg.first == HashSkottieResourceId(resource_id) &&
         arg.second.image.IsSameForTesting(image) &&
         arg.second.quality == quality;
}

MATCHER_P3(SkottieTextIs, resource_id, text, box, "") {
  return arg.first == HashSkottieResourceId(resource_id) &&
         arg.second == SkottieTextPropertyValue(text, box);
}

}  // namespace cc

#endif  // CC_TEST_PAINT_IMAGE_MATCHERS_H_

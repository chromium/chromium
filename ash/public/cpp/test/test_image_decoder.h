// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_IMAGE_DECODER_H_
#define ASH_PUBLIC_CPP_TEST_TEST_IMAGE_DECODER_H_

#include <memory>

#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/data_decoder/public/cpp/service_provider.h"
#include "services/data_decoder/public/mojom/data_decoder_service.mojom.h"
#include "third_party/skia/include/core/SkColor.h"

class SkBitmap;

namespace ash {

// Instantiating `TestImageDecoder` anywhere in your test will make the
// decoding functions in image_util.h use a stubbed decoder implementation. All
// calls to decode images and animations after instantiation (and before
// destruction) will instead call the given `ImageCallback` or
// `AnimationCallback`, respectively.
//
// It also has the same benefits as
// `InProcessImageDecoder`: it does not rely on blink or go out of process.
//
// Important: If calling `image_util::Decode*File()`, pass in an existing file,
// or the logic will return early. The contents don't matter, as long as it
// exists and isn't empty.
class TestImageDecoder : public data_decoder::ServiceProvider {
 public:
  using AnimationCallback = base::RepeatingCallback<
      std::vector<data_decoder::mojom::AnimationFramePtr>()>;
  using ImageCallback = base::RepeatingCallback<SkBitmap()>;

  TestImageDecoder(AnimationCallback test_frames_callback,
                   ImageCallback test_image_callback);
  TestImageDecoder(const TestImageDecoder&) = delete;
  TestImageDecoder& operator=(const TestImageDecoder&) = delete;
  ~TestImageDecoder() override;

  // data_decoder::ServiceProvider:
  void BindDataDecoderService(
      mojo::PendingReceiver<data_decoder::mojom::DataDecoderService> receiver)
      override;

 private:
  class DataDecoderServiceImpl;

  const std::unique_ptr<DataDecoderServiceImpl> service_;
  mojo::ReceiverSet<data_decoder::mojom::DataDecoderService> receivers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_IMAGE_DECODER_H_

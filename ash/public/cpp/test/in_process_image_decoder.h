// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_IN_PROCESS_IMAGE_DECODER_H_
#define ASH_PUBLIC_CPP_TEST_IN_PROCESS_IMAGE_DECODER_H_

#include <memory>

#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/data_decoder/public/cpp/service_provider.h"
#include "services/data_decoder/public/mojom/data_decoder_service.mojom.h"

namespace ash {

// Serves as the underlying decoder implementation in tests for any code that
// uses the decoding functions in image_util.h. To use, simply instantiate an
// InProcessImageDecoder somewhere in the test harness before a call to
// ash::DecodeImage*() is made, and all of the rest is taken care of.
//
// This is exactly the same as data_decoder::test::InProcessDataDecoder in that
// it is an in-process implementation of the data decoding mojo service, which
// avoids the complexity of a multi-process environment in tests. The difference
// is that:
// 1) It only supports image decoding; it was specifically written to address
//    the drawback mentioned in 2).
// 2) The image decoding implementation does not depend on blink like
//    data_decoder::test::InProcessDataDecoder does. This is problematic within
//    ash/ specifically because blink is an undesirable dependency.
class InProcessImageDecoder : public data_decoder::ServiceProvider {
 public:
  InProcessImageDecoder();
  InProcessImageDecoder(const InProcessImageDecoder&) = delete;
  InProcessImageDecoder& operator=(const InProcessImageDecoder&) = delete;
  ~InProcessImageDecoder() override;

 private:
  class DataDecoderServiceImpl;

  // ServiceProvider implementation:
  void BindDataDecoderService(
      mojo::PendingReceiver<data_decoder::mojom::DataDecoderService> receiver)
      override;

  const std::unique_ptr<DataDecoderServiceImpl> service_;
  mojo::ReceiverSet<data_decoder::mojom::DataDecoderService> receivers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_IN_PROCESS_IMAGE_DECODER_H_

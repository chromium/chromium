// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMAGE_DECODER_IMAGE_DECODER_H_
#define CHROME_BROWSER_IMAGE_DECODER_IMAGE_DECODER_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"

namespace data_decoder {
class DataDecoder;
}  // namespace data_decoder

namespace gfx {
class Size;
}  // namespace gfx

class SkBitmap;

// This is a helper class for decoding images safely in a sandboxed service. To
// use this, call ImageDecoder::Start(...) or
// ImageDecoder::StartWithOptions(...) on any thread.
//
// ImageRequest::OnImageDecoded or ImageRequest::OnDecodeImageFailed is posted
// back to the |task_runner_| associated with the ImageRequest.
//
// The Cancel() method runs on whichever thread called it.
//
// TODO(rockot): Use of this class should be replaced with direct image_decoder
// client library usage.
class ImageDecoder {
 public:
  // ImageRequest objects needs to be created and destroyed on the same
  // SequencedTaskRunner.
  class ImageRequest {
   public:
    // Called when image is decoded.
    virtual void OnImageDecoded(const SkBitmap& decoded_image) = 0;

    // Called when decoding image failed. ImageRequest can do some cleanup in
    // this handler.
    virtual void OnDecodeImageFailed() {}

    base::SequencedTaskRunner* task_runner() const {
      return task_runner_.get();
    }

    data_decoder::DataDecoder* data_decoder() { return data_decoder_; }

   protected:
    // Creates an ImageRequest that runs on the thread which created it.
    ImageRequest();
    // Explicitly pass in |task_runner| if the current thread is part of a
    // thread pool.
    explicit ImageRequest(
        const scoped_refptr<base::SequencedTaskRunner>& task_runner);
    // Explicitly pass in |data_decoder| if there's a specific decoder that
    // should be used; otherwise, an isolated decoder will created and used.
    explicit ImageRequest(data_decoder::DataDecoder* data_decoder);
    virtual ~ImageRequest();

   private:
    // The thread to post OnImageDecoded() or OnDecodeImageFailed() once the
    // the image has been decoded.
    const scoped_refptr<base::SequencedTaskRunner> task_runner_;

    // If null, will use a new decoder via DecodeImageIsolated() instead.
    data_decoder::DataDecoder* const data_decoder_ = nullptr;

    SEQUENCE_CHECKER(sequence_checker_);
  };

  enum ImageCodec {
    DEFAULT_CODEC = 0,  // Uses WebKit image decoding (via WebImage).
#if defined(OS_CHROMEOS)
    ROBUST_PNG_CODEC,   // Restrict decoding to robust PNG codec.
#endif                  // defined(OS_CHROMEOS)
  };

  static ImageDecoder* GetInstance();

  // Calls StartWithOptions() with ImageCodec::DEFAULT_CODEC and
  // shrink_to_fit = false.
  static void Start(ImageRequest* image_request,
                    std::vector<uint8_t> image_data);
  // Deprecated. Use std::vector<uint8_t> version to avoid an extra copy.
  static void Start(ImageRequest* image_request, const std::string& image_data);

  // Starts asynchronous image decoding. Once finished, the callback will be
  // posted back to image_request's |task_runner_|.
  // For images with multiple frames (e.g. ico files), a frame with a size as
  // close as possible to |desired_image_frame_size| is chosen (tries to take
  // one in larger size if there's no precise match). Passing gfx::Size() as
  // |desired_image_frame_size| is also supported and will result in chosing the
  // smallest available size.
  static void StartWithOptions(ImageRequest* image_request,
                               std::vector<uint8_t> image_data,
                               ImageCodec image_codec,
                               bool shrink_to_fit,
                               const gfx::Size& desired_image_frame_size);
  // Deprecated. Use std::vector<uint8_t> version to avoid an extra copy.
  static void StartWithOptions(ImageRequest* image_request,
                               const std::string& image_data,
                               ImageCodec image_codec,
                               bool shrink_to_fit);

  // Removes all instances of |image_request| from |image_request_id_map_|,
  // ensuring callbacks are not made to the image_request after it is destroyed.
  static void Cancel(ImageRequest* image_request);

 private:
  using RequestMap = std::map<int, ImageRequest*>;

  ImageDecoder();
  ~ImageDecoder() = delete;

  void StartWithOptionsImpl(ImageRequest* image_request,
                            std::vector<uint8_t> image_data,
                            ImageCodec image_codec,
                            bool shrink_to_fit,
                            const gfx::Size& desired_image_frame_size);

  void CancelImpl(ImageRequest* image_request);

  // IPC message handlers.
  void OnDecodeImageSucceeded(const SkBitmap& decoded_image, int request_id);
  void OnDecodeImageFailed(int request_id);

  // id to use for the next Start() request that comes in.
  int image_request_id_counter_;

  // Map of request id's to ImageRequests.
  RequestMap image_request_id_map_;

  // Protects |image_request_id_map_| and |image_request_id_counter_|.
  base::Lock map_lock_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecoder);
};

#endif  // CHROME_BROWSER_IMAGE_DECODER_IMAGE_DECODER_H_

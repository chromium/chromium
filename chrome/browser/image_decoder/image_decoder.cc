// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/image_decoder/image_decoder.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

const int64_t kMaxImageSizeInBytes =
    static_cast<int64_t>(IPC::Channel::kMaximumMessageSize);

// Note that this is always called on the thread which initiated the
// corresponding data_decoder::DecodeImage request.
void OnDecodeImageDone(
    base::OnceCallback<void(int)> fail_callback,
    base::OnceCallback<void(const SkBitmap&, int)> success_callback,
    int request_id,
    const SkBitmap& image) {
  if (!image.isNull() && !image.empty())
    std::move(success_callback).Run(image, request_id);
  else
    std::move(fail_callback).Run(request_id);
}

void RunDecodeCallbackOnTaskRunner(
    data_decoder::DecodeImageCallback callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const SkBitmap& image) {
  task_runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback), image));
}

template <typename ImageDataType>
void DecodeImage(ImageDataType image_data,
                 data_decoder::mojom::ImageCodec codec,
                 bool shrink_to_fit,
                 const gfx::Size& desired_image_frame_size,
                 data_decoder::DecodeImageCallback callback,
                 scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
                 data_decoder::DataDecoder* data_decoder) {
  base::span<const uint8_t> image_data_span(
      base::as_bytes(base::make_span(image_data)));

  if (data_decoder) {
    data_decoder::DecodeImage(
        data_decoder, image_data_span, codec, shrink_to_fit,
        kMaxImageSizeInBytes, desired_image_frame_size,
        base::BindOnce(&RunDecodeCallbackOnTaskRunner, std::move(callback),
                       std::move(callback_task_runner)));
  } else {
    data_decoder::DecodeImageIsolated(
        image_data_span, codec, shrink_to_fit, kMaxImageSizeInBytes,
        desired_image_frame_size,
        base::BindOnce(&RunDecodeCallbackOnTaskRunner, std::move(callback),
                       std::move(callback_task_runner)));
  }
}

}  // namespace

ImageDecoder::ImageRequest::ImageRequest()
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ImageDecoder::ImageRequest::ImageRequest(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ImageDecoder::ImageRequest::ImageRequest(
    data_decoder::DataDecoder* data_decoder)
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      data_decoder_(data_decoder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ImageDecoder::ImageRequest::~ImageRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ImageDecoder::Cancel(this);
}

// static
ImageDecoder* ImageDecoder::GetInstance() {
  static base::NoDestructor<ImageDecoder> image_decoder;
  return image_decoder.get();
}

// static
template <typename ImageDataType>
void ImageDecoder::Start(ImageRequest* image_request,
                         ImageDataType image_data) {
  StartWithOptions(image_request, std::move(image_data));
}

template void ImageDecoder::Start(ImageRequest*, std::vector<uint8_t>);
template void ImageDecoder::Start(ImageRequest*, std::string);

// static
template <typename ImageDataType>
void ImageDecoder::StartWithOptions(ImageRequest* image_request,
                                    ImageDataType image_data,
                                    ImageCodec image_codec,
                                    bool shrink_to_fit,
                                    const gfx::Size& desired_image_frame_size) {
  ImageDecoder::GetInstance()->StartWithOptionsImpl(
      image_request, std::move(image_data), image_codec, shrink_to_fit,
      desired_image_frame_size);
}

template void ImageDecoder::StartWithOptions(ImageRequest*,
                                             std::vector<uint8_t>,
                                             ImageCodec,
                                             bool,
                                             const gfx::Size&);
template void ImageDecoder::StartWithOptions(ImageRequest*,
                                             std::string,
                                             ImageCodec,
                                             bool,
                                             const gfx::Size&);

ImageDecoder::ImageDecoder() : image_request_id_counter_(0) {}

template <typename ImageDataType>
void ImageDecoder::StartWithOptionsImpl(
    ImageRequest* image_request,
    ImageDataType image_data,
    ImageCodec image_codec,
    bool shrink_to_fit,
    const gfx::Size& desired_image_frame_size) {
  DCHECK(image_request);
  DCHECK(image_request->task_runner());

  int request_id;
  {
    base::AutoLock lock(map_lock_);
    request_id = image_request_id_counter_++;
    image_request_id_map_.insert(std::make_pair(request_id, image_request));
  }

  data_decoder::mojom::ImageCodec codec =
      data_decoder::mojom::ImageCodec::kDefault;
#if BUILDFLAG(IS_CHROMEOS)
  if (image_codec == PNG_CODEC)
    codec = data_decoder::mojom::ImageCodec::kPng;
#endif  // BUILDFLAG(IS_CHROMEOS)

  auto callback =
      base::BindOnce(&OnDecodeImageDone,
                     base::BindOnce(&ImageDecoder::OnDecodeImageFailed,
                                    base::Unretained(this)),
                     base::BindOnce(&ImageDecoder::OnDecodeImageSucceeded,
                                    base::Unretained(this)),
                     request_id);

  DecodeImage<ImageDataType>(std::move(image_data), codec, shrink_to_fit,
                             desired_image_frame_size, std::move(callback),
                             image_request->task_runner(),
                             image_request->data_decoder());
}

template void ImageDecoder::StartWithOptionsImpl(ImageRequest*,
                                                 std::vector<uint8_t>,
                                                 ImageCodec,
                                                 bool,
                                                 const gfx::Size&);
template void ImageDecoder::StartWithOptionsImpl(ImageRequest*,
                                                 std::string,
                                                 ImageCodec,
                                                 bool,
                                                 const gfx::Size&);

// static
void ImageDecoder::Cancel(ImageRequest* image_request) {
  DCHECK(image_request);
  ImageDecoder::GetInstance()->CancelImpl(image_request);
}

void ImageDecoder::CancelImpl(ImageRequest* image_request) {
  base::AutoLock lock(map_lock_);
  for (auto it = image_request_id_map_.begin();
       it != image_request_id_map_.end();) {
    if (it->second == image_request) {
      image_request_id_map_.erase(it++);
    } else {
      ++it;
    }
  }
}

void ImageDecoder::OnDecodeImageSucceeded(const SkBitmap& decoded_image,
                                          int request_id) {
  ImageRequest* image_request;
  {
    base::AutoLock lock(map_lock_);
    auto it = image_request_id_map_.find(request_id);
    if (it == image_request_id_map_.end())
      return;
    image_request = it->second;
    image_request_id_map_.erase(it);
  }

  DCHECK(image_request->task_runner()->RunsTasksInCurrentSequence());
  image_request->OnImageDecoded(decoded_image);
}

void ImageDecoder::OnDecodeImageFailed(int request_id) {
  ImageRequest* image_request;
  {
    base::AutoLock lock(map_lock_);
    auto it = image_request_id_map_.find(request_id);
    if (it == image_request_id_map_.end())
      return;
    image_request = it->second;
    image_request_id_map_.erase(it);
  }

  DCHECK(image_request->task_runner()->RunsTasksInCurrentSequence());
  image_request->OnDecodeImageFailed();
}

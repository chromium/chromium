// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/image_decoder/image_decoder.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_channel.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

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
    data_decoder::mojom::ImageDecoder::DecodeImageCallback callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const SkBitmap& image) {
  task_runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback), image));
}

void DecodeImage(
    std::vector<uint8_t> image_data,
    data_decoder::mojom::ImageCodec codec,
    bool shrink_to_fit,
    const gfx::Size& desired_image_frame_size,
    data_decoder::mojom::ImageDecoder::DecodeImageCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    data_decoder::DataDecoder* data_decoder) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (data_decoder) {
    data_decoder::DecodeImage(
        data_decoder, image_data, codec, shrink_to_fit, kMaxImageSizeInBytes,
        desired_image_frame_size,
        base::BindOnce(&RunDecodeCallbackOnTaskRunner, std::move(callback),
                       std::move(callback_task_runner)));
  } else {
    data_decoder::DecodeImageIsolated(
        image_data, codec, shrink_to_fit, kMaxImageSizeInBytes,
        desired_image_frame_size,
        base::BindOnce(&RunDecodeCallbackOnTaskRunner, std::move(callback),
                       std::move(callback_task_runner)));
  }
}

}  // namespace

ImageDecoder::ImageRequest::ImageRequest()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ImageDecoder::ImageRequest::ImageRequest(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ImageDecoder::ImageRequest::ImageRequest(
    data_decoder::DataDecoder* data_decoder)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      data_decoder_(data_decoder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ImageDecoder::ImageRequest::~ImageRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ImageDecoder::Cancel(this);
}

// static
ImageDecoder* ImageDecoder::GetInstance() {
  static auto* image_decoder = new ImageDecoder();
  return image_decoder;
}

// static
void ImageDecoder::Start(ImageRequest* image_request,
                         std::vector<uint8_t> image_data) {
  StartWithOptions(image_request, std::move(image_data), DEFAULT_CODEC, false,
                   gfx::Size());
}

// static
void ImageDecoder::Start(ImageRequest* image_request,
                         const std::string& image_data) {
  Start(image_request,
        std::vector<uint8_t>(image_data.begin(), image_data.end()));
}

// static
void ImageDecoder::StartWithOptions(ImageRequest* image_request,
                                    std::vector<uint8_t> image_data,
                                    ImageCodec image_codec,
                                    bool shrink_to_fit,
                                    const gfx::Size& desired_image_frame_size) {
  ImageDecoder::GetInstance()->StartWithOptionsImpl(
      image_request, std::move(image_data), image_codec, shrink_to_fit,
      desired_image_frame_size);
}

// static
void ImageDecoder::StartWithOptions(ImageRequest* image_request,
                                    const std::string& image_data,
                                    ImageCodec image_codec,
                                    bool shrink_to_fit) {
  StartWithOptions(image_request,
                   std::vector<uint8_t>(image_data.begin(), image_data.end()),
                   image_codec, shrink_to_fit, gfx::Size());
}

ImageDecoder::ImageDecoder() : image_request_id_counter_(0) {}

void ImageDecoder::StartWithOptionsImpl(
    ImageRequest* image_request,
    std::vector<uint8_t> image_data,
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
      data_decoder::mojom::ImageCodec::DEFAULT;
#if defined(OS_CHROMEOS)
  if (image_codec == ROBUST_PNG_CODEC)
    codec = data_decoder::mojom::ImageCodec::ROBUST_PNG;
#endif  // defined(OS_CHROMEOS)

  auto callback =
      base::BindOnce(&OnDecodeImageDone,
                     base::BindOnce(&ImageDecoder::OnDecodeImageFailed,
                                    base::Unretained(this)),
                     base::BindOnce(&ImageDecoder::OnDecodeImageSucceeded,
                                    base::Unretained(this)),
                     request_id);

  // NOTE: There exist ImageDecoder consumers which implicitly rely on this
  // operation happening on a thread which always has a ThreadTaskRunnerHandle.
  // We arbitrarily use the IO thread here to match details of the legacy
  // implementation.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&DecodeImage, std::move(image_data), codec, shrink_to_fit,
                     desired_image_frame_size, std::move(callback),
                     base::WrapRefCounted(image_request->task_runner()),
                     image_request->data_decoder()));
}

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

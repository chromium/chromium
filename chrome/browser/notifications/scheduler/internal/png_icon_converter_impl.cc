// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/png_icon_converter_impl.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/notifications/scheduler/internal/icon_entry.h"
#include "ui/gfx/codec/png_codec.h"

namespace notifications {
namespace {

std::unique_ptr<EncodeResult> ConvertIconToStringInternal(
    std::vector<SkBitmap> images) {
  base::AssertLongCPUWorkAllowed();
  std::vector<std::string> encoded_data;
  for (size_t i = 0; i < images.size(); i++) {
    std::vector<unsigned char> image_data;
    bool success = gfx::PNGCodec::EncodeBGRASkBitmap(
        std::move(images[i]), false /*discard_transparency*/, &image_data);
    if (!success)
      return std::make_unique<EncodeResult>(false, std::vector<std::string>());
    std::string encoded(image_data.begin(), image_data.end());
    encoded_data.emplace_back(std::move(encoded));
  }
  return std::make_unique<EncodeResult>(
      true, std::vector<std::string>(std::move(encoded_data)));
}

std::unique_ptr<DecodeResult> ConvertStringToIconInternal(
    std::vector<std::string> encoded_data) {
  base::AssertLongCPUWorkAllowed();
  std::vector<SkBitmap> icons;
  for (size_t i = 0; i < encoded_data.size(); i++) {
    SkBitmap image;
    bool success = gfx::PNGCodec::Decode(reinterpret_cast<const unsigned char*>(
                                             std::move(encoded_data[i]).data()),
                                         encoded_data[i].length(), &image);
    if (!success)
      return std::make_unique<DecodeResult>(false, std::vector<SkBitmap>());

    icons.emplace_back(std::move(image));
  }
  return std::make_unique<DecodeResult>(
      true, std::vector<SkBitmap>(std::move(icons)));
}

}  // namespace

PngIconConverterImpl::PngIconConverterImpl() = default;

PngIconConverterImpl::~PngIconConverterImpl() = default;

void PngIconConverterImpl::ConvertIconToString(std::vector<SkBitmap> images,
                                               EncodeCallback callback) {
  DCHECK(callback);
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ConvertIconToStringInternal, std::move(images)),
      std::move(callback));
}

void PngIconConverterImpl::ConvertStringToIcon(
    std::vector<std::string> encoded_data,
    DecodeCallback callback) {
  DCHECK(callback);
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ConvertStringToIconInternal, std::move(encoded_data)),
      std::move(callback));
}

}  // namespace notifications

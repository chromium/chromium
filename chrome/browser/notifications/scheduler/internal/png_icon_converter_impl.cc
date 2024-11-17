// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/png_icon_converter_impl.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/notifications/scheduler/internal/icon_entry.h"
#include "ui/gfx/codec/png_codec.h"

namespace notifications {
namespace {

std::unique_ptr<EncodeResult> ConvertIconToStringInternal(
    const std::vector<SkBitmap>& images) {
  base::AssertLongCPUWorkAllowed();
  std::vector<std::string> encoded_data;
  for (const auto& image : images) {
    std::optional<std::vector<uint8_t>> image_data =
        gfx::PNGCodec::EncodeBGRASkBitmap(image,
                                          /*discard_transparency=*/false);
    if (!image_data) {
      return std::make_unique<EncodeResult>(false, std::vector<std::string>());
    }
    encoded_data.emplace_back(base::as_string_view(image_data.value()));
  }
  return std::make_unique<EncodeResult>(
      true, std::vector<std::string>(std::move(encoded_data)));
}

std::unique_ptr<DecodeResult> ConvertStringToIconInternal(
    const std::vector<std::string>& encoded_data) {
  base::AssertLongCPUWorkAllowed();
  std::vector<SkBitmap> icons;
  for (const auto& data : encoded_data) {
    SkBitmap image = gfx::PNGCodec::Decode(base::as_byte_span(data));
    if (image.isNull()) {
      return std::make_unique<DecodeResult>(false, std::vector<SkBitmap>());
    }

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
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ConvertIconToStringInternal, std::move(images)),
      std::move(callback));
}

void PngIconConverterImpl::ConvertStringToIcon(
    std::vector<std::string> encoded_data,
    DecodeCallback callback) {
  DCHECK(callback);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ConvertStringToIconInternal, std::move(encoded_data)),
      std::move(callback));
}

}  // namespace notifications
